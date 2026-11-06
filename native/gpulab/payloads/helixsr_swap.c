/* HelixSR on/off by moving files (see helixsr_swap.h). Portable C: the PS5 payload and the host tests build it as is. */
#define _GNU_SOURCE
#include "helixsr_swap.h"
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- packages ------------------------------------------------------------------------------------------------------
 * CONTROL Resonant (PPSA34547 01.300.300, backported), overlay-files package. On: the backport folder
 * /data/homebrew/backports/PPSA34547 is HelixSR r18 (u08: dual-mode executable d0f3c69e/5fd8484a, blob 458f289f, toc 485f21fa, selector 5c8e0bcf; Performance 1505x847 -> 2560x1440 and Cinematic 2258x1270 -> 3840x2160, other layouts keep stock FSR2. Before: r17 (u05; table updated from r15: blob 6e48bf72, toc 4f8a5167, selector 0c088335; r15 was: executable SELF d90e12dc / installed bc90c820 with the stock FSR2 and
 * FSR3 passes skipped, unchanged from r13; blob 316a7c8d = r13 + per-pixel sanitised motion/colour inputs, the five unused
 * F1/F2 argument writers removed, InputStatus mask 0x1f; toc 2de59ecc; selector 0c088335 = no diagnostic barcode on screen,
 * camera cuts accepted). Off: the four overlay files move into the package; fakelib2 stays in the backport root. */
static const hs_file control_files[]={
    {"eboot.bin","eboot.bin",{NULL,NULL},
     {"d0f3c69ed019cd1ab06a04038d1a48239c8cfeeac5c5f7da5e716bd1e1c9be9e","5fd8484a84ee9df60d327aedbfa7c22b84efca47df1601e17a17bd3aa96ce196"}},
    {"data_pack2/generic/base-generic-000.rmdblob","base-generic-000.rmdblob",{NULL,NULL},
     {"458f289f0f19ff281a689320982abcf8a5691e77e97b1e41d0a754c5eba44014",NULL}},
    {"data_pack2/prospero/base-generic.rmdtoc","base-generic.rmdtoc",{NULL,NULL},
     {"485f21faeda6e57a1bb763f47a39e6bb3048edad0f96d2bd848d010679e8e6ed",NULL}},
    {"shaders/build/ps5/postprocess.binrfx","postprocess.binrfx",{NULL,NULL},
     {"5c8e0bcf369b04c1723ce1916b91de6fd37c275d0b63f55b5b93bebdb13aca1e",NULL}},
};
static const hs_file aw2_files[]={
    {"eboot.bin","eboot.bin",{NULL,NULL},
     {"5c6a93a6cec2255b1c412ca74dd9593891ead0d1b4f31c40c1d077b2a07e6521","a4e890645a310ae5aa5818d5aab01d2a10ba191892ab2113c5e190572dd32d18","31a3ffd1b582b6d363da7432c41962e1b48ba59fcaa14befaae916d38cb66c35"}},
};
static const hs_file saros_files[]={
 {"eboot.bin","eboot.bin",{NULL,NULL},{"68917b032b806babb10d3acc162afcd5a91b36fab5392281c38bc92e0553c44d",NULL}}
};
static const hs_package packages[]={
 {"PPSA07631","01.000.000","saros-native-v1","SAROS 1440p reconstruction to 4K; shadows and fog on",1,saros_files,1},
    {"PPSA02572","01.200.007","aw2-t12","HelixSR t12 Performance",1,aw2_files,1},
    {"PPSA34547","01.300.300","control-r18-20261029","HelixSR r18, Performance + Cinematic, faster encoder (off = stock FSR2)",
     sizeof control_files/sizeof *control_files,control_files,2},
};
const hs_package *hs_find(const char *title,const char *version)
{
    for(unsigned i=0;i<sizeof packages/sizeof *packages;++i)
        if(!strcmp(packages[i].title,title)&&!strcmp(packages[i].version,version))return &packages[i];
    return NULL;
}

/* ---- SHA-256 ----------------------------------------------------------------------------------------------------- */
typedef struct { uint32_t h[8]; uint64_t n; unsigned char b[64]; size_t u; } sha_ctx;
static const uint32_t K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void sha_block(sha_ctx *c,const unsigned char *p)
{
    uint32_t w[64],a,b,d,e,f,g,h,cc;
    for(int i=0;i<16;i++)w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|p[4*i+3];
    for(int i=16;i<64;i++){uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3),s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for(int i=0;i<64;i++){
        uint32_t t1=h+(ROR(e,6)^ROR(e,11)^ROR(e,25))+((e&f)^(~e&g))+K[i]+w[i];
        uint32_t t2=(ROR(a,2)^ROR(a,13)^ROR(a,22))+((a&b)^(a&cc)^(b&cc));
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}
static void sha_init(sha_ctx *c)
{
    static const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(c->h,iv,sizeof iv);c->n=0;c->u=0;
}
static void sha_update(sha_ctx *c,const unsigned char *p,size_t n)
{
    c->n+=n;
    if(c->u){size_t t=64-c->u;if(t>n)t=n;memcpy(c->b+c->u,p,t);c->u+=t;p+=t;n-=t;if(c->u==64){sha_block(c,c->b);c->u=0;}}
    while(n>=64){sha_block(c,p);p+=64;n-=64;}
    if(n){memcpy(c->b,p,n);c->u=n;}
}
static void sha_hex(sha_ctx *c,char out[65])
{
    uint64_t bits=c->n*8;unsigned char pad=0x80,z[64]={0},len[8];
    sha_update(c,&pad,1);
    sha_update(c,z,(c->u<=56?56-c->u:120-c->u));
    for(int i=0;i<8;i++)len[i]=(unsigned char)(bits>>(56-8*i));
    sha_update(c,len,8);
    for(int i=0;i<8;i++)snprintf(out+8*i,9,"%08x",c->h[i]);
}

/* ---- hash cache: "dev ino size sec nsec sha" per line ---------------------------------------------------------- */
#define CACHE_MAX 64
typedef struct { unsigned long long dev,ino,size; long long sec,nsec; char sha[65]; } cache_row;
typedef struct { cache_row rows[CACHE_MAX]; unsigned count; int dirty; } cache_t;
static void cache_load(cache_t *c,const char *path)
{
    memset(c,0,sizeof *c);if(!path)return;
    FILE *f=fopen(path,"r");if(!f)return;
    cache_row r;
    while(c->count<CACHE_MAX&&fscanf(f,"%llu %llu %llu %lld %lld %64s",&r.dev,&r.ino,&r.size,&r.sec,&r.nsec,r.sha)==6)
        if(strlen(r.sha)==64)c->rows[c->count++]=r;
    fclose(f);
}
static void cache_save(cache_t *c,const char *path)
{
    if(!path||!c->dirty)return;
    char tmp[600];if(snprintf(tmp,sizeof tmp,"%s.tmp",path)>=(int)sizeof tmp)return;
    FILE *f=fopen(tmp,"w");if(!f)return;
    for(unsigned i=0;i<c->count;i++)
        fprintf(f,"%llu %llu %llu %lld %lld %s\n",c->rows[i].dev,c->rows[i].ino,c->rows[i].size,c->rows[i].sec,c->rows[i].nsec,c->rows[i].sha);
    int ok=!fflush(f)&&!fsync(fileno(f));
    if(fclose(f))ok=0;
    if(!ok||rename(tmp,path))unlink(tmp);
}
static int hash_file(const char *path,const struct stat *st,cache_t *c,char out[65])
{
    for(unsigned i=0;i<c->count;i++){
        const cache_row *r=&c->rows[i];
        if(r->dev==(unsigned long long)st->st_dev&&r->ino==(unsigned long long)st->st_ino&&r->size==(unsigned long long)st->st_size&&
           r->sec==(long long)st->st_mtim.tv_sec&&r->nsec==(long long)st->st_mtim.tv_nsec){memcpy(out,r->sha,65);return 0;}
    }
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;
    enum { CHUNK=1u<<20 };
    unsigned char *buf=malloc(CHUNK);sha_ctx s;sha_init(&s);int rc=buf?0:-1;
    while(!rc){
        ssize_t n=read(fd,buf,CHUNK);
        if(n<0&&errno==EINTR)continue;
        if(n<0){rc=-1;break;}
        if(!n)break;
        sha_update(&s,buf,(size_t)n);
    }
    free(buf);close(fd);if(rc)return -1;
    sha_hex(&s,out);
    cache_row r={(unsigned long long)st->st_dev,(unsigned long long)st->st_ino,(unsigned long long)st->st_size,
                 (long long)st->st_mtim.tv_sec,(long long)st->st_mtim.tv_nsec,{0}};
    memcpy(r.sha,out,65);
    if(c->count==CACHE_MAX){memmove(c->rows,c->rows+1,(CACHE_MAX-1)*sizeof *c->rows);c->count--;}
    c->rows[c->count++]=r;c->dirty=1;
    return 0;
}

/* ---- classification and moves ---------------------------------------------------------------------------------- */
enum kind { K_NONE=0, K_ORIG, K_HELIX, K_OTHER, K_ERROR };
enum place { P_G=0, P_N, P_PO, P_PH, P_COUNT };
static const char *const place_name[P_COUNT]={"game","game.hlx-new","package/original","package/helix"};
typedef struct { char path[P_COUNT][512]; enum kind kind[P_COUNT]; char sha[P_COUNT][65]; } file_view;

static int in(const char *sha,const char *const accepted[3]) { for(int i=0;i<3;i++)if(accepted[i]&&!strcmp(sha,accepted[i]))return 1;return 0; }
static int view(const hs_file *f,const char *game_root,const char *package_root,cache_t *c,file_view *v)
{
    memset(v,0,sizeof *v);
    if(snprintf(v->path[P_G],512,"%s/%s",game_root,f->rel)>=512||snprintf(v->path[P_N],512,"%s/%s.hlx-new",game_root,f->rel)>=512||
       snprintf(v->path[P_PO],512,"%s/original/%s",package_root,f->base)>=512||snprintf(v->path[P_PH],512,"%s/helix/%s",package_root,f->base)>=512)return -1;
    for(int p=0;p<P_COUNT;p++){
        struct stat st;
        if(lstat(v->path[p],&st)){v->kind[p]=errno==ENOENT?K_NONE:K_ERROR;continue;}
        if(!S_ISREG(st.st_mode)||hash_file(v->path[p],&st,c,v->sha[p])){v->kind[p]=K_ERROR;continue;}
        v->kind[p]=in(v->sha[p],f->original)?K_ORIG:in(v->sha[p],f->helix)?K_HELIX:K_OTHER;
    }
    return 0;
}
static void fail(hs_result *r,const char *state,const char *fmt,const char *a,const char *b)
{
    r->ok=0;snprintf(r->state,sizeof r->state,"%s",state);snprintf(r->reason,sizeof r->reason,fmt,a,b);
}
/* Where each version is, or a refusal. Allowed: original in {G,N,PO}, HelixSR in {G,N,PH}, exactly one of each. */
static int locate(const hs_file *f,const file_view *v,int *orig,int *helix,hs_result *r)
{
    *orig=*helix=-1;
    for(int p=0;p<P_COUNT;p++){
        if(v->kind[p]==K_NONE)continue;
        if(v->kind[p]==K_ERROR){fail(r,p<=P_N?"unrecognized":"package_damaged","%s could not be read (%s).",f->base,place_name[p]);return -1;}
        if(v->kind[p]==K_OTHER){
            if(p<=P_N)fail(r,"unrecognized","%s: unexpected version (game update?). Nothing changed.%s",f->base,"");
            else fail(r,"package_damaged","The HelixSR copy of %s is damaged. Nothing changed.%s",f->base,"");
            return -1;
        }
        if(v->kind[p]==K_ORIG){if(*orig>=0||p==P_PH){fail(r,"package_damaged","%s: the original is duplicated or misplaced (%s).",f->base,place_name[p]);return -1;}*orig=p;}
        if(v->kind[p]==K_HELIX){if(*helix>=0||p==P_PO){fail(r,"package_damaged","%s: HelixSR file duplicated or misplaced (%s).",f->base,place_name[p]);return -1;}*helix=p;}
    }
    if(*orig<0){fail(r,"unrecognized","%s: the game's own file is missing. Nothing changed.%s",f->base,"");return -1;}
    if(*helix<0){fail(r,"package_missing","%s: HelixSR file missing; copy the package again.%s",f->base,"");return -1;}
    return 0;
}
static int (*swap_rename)(const char *,const char *)=rename;   /* the host test injects failures here */
int (*hs_gate)(void);
static int move(const file_view *v,int from,int to,hs_result *r,FILE *log)
{
    if(hs_gate&&hs_gate()){if(log)fprintf(log,"swap_move_refused gate from=%s\n",v->path[from]);errno=EBUSY;return -1;}
    int rc=swap_rename(v->path[from],v->path[to]);
    if(log)fprintf(log,"swap_move from=%s to=%s rc=%d errno=%d\n",v->path[from],v->path[to],rc,rc?errno:0);
    if(!rc)r->moves++;
    return rc;
}
/* Moves for one file; t = target place of the target version, o = where the other version is, slot = its package slot.
 * Every step leaves both versions at a recognised place, so a crash between steps is finished by the next run. */
static int solve(const file_view *v,int t,int o,int slot,hs_result *r,FILE *log)
{
    if(t==P_G&&o==slot)return 0;
    if(t==P_G&&o==P_N)return move(v,P_N,slot,r,log);
    if(t==P_N&&o==P_G)return move(v,P_G,slot,r,log)||move(v,P_N,P_G,r,log);
    if(t==P_N&&o==slot)return move(v,P_N,P_G,r,log);
    if(o==P_G)return move(v,t,P_N,r,log)||move(v,P_G,slot,r,log)||move(v,P_N,P_G,r,log);
    if(o==P_N)return move(v,P_N,slot,r,log)||move(v,t,P_G,r,log);
    return move(v,t,P_G,r,log);   /* t in its package slot, o in its slot, nothing at G or N */
}
static int dir_ok(const char *root,const char *sub)
{
    char p[512];struct stat st;
    return snprintf(p,sizeof p,"%s/%s",root,sub)<(int)sizeof p&&!lstat(p,&st)&&S_ISDIR(st.st_mode);
}
/* Reject symlinks and non-directory ancestors before examining either folder. */
static int safe_ancestors(const char *path) {
    char b[512]; struct stat st; size_t n=strlen(path);
    if(!n||n>=sizeof b||path[0]!='/')return 0;
    memcpy(b,path,n+1);
    for(size_t i=1;i<n;i++)if(b[i]=='/') {
        b[i]=0; int bad=lstat(b,&st)||!S_ISDIR(st.st_mode); b[i]='/'; if(bad)return 0;
    }
    return 1;
}
/* Injectable only by the host test; enumeration errors must never look like EOF. */
static struct dirent *(*swap_readdir)(DIR *)=readdir;
/* Exact whitelist, including directory nodes. Refuse every extra or symlink. */
static int folder_tree_mode(const hs_package *pkg,const char *base,const char *rel,int deps) {
    char path[512]; if(snprintf(path,sizeof path,"%s%s%s",base,*rel?"/":"",rel)>=(int)sizeof path)return 0;
    DIR *d=opendir(path); if(!d)return 0; struct dirent *e; int ok=1;
    while(ok) {
        errno=0;e=swap_readdir(d);
        if(!e){if(errno)ok=0;break;}
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        char child[512],full[512]; struct stat st; int exact=0,prefix=0;
        if(snprintf(child,sizeof child,"%s%s%s",rel,*rel?"/":"",e->d_name)>=(int)sizeof child||
           snprintf(full,sizeof full,"%s/%s",base,child)>=(int)sizeof full||lstat(full,&st)){ok=0;break;}
        if(deps&&!strcmp(child,"fakelib2")&&S_ISDIR(st.st_mode))continue;
        for(unsigned i=0;i<pkg->count;i++) {
            const char *want=pkg->files[i].rel; size_t z=strlen(child);
            if(!strcmp(want,child))exact=1;
            if(!strncmp(want,child,z)&&want[z]=='/')prefix=1;
        }
        if(S_ISREG(st.st_mode))ok=exact;
        else if(S_ISDIR(st.st_mode))ok=prefix&&folder_tree_mode(pkg,base,child,deps);
        else ok=0;
    }
    closedir(d); return ok;
}
static int folder_tree(const hs_package *pkg,const char *base,const char *rel) { return folder_tree_mode(pkg,base,rel,0); }
#ifndef AW2_SUPPORT_ROOT
#define AW2_SUPPORT_ROOT "/data/helixsr-aw2-t12"
#endif
static const hs_file aw2_support[]={
 {"copy.bin","copy.bin",{NULL,NULL},{"9813366beb23417c521a0e07ac6f8be29351ebd8438d68295e085143989d9b0b",NULL}},
 {"chain.bin","chain.bin",{NULL,NULL},{"712fa3e9f807a97d92e525f0c3f3f95442a5e5d5e00b1ecf287835ad659786d1",NULL}}
};
static const hs_file *support_files=aw2_support;
static int support_ok(void) {
    static cache_t fresh; memset(&fresh,0,sizeof fresh); char path[512],sha[65]; struct stat st;
    if(!safe_ancestors(AW2_SUPPORT_ROOT)||lstat(AW2_SUPPORT_ROOT,&st)||!S_ISDIR(st.st_mode))return 0;
    for(unsigned i=0;i<2;i++) {
        const hs_file *f=&support_files[i];
        if(snprintf(path,sizeof path,"%s/%s",AW2_SUPPORT_ROOT,f->rel)>=(int)sizeof path||lstat(path,&st)||!S_ISREG(st.st_mode)||hash_file(path,&st,&fresh,sha)||!in(sha,f->helix))return 0;
    }
    return 1;
}
#ifndef SAROS_SUPPORT_ROOT
#define SAROS_SUPPORT_ROOT "/data/helixsr-saros-full"
#endif
static const hs_file saros_support_default[]={
 {"tile.bin","tile.bin",{NULL,NULL},{"1be3a14f7f25ea298929b485ad0cdf5e17441cd5ea94cb85f22a4f8500c9eb8e",NULL}}
};
static const hs_file *saros_support_files=saros_support_default;
static off_t saros_support_bytes=2315936;
static int saros_support_ok(void) {
 static cache_t fresh;memset(&fresh,0,sizeof fresh);char path[512],sha[65];struct stat st;
 hs_package check={"PPSA07631","01.000.000","saros-native-v1","",1,saros_support_files,1};
 if(!safe_ancestors(SAROS_SUPPORT_ROOT)||lstat(SAROS_SUPPORT_ROOT,&st)||!S_ISDIR(st.st_mode)||!folder_tree(&check,SAROS_SUPPORT_ROOT,""))return 0;
 return snprintf(path,sizeof path,"%s/tile.bin",SAROS_SUPPORT_ROOT)<(int)sizeof path &&
  !lstat(path,&st)&&S_ISREG(st.st_mode)&&st.st_size==saros_support_bytes&&
  !hash_file(path,&st,&fresh,sha)&&in(sha,saros_support_files[0].helix);
}
/* Whole-folder packages: exactly one of game_root and package/stored/game holds the build, recognised by every listed
 * file's HelixSR hash; switching is one rename of the folder (behind the gate). Anything else changes nothing. */
static int run_folder(const hs_package *pkg,const char *game_root,const char *package_root,int target,const char *cache_path,
                      hs_result *r,FILE *log)
{
    char stored[512],p[512],sha[65];struct stat st;
    if(!safe_ancestors(game_root)||!safe_ancestors(package_root)||!dir_ok(package_root,"stored")||snprintf(stored,sizeof stored,"%s/stored/game",package_root)>=(int)sizeof stored)
        {fail(r,"package_missing","The HelixSR package is not on the console.%s%s","","");return -1;}
    int on=!lstat(game_root,&st),on_error=!on&&errno!=ENOENT,on_dir=on&&S_ISDIR(st.st_mode);
    int off=!lstat(stored,&st),off_error=!off&&errno!=ENOENT,off_dir=off&&S_ISDIR(st.st_mode);
    if(on_error||off_error){fail(r,"package_damaged","Cannot inspect folder; nothing changed.%s%s","","");return -1;}
    if((on&&!on_dir)||(off&&!off_dir)||(on&&off)){fail(r,"package_damaged","The game's backport folder and the package both hold files. Nothing changed.%s%s","","");return -1;}
    if(!on&&!off){fail(r,"package_missing","The HelixSR build is not on the console; copy the package again.%s%s","","");return -1;}
    if(!safe_ancestors(stored)){fail(r,"package_damaged","Unsafe folder path; nothing changed.%s%s","","");return -1;}
    const char *at=on?game_root:stored;
    if(!folder_tree(pkg,at,"")){fail(r,"package_damaged","Unexpected files or links in backport; nothing changed.%s%s","","");return -1;}
    if(target==HS_HELIX&&!strcmp(pkg->title,"PPSA02572")&&!support_ok()){
        fail(r,"package_damaged","AW2 support files missing or damaged; nothing changed.%s%s","","");return -1;
    }
    if(target==HS_HELIX&&!strcmp(pkg->title,"PPSA07631")&&!saros_support_ok()){
        fail(r,"package_damaged","SAROS model asset missing or damaged; nothing changed.%s%s","","");return -1;
    }
    static cache_t cache;memset(&cache,0,sizeof cache); (void)cache_path;
    int bad=0;
    for(unsigned i=0;i<pkg->count&&!bad;i++){
        const hs_file *f=&pkg->files[i];
        if(snprintf(p,sizeof p,"%s/%s",at,f->rel)>=(int)sizeof p||lstat(p,&st)||!S_ISREG(st.st_mode)||hash_file(p,&st,&cache,sha))
            {fail(r,on?"unrecognized":"package_damaged","%s is missing or unreadable in the HelixSR folder. Nothing changed.%s",f->base,"");bad=1;}
        else if(!in(sha,f->helix))
            {fail(r,on?"unrecognized":"package_damaged","%s: the backport folder is not this HelixSR build. Nothing changed.%s",f->base,"");bad=1;}
    }
    if(bad){if(log)fprintf(log,"swap_refused state=%s reason=%s\n",r->state,r->reason);return -1;}
    if(target<0){r->ok=1;snprintf(r->state,sizeof r->state,"%s",on?"helix":"original");
        snprintf(r->reason,sizeof r->reason,"%s",on?"HelixSR is on.":"HelixSR is off; the game runs its own files.");return 0;}
    const int want=target==HS_HELIX;
    if(want!=on){
        const char *from=on?game_root:stored,*to=on?stored:game_root;
        if(hs_gate&&hs_gate()){if(log)fprintf(log,"swap_move_refused gate from=%s\n",from);
            fail(r,on?"helix":"original","A game is running; nothing changed. Close it and launch again.%s%s","","");return -1;}
        int rc=swap_rename(from,to);
        if(log)fprintf(log,"swap_move from=%s to=%s rc=%d errno=%d\n",from,to,rc,rc?errno:0);
        if(rc){fail(r,on?"helix":"original","The HelixSR folder could not be moved; nothing changed.%s%s","","");return -1;}
        r->moves=1;
    }
    r->ok=1;snprintf(r->state,sizeof r->state,"%s",want?"helix":"original");
    snprintf(r->reason,sizeof r->reason,"%s",want?(r->moves?"HelixSR switched on.":"HelixSR was already on."):
             (r->moves?"HelixSR is off; the game runs its own files.":"HelixSR was already off."));
    if(log)fprintf(log,"swap_done state=%s moves=%u\n",r->state,r->moves);
    return 0;
}
typedef struct { off_t size; const char *sha; } hs_identity;
typedef struct { const char *name; hs_identity elf,self; } hs_dependency;
/* ftpsrv normally presents an extracted ELF. Native open() reads the stored SELF.
 * Each accepted representation is pinned to its own exact length and digest. */
static const hs_dependency control_dependencies_default[]={
 {"libSceAgc.sprx",
  {366600,"09899ec2963c3f82ef1bc8f5774cebe5ec4a09f7c0c2792912140ac022b55e1b"},
  {326617,"e1c8f6dc384e0a5927f863fc6fd698e3da603ed41de512ae3529ad06dea2e191"}},
 {"libSceAgcDriver.sprx",
  {174688,"5590ee3fe3c2f73200674e17938a6f3e3dece8826a0ecc7249dcba2183adcf07"},
  {122383,"d83edb3a0b57c4a2476d902e16da051db82ef36b81b791d6e8d2d998f083e27b"}},
 {"libScePsml.sprx",
  {1550320,"4bf75f27e24eaf03c43def7a678fb79b4c582c633cace1129ef0e11b2ed01ea5"},
  {1524286,"6632020e026d7cec0af89604de96e64f42cc4764af0a28de7ed20fa78f56a36d"}}
};
static const hs_dependency *control_dependencies=control_dependencies_default;
static int dependencies_ok(const char *root) {
    char dir[512],p[512],sha[65];struct stat st;static cache_t fresh;memset(&fresh,0,sizeof fresh);
    if(snprintf(dir,sizeof dir,"%s/fakelib2",root)>=(int)sizeof dir)return 0;
    if(lstat(dir,&st))return errno==ENOENT;
    if(!S_ISDIR(st.st_mode))return 0;
    DIR *d=opendir(dir);if(!d)return 0;struct dirent *e;unsigned seen=0;int ok=1;
    while(ok) {
        errno=0;e=swap_readdir(d);
        if(!e){if(errno)ok=0;break;}
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        unsigned i;for(i=0;i<3;i++)if(!strcmp(e->d_name,control_dependencies[i].name))break;
        if(i==3||(seen&(1u<<i))||snprintf(p,sizeof p,"%s/%s",dir,e->d_name)>=(int)sizeof p||
           lstat(p,&st)||!S_ISREG(st.st_mode)){ok=0;break;}
        const hs_dependency *dep=&control_dependencies[i];
        int elf=st.st_size==dep->elf.size,self=st.st_size==dep->self.size;
        if((!elf&&!self)||hash_file(p,&st,&fresh,sha)||
           !((elf&&!strcmp(sha,dep->elf.sha))||(self&&!strcmp(sha,dep->self.sha)))){ok=0;break;}
        seen|=1u<<i;
    }
    closedir(d);return ok&&seen==7;
}
/* Make destination directories only after preflight, checking the gate before every mkdir. */
static int overlay_dirs(const char *root,const char *rel,FILE *log) {
    char p[512];struct stat st;
    if(snprintf(p,sizeof p,"%s/%s",root,rel)>=(int)sizeof p)return -1;
    for(char *q=p+1;*q;q++)if(*q=='/') {
        *q=0;
        if(lstat(p,&st)) {
            if(errno!=ENOENT||(hs_gate&&hs_gate())||mkdir(p,0755)){*q='/';return -1;}
            if(log)fprintf(log,"swap_mkdir path=%s\n",p);
        } else if(!S_ISDIR(st.st_mode)){*q='/';return -1;}
        *q='/';
    }
    return 0;
}
/* Every overlay file has exactly one valid instance across the two trees. */
static int run_overlay(const hs_package *pkg,const char *game,const char *package,int target,hs_result *r,FILE *log) {
    char stored[512],sha[65];static char paths[8][2][512];struct stat st;int where[8];unsigned on=0;
    static cache_t fresh;memset(&fresh,0,sizeof fresh);
    if(pkg->count>8||snprintf(stored,sizeof stored,"%s/stored/game",package)>=(int)sizeof stored||
       !safe_ancestors(game)||!safe_ancestors(package)||lstat(package,&st)||!S_ISDIR(st.st_mode)||
       !dir_ok(package,"stored")||!safe_ancestors(stored)) {
        fail(r,"package_damaged","Unsafe or missing package path; nothing changed.%s%s","","");return -1;
    }
    const char *roots[2]={game,stored};
    for(int j=0;j<2;j++) {
        if(lstat(roots[j],&st)) {if(errno==ENOENT)continue;fail(r,"package_damaged","Cannot inspect overlay tree.%s%s","","");return -1;}
        if(!S_ISDIR(st.st_mode)||!folder_tree_mode(pkg,roots[j],"",j==0)) {
            fail(r,"package_damaged","Unexpected files or links in overlay tree.%s%s","","");return -1;
        }
    }
    if(!dependencies_ok(game)){fail(r,"package_damaged","CONTROL dependencies are incomplete or damaged.%s%s","","");return -1;}
    for(unsigned i=0;i<pkg->count;i++) {
        int seen=0;where[i]=-1;
        for(int j=0;j<2;j++) {
            if(snprintf(paths[i][j],512,"%s/%s",roots[j],pkg->files[i].rel)>=512) {fail(r,"package_damaged","Overlay path too long.%s%s","","");return -1;}
            if(lstat(paths[i][j],&st)) {if(errno==ENOENT)continue;fail(r,"package_damaged","Cannot inspect overlay file.%s%s","","");return -1;}
            if(!S_ISREG(st.st_mode)||hash_file(paths[i][j],&st,&fresh,sha)||!in(sha,pkg->files[i].helix)) {
                fail(r,"package_damaged","%s is damaged or unrecognized.%s",pkg->files[i].base,"");return -1;
            }
            seen++;where[i]=j;
        }
        if(seen!=1){fail(r,"package_damaged","%s is missing or duplicated.%s",pkg->files[i].base,"");return -1;}
        on+=where[i]==0;
    }
    const char *state=on==pkg->count?"helix":on==0?"original":"mixed";
    if(target<0){r->ok=1;snprintf(r->state,sizeof r->state,"%s",state);snprintf(r->reason,sizeof r->reason,"%s",state);return 0;}
    int dest=target==HS_HELIX?0:1;
    for(unsigned i=0;i<pkg->count;i++)if(where[i]!=dest) {
        if(overlay_dirs(roots[dest],pkg->files[i].rel,log)||(hs_gate&&hs_gate())||
           !lstat(paths[i][dest],&st)||errno!=ENOENT||swap_rename(paths[i][where[i]],paths[i][dest])) {
            fail(r,"mixed","Switch incomplete; retry before launching.%s%s","","");return -1;
        }
        r->moves++;
    }
    hs_result check;memset(&check,0,sizeof check);
    if(run_overlay(pkg,game,package,-1,&check,NULL)||strcmp(check.state,dest?"original":"helix")) {
        fail(r,"mixed","Final overlay verification failed; do not launch.%s%s","","");return -1;
    }
    r->ok=1;snprintf(r->state,sizeof r->state,"%s",check.state);snprintf(r->reason,sizeof r->reason,"Switch complete.");return 0;
}
static int run(const hs_package *pkg,const char *game_root,const char *package_root,int target,const char *cache_path,
               hs_result *r,FILE *log,int allow_rollback)
{
    memset(r,0,sizeof *r);
    if(!pkg||!pkg->count){fail(r,"package_missing","No HelixSR package for this game.%s%s","","");return -1;}
    if(pkg->folder==2)return run_overlay(pkg,game_root,package_root,target,r,log);
    if(pkg->folder)return run_folder(pkg,game_root,package_root,target,cache_path,r,log);
    if(!dir_ok(package_root,"helix")||!dir_ok(package_root,"original")){fail(r,"package_missing","The HelixSR package is not on the console.%s%s","","");return -1;}
    /* Static (about 25 KB) to stay off the payload stack; the nested calls below (rollback, final check) run only after
     * this call has finished with them, and save/reload the cache through its file. */
    static cache_t cache;cache_load(&cache,cache_path);
    static file_view views[8];int orig[8],helix[8];
    if(pkg->count>8){fail(r,"package_damaged","Package table too large.%s%s","","");return -1;}
    for(unsigned i=0;i<pkg->count;i++){
        if(view(&pkg->files[i],game_root,package_root,&cache,&views[i])){fail(r,"package_damaged","Path too long: %s%s",pkg->files[i].rel,"");cache_save(&cache,cache_path);return -1;}
        if(locate(&pkg->files[i],&views[i],&orig[i],&helix[i],r)){cache_save(&cache,cache_path);if(log)fprintf(log,"swap_refused state=%s reason=%s\n",r->state,r->reason);return -1;}
    }
    unsigned at_target=0;
    for(unsigned i=0;i<pkg->count;i++)at_target+=target==HS_HELIX?(helix[i]==P_G&&orig[i]==P_PO):(orig[i]==P_G&&helix[i]==P_PH);
    if(target<0){   /* status only */
        unsigned helix_on=0,original_on=0;
        for(unsigned i=0;i<pkg->count;i++){helix_on+=helix[i]==P_G&&orig[i]==P_PO;original_on+=orig[i]==P_G&&helix[i]==P_PH;}
        r->ok=1;snprintf(r->state,sizeof r->state,"%s",helix_on==pkg->count?"helix":original_on==pkg->count?"original":"mixed");
        snprintf(r->reason,sizeof r->reason,"%s",helix_on==pkg->count?"HelixSR is on.":original_on==pkg->count?"The game's previous files are in place.":
                 "An earlier switch was interrupted; the next launch finishes it.");
        cache_save(&cache,cache_path);return 0;
    }
    int failed=0;
    if(at_target!=pkg->count)
        for(unsigned i=0;i<pkg->count&&!failed;i++){
            int t=target==HS_HELIX?helix[i]:orig[i],o=target==HS_HELIX?orig[i]:helix[i],slot=target==HS_HELIX?P_PO:P_PH;
            failed=solve(&views[i],t,o,slot,r,log);
        }
    unsigned moves=r->moves;
    cache_save(&cache,cache_path);
    if(failed){
        if(log)fprintf(log,"swap_move_failed target=%d moves=%u\n",target,moves);
        if(allow_rollback&&target==HS_HELIX){
            hs_result back;run(pkg,game_root,package_root,HS_ORIGINAL,cache_path,&back,log,0);
            fail(r,back.ok?"original":"mixed",back.ok?"A file could not be moved; the game's previous files were put back.%s%s":
                 "A file could not be moved; launch with HelixSR Off to finish.%s%s","","");
        }else fail(r,"mixed","A file could not be moved; launch again to finish.%s%s","","");
        r->moves=moves;return -1;
    }
    /* Confirm the final layout from the files themselves. */
    hs_result check;run(pkg,game_root,package_root,-1,cache_path,&check,NULL,0);
    const char *want=target==HS_HELIX?"helix":"original";
    if(!check.ok||strcmp(check.state,want)){fail(r,"mixed","Files moved but the check failed (%s); launch again.%s",check.state,"");r->moves=moves;return -1;}
    r->ok=1;r->moves=moves;snprintf(r->state,sizeof r->state,"%s",want);
    snprintf(r->reason,sizeof r->reason,"%s",target==HS_HELIX?(moves?"HelixSR switched on.":"HelixSR was already on."):
             (moves?"HelixSR is off; the game's previous files are back.":"HelixSR was already off."));
    if(log)fprintf(log,"swap_done state=%s moves=%u\n",r->state,moves);
    return 0;
}
int hs_apply(const hs_package *p,const char *game_root,const char *package_root,enum hs_target t,const char *cache_path,hs_result *r,FILE *log)
{
    return run(p,game_root,package_root,(int)t,cache_path,r,log,1);
}
int hs_status(const hs_package *p,const char *game_root,const char *package_root,const char *cache_path,hs_result *r,FILE *log)
{
    return run(p,game_root,package_root,-1,cache_path,r,log,0);
}
