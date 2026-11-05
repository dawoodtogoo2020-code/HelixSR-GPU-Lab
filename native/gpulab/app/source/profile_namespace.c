#include "profile_namespace.h"
#include "profile_store.h"
#include <string.h>

static uint32_t rotate(uint32_t x,unsigned n)
{ return (x>>n)|(x<<(32u-n)); }

/* Bounded SHA256 used only for the fixed canonical identity below. */
static void identity_digest(const unsigned char input[276],unsigned char digest[32])
{
    static const uint32_t constants[64]={
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
        0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
        0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
        0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
        0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
        0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
        0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
        0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
        0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t state[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                       0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    unsigned char padded[320]={0};
    size_t offset,j,extent=sizeof padded;
    uint64_t bits=UINT64_C(276)*8u;
    memcpy(padded,input,276);
    padded[276]=0x80;
    for(j=0;j<8;j++) padded[extent-1-j]=(unsigned char)(bits>>(j*8));
    for(offset=0;offset<extent;offset+=64) {
        uint32_t words[64],a,b,c,d,e,f,g,h;
        for(j=0;j<16;j++) {
            const unsigned char *p=padded+offset+j*4;
            words[j]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                     ((uint32_t)p[2]<<8)|(uint32_t)p[3];
        }
        for(j=16;j<64;j++) {
            uint32_t s0=rotate(words[j-15],7)^rotate(words[j-15],18)^(words[j-15]>>3);
            uint32_t s1=rotate(words[j-2],17)^rotate(words[j-2],19)^(words[j-2]>>10);
            words[j]=words[j-16]+s0+words[j-7]+s1;
        }
        a=state[0]; b=state[1]; c=state[2]; d=state[3];
        e=state[4]; f=state[5]; g=state[6]; h=state[7];
        for(j=0;j<64;j++) {
            uint32_t upper=rotate(e,6)^rotate(e,11)^rotate(e,25);
            uint32_t lower=rotate(a,2)^rotate(a,13)^rotate(a,22);
            uint32_t choose=(e&f)^(~e&g),majority=(a&b)^(a&c)^(b&c);
            uint32_t first=h+upper+choose+constants[j]+words[j];
            uint32_t second=lower+majority;
            h=g; g=f; f=e; e=d+first; d=c; c=b; b=a; a=first+second;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    for(j=0;j<32;j++) digest[j]=(unsigned char)(state[j/4]>>(24u-8u*(j%4)));
}

int pfa_settings_namespace(const ap_identity *identity,char out[65])
{
    static const unsigned char domain[]="HelixSR-settings-v1";
    static const char hex[]="0123456789abcdef";
    unsigned char input[sizeof domain-1+257],digest[32];
    size_t j;
    _Static_assert(sizeof input==276,"identity namespace encoding extent");
    if (!out) return 0;
    out[0]=0;
    if (!ap_identity_valid(identity)) return 0;
    memcpy(input,domain,sizeof domain-1);
    ps_encode_identity(input+sizeof domain-1,identity);
    identity_digest(input,digest);
    for(j=0;j<32;j++) {
        out[j*2]=hex[digest[j]>>4]; out[j*2+1]=hex[digest[j]&15];
    }
    out[64]=0;
    return 1;
}
