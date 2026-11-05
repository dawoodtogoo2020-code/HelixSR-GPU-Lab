#include "scan_engine.h"
#include <string.h>

/* Signatures, case-sensitive. Each is also searched as UTF-16LE (Unreal keeps
 * console variables and many names as wide strings). */
static const struct { const char *text; unsigned char tech; } signatures[]={
    {"FFXFSR3",SE_FSR3},{"ffxFsr3",SE_FSR3},{"FfxFsr3",SE_FSR3},{"ffx_fsr3",SE_FSR3},{"FSR3Upscaler",SE_FSR3},
    {"fsr3upscaler",SE_FSR3},{"FidelityFX Super Resolution 3",SE_FSR3},{"r.FidelityFX.FSR3",SE_FSR3},
    {"FFXFSR2",SE_FSR2},{"ffxFsr2",SE_FSR2},{"FfxFsr2",SE_FSR2},{"ffx_fsr2",SE_FSR2},
    {"FidelityFX Super Resolution 2",SE_FSR2},{"r.FidelityFX.FSR2",SE_FSR2},{"FSR2",SE_FSR2},
    {"ffxFsr4",SE_FSR4},{"FfxFsr4",SE_FSR4},{"FidelityFX Super Resolution 4",SE_FSR4},{"FSR4",SE_FSR4},
    {"ffxFsr1",SE_FSR1},{"FfxFsr1",SE_FSR1},{"FsrEasu",SE_FSR1},{"FsrRcas",SE_FSR1},
    {"FidelityFX Super Resolution 1",SE_FSR1},{"FSR1",SE_FSR1},
    {"PSSR",SE_PSSR},{"ScePssr",SE_PSSR},{"scePssr",SE_PSSR},{"sce::Pssr",SE_PSSR},
    {"TemporalSuperResolution",SE_TSR},{"r.TSR.",SE_TSR},{"TSRHistory",SE_TSR},
    {"r.TemporalAA.Upscaler",SE_TAAU},{"TemporalAAUpscaler",SE_TAAU},
    {"XeSS",SE_XESS},{"xess_",SE_XESS},{"DLSS",SE_DLSS},{"NVSDK_NGX",SE_DLSS},
    {"Checkerboard",SE_CHECKERBOARD},{"CheckerBoard",SE_CHECKERBOARD},
    {"/Script/Engine",SE_UNREAL},{"UnrealEngine",SE_UNREAL},{"++UE5",SE_UNREAL},{"++UE4",SE_UNREAL},
    {"Northlight",SE_NORTHLIGHT},{"UnityEngine",SE_UNITY},{"UnityPlayer",SE_UNITY},
    {"Frostbite",SE_FROSTBITE}};
enum { SIGNATURES=sizeof signatures/sizeof *signatures, PATTERNS=SIGNATURES*2, MAX_PATTERN=64 };
static struct { unsigned char bytes[MAX_PATTERN]; unsigned char length,tech; } patterns[PATTERNS];
static unsigned char lead[65536];
static size_t longest;
static int built;

static void build(void)
{
    for(unsigned i=0;i<SIGNATURES;++i) {
        size_t n=strlen(signatures[i].text);
        for(unsigned wide=0;wide<2;++wide) {
            unsigned p=i*2+wide;
            patterns[p].tech=signatures[i].tech;
            patterns[p].length=(unsigned char)(wide?n*2:n);
            for(size_t j=0;j<n;++j) {
                if(wide){patterns[p].bytes[j*2]=(unsigned char)signatures[i].text[j];patterns[p].bytes[j*2+1]=0;}
                else patterns[p].bytes[j]=(unsigned char)signatures[i].text[j];
            }
            lead[patterns[p].bytes[0]<<8|patterns[p].bytes[1]]=1;
            if(patterns[p].length>longest)longest=patterns[p].length;
        }
    }
    built=1;
}
/* Counts matches starting at positions [0, starts) of data[0, bytes) that end
 * after position min_end. */
static void match(se_state *s,const unsigned char *data,size_t bytes,size_t starts,size_t min_end)
{
    for(size_t i=0;i<starts&&i+1<bytes;++i) {
        if(!lead[data[i]<<8|data[i+1]])continue;
        for(unsigned p=0;p<PATTERNS;++p) {
            size_t n=patterns[p].length;
            if(i+n<=bytes&&i+n>min_end&&data[i]==patterns[p].bytes[0]&&!memcmp(data+i,patterns[p].bytes,n))
                if(s->hits[patterns[p].tech]<0xffffffffu)++s->hits[patterns[p].tech];
        }
    }
}
static uint32_t rotr(uint32_t x,unsigned n) { return (x>>n)|(x<<(32-n)); }
static void block(uint32_t h[8],const unsigned char *p)
{
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];
    for(unsigned i=0;i<16;i++)w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|p[4*i+3];
    for(unsigned i=16;i<64;i++) {
        uint32_t a=w[i-15],b=w[i-2];
        w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
    for(unsigned i=0;i<64;i++) {
        uint32_t t=z+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
        z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
void se_init(se_state *s)
{
    static const uint32_t start[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    if(!built)build();
    memset(s,0,sizeof *s);memcpy(s->h,start,sizeof start);
}
void se_update(se_state *s,const unsigned char *data,size_t bytes)
{
    if(!bytes)return;
    /* Matches that start in the previous chunk's tail and end in this one. */
    if(s->tail_used) {
        unsigned char joined[2*MAX_PATTERN];
        size_t head=bytes<longest-1?bytes:longest-1;
        memcpy(joined,s->tail,s->tail_used);memcpy(joined+s->tail_used,data,head);
        match(s,joined,s->tail_used+head,s->tail_used,s->tail_used);
    }
    match(s,data,bytes,bytes,0);
    size_t keep=longest-1;
    if(bytes>=keep){memcpy(s->tail,data+bytes-keep,keep);s->tail_used=keep;}
    else {
        size_t total=s->tail_used+bytes,drop=total>keep?total-keep:0;
        memmove(s->tail,s->tail+drop,s->tail_used-drop);memcpy(s->tail+s->tail_used-drop,data,bytes);
        s->tail_used=total-drop;
    }
    s->bytes+=bytes;
    while(bytes) {
        size_t take=64-s->block_used;if(take>bytes)take=bytes;
        memcpy(s->block+s->block_used,data,take);s->block_used+=take;data+=take;bytes-=take;
        if(s->block_used==64){block(s->h,s->block);s->block_used=0;}
    }
}
void se_finish(se_state *s,char hex[65])
{
    uint64_t bits=s->bytes*8;unsigned char pad[128]={0};
    size_t n=s->block_used;memcpy(pad,s->block,n);pad[n]=0x80;
    size_t total=n<56?64:128;
    for(unsigned i=0;i<8;i++)pad[total-1-i]=(unsigned char)(bits>>(8*i));
    block(s->h,pad);if(total==128)block(s->h,pad+64);
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;i++) {
        unsigned char v=(unsigned char)(s->h[i/4]>>(24-8*(i%4)));
        hex[i*2]=digits[v>>4];hex[i*2+1]=digits[v&15];
    }
    hex[64]='\0';
}
const char *se_tech_name(unsigned tech)
{
    static const char *names[SE_TECH_COUNT]={"FSR 1","FSR 2","FSR 3","FSR 4","PSSR","Unreal TSR","TAAU","XeSS","DLSS",
        "Checkerboard","Unreal Engine","Northlight","Unity","Frostbite"};
    return tech<SE_TECH_COUNT?names[tech]:"Unknown";
}
const char *se_primary(const se_state *s)
{
    /* Newest FSR first; Unreal links TSR into every build, so it only counts
     * when no FSR plugin is present. PSSR needs a PS5 Pro. */
    static const unsigned order[]={SE_FSR4,SE_FSR3,SE_FSR2,SE_TSR,SE_TAAU,SE_XESS,SE_PSSR,SE_FSR1,SE_CHECKERBOARD};
    for(unsigned i=0;i<sizeof order/sizeof *order;++i) {
        unsigned t=order[i];
        if(t==SE_TSR&&!s->hits[SE_UNREAL])continue;
        if(s->hits[t]>=2)return se_tech_name(t);
    }
    return "Unknown";
}
const char *se_engine(const se_state *s)
{
    unsigned best=SE_TECH_COUNT;
    for(unsigned t=SE_UNREAL;t<SE_TECH_COUNT;++t)
        if(s->hits[t]>=2&&(best==SE_TECH_COUNT||s->hits[t]>s->hits[best]))best=t;
    return best<SE_TECH_COUNT?se_tech_name(best):"Unknown";
}
