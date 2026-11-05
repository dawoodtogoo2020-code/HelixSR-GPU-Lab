/* Host test of helixsr_swap.c on a temporary game folder and package (build.sh). The module is included so the test can
 * hash its synthetic files with the same SHA-256 and inject rename failures. */
static char test_support_root[256],test_saros_support_root[256];
#define SAROS_SUPPORT_ROOT test_saros_support_root
#define AW2_SUPPORT_ROOT test_support_root
#include "helixsr_swap.c"
#include "../common/package_support.h"
#include <assert.h>
#include <dirent.h>

static char root[256],game[300],pkg[300],cache[300];
static char sha_of(const char *data,char out[65]) { sha_ctx c;sha_init(&c);sha_update(&c,(const unsigned char *)data,strlen(data));sha_hex(&c,out);return 0; }
static void put(const char *path,const char *data)
{
    FILE *f=fopen(path,"wb");assert(f);fputs(data,f);assert(!fclose(f));
}
static int has(const char *path,const char *data)
{
    static char buf[256];FILE *f=fopen(path,"rb");if(!f)return data==NULL;
    size_t n=fread(buf,1,sizeof buf-1,f);fclose(f);buf[n]=0;return data&&!strcmp(buf,data);
}
static void path(char *out,const char *a,const char *b) { snprintf(out,512,"%s/%s",a,b); }
static const char *E0="eboot original SELF",*E0I="eboot original installed form",*E1="eboot helix SELF",*E1I="eboot helix installed form";
static const char *B0="blob original",*B1="blob helix",*X="something else";
static char h[6][65];
static hs_file files[2];
static hs_package package;
static void setup(void)
{
    char cmd[700];snprintf(cmd,sizeof cmd,"rm -rf '%s'/* && mkdir -p '%s/data/sub' '%s/original' '%s/helix'",root,game,pkg,pkg);assert(!system(cmd));
    char p[512];
    path(p,game,"eboot.bin");put(p,E0);path(p,game,"data/sub/blob.bin");put(p,B0);
    path(p,pkg,"helix/eboot.bin");put(p,E1);path(p,pkg,"helix/blob.bin");put(p,B1);
}
/* Contents expected at the four places of each file (NULL = absent). */
static void expect(const char *ge,const char *pe0,const char *pe1,const char *gb,const char *pb0,const char *pb1)
{
    char p[512];
    path(p,game,"eboot.bin");assert(has(p,ge));path(p,pkg,"original/eboot.bin");assert(has(p,pe0));path(p,pkg,"helix/eboot.bin");assert(has(p,pe1));
    path(p,game,"data/sub/blob.bin");assert(has(p,gb));path(p,pkg,"original/blob.bin");assert(has(p,pb0));path(p,pkg,"helix/blob.bin");assert(has(p,pb1));
    path(p,game,"eboot.bin.hlx-new");assert(has(p,NULL));path(p,game,"data/sub/blob.bin.hlx-new");assert(has(p,NULL));
}
static int fail_after=-1,renames=0;
static int failing_rename(const char *a,const char *b) { if(fail_after>=0&&renames++>=fail_after){errno=EIO;return -1;} return rename(a,b); }
static int gate_busy(void) { return 1; }
static int read_fail_after=-1,read_seen=0;
static struct dirent *failing_readdir(DIR *d) {
    if(read_fail_after>=0&&read_seen>=read_fail_after){errno=EIO;return NULL;}
    struct dirent *e=readdir(d);
    if(e&&strcmp(e->d_name,".")&&strcmp(e->d_name,".."))read_seen++;
    return e;
}

int main(void)
{
    /* The production safety check rejects symlinked ancestors (/tmp on macOS). */
    char *tmp=realpath("/tmp",NULL);assert(tmp);
    assert(snprintf(root,sizeof root,"%s/helixsr-swap-test.XXXXXX",tmp)<(int)sizeof root);
    free(tmp);assert(mkdtemp(root));
    snprintf(game,sizeof game,"%s/game",root);snprintf(pkg,sizeof pkg,"%s/pkg",root);snprintf(cache,sizeof cache,"%s/cache.txt",root);
    char cmd[400];
    sha_of(E0,h[0]);sha_of(E0I,h[1]);sha_of(E1,h[2]);sha_of(E1I,h[3]);sha_of(B0,h[4]);sha_of(B1,h[5]);
    files[0]=(hs_file){"eboot.bin","eboot.bin",{h[0],h[1]},{h[2],h[3]}};
    files[1]=(hs_file){"data/sub/blob.bin","blob.bin",{h[4],NULL},{h[5],NULL}};
    package=(hs_package){"PPSA00000","01.000.000","test","test",2,files,0};
    hs_result r;unsigned cases=0;

    /* 0. SHA-256 against FIPS 180-2 vectors, the last one through hash_file (1 MiB reads, cache) */
    char d[65];
    sha_of("",d);assert(!strcmp(d,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha_of("abc",d);assert(!strcmp(d,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    sha_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",d);
    assert(!strcmp(d,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    {
        char p[512];path(p,root,"million-a");FILE *f=fopen(p,"wb");assert(f);
        for(int i=0;i<1000000;i++)fputc('a',f);
        assert(!fclose(f));
        struct stat st;assert(!stat(p,&st));static cache_t c;memset(&c,0,sizeof c);
        assert(!hash_file(p,&st,&c,d)&&!strcmp(d,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
        assert(!hash_file(p,&st,&c,d)&&!strcmp(d,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")&&c.count==1);
        unlink(p);cases+=5;
    }

    /* 1. on, on again (no moves), off, off again */
    setup();
    assert(!hs_apply(&package,game,pkg,HS_HELIX,cache,&r,stdout)&&r.ok&&!strcmp(r.state,"helix")&&r.moves==6);
    expect(E1,E0,NULL,B1,B0,NULL);
    assert(!hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok&&r.moves==0);
    assert(!hs_status(&package,game,pkg,cache,&r,NULL)&&!strcmp(r.state,"helix"));
    assert(!hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&!strcmp(r.state,"original")&&r.moves==6);
    expect(E0,NULL,E1,B0,NULL,B1);
    assert(!hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&r.moves==0);
    cases+=6;

    /* 2. every interrupted layout finishes in both directions */
    char g[512],n[512],po[512],ph[512];
    path(g,game,"eboot.bin");path(n,game,"eboot.bin.hlx-new");path(po,pkg,"original/eboot.bin");path(ph,pkg,"helix/eboot.bin");
    const char *layouts[][4]={ /* contents at G, N, PO, PH */
        {E0,E1,NULL,NULL},{E1,E0,NULL,NULL},{NULL,E1,E0,NULL},{NULL,E0,NULL,E1},{NULL,NULL,E0,E1},{E1,NULL,E0,NULL},{E0,NULL,NULL,E1}};
    for(unsigned l=0;l<sizeof layouts/sizeof *layouts;l++)
        for(int target=0;target<2;target++){
            setup();unlink(g);unlink(ph);
            const char *places[4]={g,n,po,ph};
            for(int p=0;p<4;p++)if(layouts[l][p])put(places[p],layouts[l][p]);
            assert(!hs_apply(&package,game,pkg,target?HS_HELIX:HS_ORIGINAL,cache,&r,NULL)&&r.ok);
            if(target)expect(E1,E0,NULL,B1,B0,NULL);else expect(E0,NULL,E1,B0,NULL,B1);
            cases++;
        }

    /* 3. the executable's installed form counts as the same version */
    setup();put(g,E0I);
    assert(!hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);expect(E1,E0I,NULL,B1,B0,NULL);cases++;

    /* 4. refusals change nothing */
    setup();put(g,X);
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"unrecognized")&&!r.moves);
    expect(X,NULL,E1,B0,NULL,B1);cases++;
    setup();char pb[512];path(pb,pkg,"helix/blob.bin");put(pb,X);
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"package_damaged")&&!r.moves);
    expect(E0,NULL,E1,B0,NULL,X);cases++;
    setup();unlink(pb);
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"package_missing")&&!r.moves);
    expect(E0,NULL,E1,B0,NULL,NULL);cases++;
    setup();put(po,E1);   /* a HelixSR copy in the original slot: duplicated and misplaced */
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"package_damaged")&&!r.moves);cases++;
    setup();put(n,E0);    /* two originals */
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"package_damaged")&&!r.moves);cases++;
    setup();snprintf(cmd,sizeof cmd,"rm -rf '%s/original'",pkg);assert(!system(cmd));
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"package_missing")&&!r.moves);cases++;
    setup();unlink(g);    /* the game's own executable missing */
    assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!strcmp(r.state,"unrecognized")&&!r.moves);cases++;

    /* 5. a file changed after it was cached is seen as changed */
    setup();assert(!hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);
    sleep(1);put(g,X);
    assert(hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!strcmp(r.state,"unrecognized"));cases++;

    /* 6. a failed rename is rolled back to the game's own files; a failed rollback is finished by the next run */
    swap_rename=failing_rename;
    for(int k=0;k<6;k++){
        setup();renames=0;fail_after=k;
        assert(hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok);
        fail_after=-1;
        if(!strcmp(r.state,"original"))expect(E0,NULL,E1,B0,NULL,B1);
        else {assert(!strcmp(r.state,"mixed"));assert(!hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok);expect(E0,NULL,E1,B0,NULL,B1);}
        cases++;
    }
    setup();assert(!hs_apply(&package,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);
    renames=0;fail_after=2;   /* turning off fails half-way: next run finishes */
    assert(hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"mixed"));
    fail_after=-1;assert(!hs_status(&package,game,pkg,cache,&r,NULL)&&!strcmp(r.state,"mixed"));
    assert(!hs_apply(&package,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok);expect(E0,NULL,E1,B0,NULL,B1);cases+=2;
    swap_rename=rename;

    /* 7. the real CONTROL table: whole-folder package, four files, both executable forms listed */
    const hs_package *control=hs_find("PPSA34547","01.300.300");
    assert(control&&control->folder==2&&control->count==4&&control->files[0].helix[1]&&!hs_find("PPSA34547","01.000.000"));
    for(unsigned i=0;i<control->count;i++)
        for(int k=0;k<2;k++){const char *b=control->files[i].helix[k];assert(!b||strlen(b)==64);assert(!control->files[i].original[k]);}
    cases++;

    /* Overlay transitions retain all dependencies and resume after any rename failure. */
    {
        hs_package fp=package;fp.folder=2;char p[512],stored[512],cmd2[900];
        hs_dependency dep[3]={{"a",{0,h[0]},{0,h[3]}},{"b",{0,h[1]},{0,h[4]}},{"c",{0,h[2]},{0,h[5]}}};
        const char *contents[3]={E0,E0I,E1};
        const char *self_contents[3]={E1I,B0,B1};
        for(int i=0;i<3;i++) {
            dep[i].elf.size=(off_t)strlen(contents[i]);
            dep[i].self.size=(off_t)strlen(self_contents[i]);
        }
        control_dependencies=dep;path(stored,pkg,"stored/game");
        for(int direction=0;direction<2;direction++)for(int k=0;k<2;k++) {
            snprintf(cmd2,sizeof cmd2,"rm -rf '%s'/* && mkdir -p '%s/data/sub' '%s/stored/game' '%s/fakelib2'",root,game,pkg,game);assert(!system(cmd2));
            path(p,game,"eboot.bin");put(p,E1);path(p,game,"data/sub/blob.bin");put(p,B1);
            for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);put(p,contents[i]);}
            if(direction)assert(!hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL));
            swap_rename=failing_rename;renames=0;fail_after=k;
            assert(hs_apply(&fp,game,pkg,direction?HS_HELIX:HS_ORIGINAL,NULL,&r,NULL)&&!r.ok);
            fail_after=-1;swap_rename=rename;
            if(k)assert(!hs_status(&fp,game,pkg,NULL,&r,NULL)&&!strcmp(r.state,"mixed"));
            assert(!hs_apply(&fp,game,pkg,direction?HS_HELIX:HS_ORIGINAL,NULL,&r,NULL)&&r.ok);
            for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);assert(has(p,contents[i]));}
            cases++;
        }
        path(p,game,"fakelib2/a");put(p,X);assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);put(p,E0);
        path(p,game,"fakelib2/b");assert(!unlink(p));assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);put(p,E0I);
        path(p,game,"fakelib2/unknown");put(p,X);assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);assert(!unlink(p));
        path(p,game,"fakelib2/b");assert(!unlink(p));assert(!symlink("a",p));assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);assert(!unlink(p));put(p,E0I);
        /* Native opens see the stored SELF; both transitions must preserve those bytes. */
        for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);put(p,self_contents[i]);}
        assert(!hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&r.ok&&r.moves==2);
        assert(!hs_apply(&fp,game,pkg,HS_HELIX,NULL,&r,NULL)&&r.ok&&r.moves==2);
        for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);assert(has(p,self_contents[i]));}
        /* Reject the right hash paired with the other form's size. */
        dep[0].self.sha=h[0];dep[0].elf.sha=h[3];
        assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.ok&&!r.moves);
        dep[0].self.sha=h[3];dep[0].elf.sha=h[0];
        path(p,game,"fakelib2/c");put(p,"blob HELIX");
        assert(strlen("blob HELIX")==strlen(B1));
        assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.ok&&!r.moves);
        for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);put(p,contents[i]);}
        cases+=4;
        path(p,stored,"eboot.bin");put(p,E1);assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);assert(!unlink(p));
        path(p,game,"eboot.bin");assert(!unlink(p));assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.moves);put(p,E1);
        /* A directory read error before or after an entry cannot be mistaken for EOF. */
        swap_readdir=failing_readdir;
        for(int n=0;n<2;n++) {
            read_seen=0;read_fail_after=n;assert(!folder_tree_mode(&fp,game,"",1));
            read_seen=0;assert(!dependencies_ok(game));
            read_seen=0;assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&!r.ok&&!r.moves);
            for(int i=0;i<3;i++){snprintf(p,sizeof p,"%s/fakelib2/%s",game,dep[i].name);assert(has(p,contents[i]));}
            path(p,game,"eboot.bin");assert(has(p,E1));
            cases+=3;
        }
        swap_readdir=readdir;read_fail_after=-1;
        snprintf(cmd2,sizeof cmd2,"rm -rf '%s/fakelib2'",game);assert(!system(cmd2));
        assert(!hs_apply(&fp,game,pkg,HS_ORIGINAL,NULL,&r,NULL)&&r.ok);
        control_dependencies=control_dependencies_default;cases+=7;
        const hs_package *aw=hs_find("PPSA02572","01.200.007");
        assert(aw&&aw->files[0].helix[2]&&!strcmp(aw->files[0].helix[2],"31a3ffd1b582b6d363da7432c41962e1b48ba59fcaa14befaae916d38cb66c35"));cases++;
    }

    /* 8. whole-folder package: on = game folder is the build, off = package/stored/game, one rename each way */
    {
        hs_package fp=package;fp.folder=1;char p[512],cmd[700],stored[512];path(stored,pkg,"stored/game");
        snprintf(cmd,sizeof cmd,"rm -rf '%s'/* && mkdir -p '%s/data/sub' '%s/stored'",root,game,pkg);assert(!system(cmd));
        path(p,game,"eboot.bin");put(p,E1I);path(p,game,"data/sub/blob.bin");put(p,B1);path(p,game,"extra.prx");put(p,X);
        assert(hs_status(&fp,game,pkg,cache,&r,NULL)&&!r.ok);
        path(p,game,"extra.prx");assert(!unlink(p));
        assert(!hs_status(&fp,game,pkg,cache,&r,NULL)&&r.ok&&!strcmp(r.state,"helix"));
        assert(!hs_apply(&fp,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&r.moves==1&&!strcmp(r.state,"original"));
        path(p,game,"eboot.bin");assert(has(p,NULL));path(p,stored,"eboot.bin");assert(has(p,E1I));
        assert(!hs_apply(&fp,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&!r.moves);
        hs_gate=gate_busy;assert(hs_apply(&fp,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"original"));hs_gate=NULL;
        path(p,stored,"eboot.bin");assert(has(p,E1I));
        assert(!hs_apply(&fp,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok&&r.moves==1&&!strcmp(r.state,"helix"));
        path(p,stored,"eboot.bin");assert(has(p,NULL));
        path(p,game,"data/sub/blob.bin");put(p,B0);   /* another build in the backport folder: refuse both ways */
        assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"unrecognized"));
        assert(hs_apply(&fp,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok);path(p,game,"eboot.bin");assert(has(p,E1I));
        path(p,game,"data/sub/blob.bin");put(p,B1);snprintf(cmd,sizeof cmd,"mkdir -p '%s'",stored);assert(!system(cmd));
        assert(hs_apply(&fp,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"package_damaged"));   /* both present */
        snprintf(cmd,sizeof cmd,"rm -rf '%s' '%s'",stored,game);assert(!system(cmd));
        assert(hs_apply(&fp,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok&&!strcmp(r.state,"package_missing"));
        cases+=8;
    }
    /* Exact model/version gate and AW2 dependency checks using synthetic hashes. */
    {
        assert(gp_supported("ps5-regular","PPSA02572","01.200.007"));
        assert(!gp_supported("ps5-pro","PPSA02572","01.200.007"));
        assert(!gp_supported(NULL,"PPSA02572","01.200.007"));
        assert(!gp_supported("ps5-regular","PPSA02572","01.200.008"));
        assert(gp_supported("ps5-pro","CUSA08809","01.00"));
        assert(hs_find("PPSA02572","01.200.007")&&!hs_find("PPSA02572","01.200.008"));
        snprintf(cmd,sizeof cmd,"rm -rf '%s'/* && mkdir -p '%s/stored/game' '%s/support'",root,pkg,root);assert(!system(cmd));
        snprintf(test_support_root,sizeof test_support_root,"%s/support",root);
        char p[512];path(p,pkg,"stored/game/eboot.bin");put(p,E1);
        path(p,test_support_root,"copy.bin");put(p,E1);path(p,test_support_root,"chain.bin");put(p,B1);
        hs_file af={"eboot.bin","eboot.bin",{NULL,NULL},{h[2],h[3]}};
        hs_file deps[2]={{"copy.bin","copy.bin",{NULL,NULL},{h[2],NULL}},{"chain.bin","chain.bin",{NULL,NULL},{h[5],NULL}}};
        hs_package ap={"PPSA02572","01.200.007","test","test",1,&af,1}; support_files=deps;
        assert(!hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);
        path(p,test_support_root,"copy.bin");put(p,X);
        assert(!hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok);
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok);
        path(p,test_support_root,"copy.bin");put(p,E1);
        swap_rename=failing_rename;fail_after=0;renames=0;
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok);
        swap_rename=rename;fail_after=-1;
        assert(!hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);
        path(p,game,"extra");put(p,X);assert(hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!r.ok);
        unlink(p);
        path(p,game,"alias");assert(!symlink(test_support_root,p));
        assert(hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&!r.ok);unlink(p);
        assert(!hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok);
        char alias[512],outside[512];path(alias,root,"alias");path(outside,root,"real");
        assert(!mkdir(outside,0755));assert(!symlink(outside,alias));
        char unsafe[512];snprintf(unsafe,sizeof unsafe,"%s/game",alias);
        assert(hs_apply(&ap,unsafe,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok);unlink(alias);
        path(p,root,"device.txt");put(p,"ps5-pro\n");assert(!strcmp(gp_device(root),"ps5-pro"));
        put(p,"unknown\n");assert(!gp_device(root));unlink(p);assert(!gp_device(root));
        support_files=aw2_support;cases+=16;
    }

    /* SAROS's independently verified asset is required for ON, never for restoring OFF. */
    {
        assert(gp_supported("ps5-regular","PPSA07631","01.000.000"));
        assert(!gp_supported("ps5-pro","PPSA07631","01.000.000"));
        assert(!gp_supported("ps5-regular","PPSA07631","01.000.001"));
        assert(hs_find("PPSA07631","01.000.000")&&!hs_find("PPSA07631","01.000.001"));
        snprintf(cmd,sizeof cmd,"rm -rf '%s'/* && mkdir -p '%s/stored/game' '%s/saros'",root,pkg,root);assert(!system(cmd));
        snprintf(test_saros_support_root,sizeof test_saros_support_root,"%s/saros",root);
        char p[512],asset[512],stored[512];path(stored,pkg,"stored/game/eboot.bin");put(stored,E1);
        path(asset,test_saros_support_root,"tile.bin");
        hs_file af={"eboot.bin","eboot.bin",{NULL,NULL},{h[2],NULL}};
        hs_file dep={"tile.bin","tile.bin",{NULL,NULL},{h[5],NULL}};
        hs_package ap={"PPSA07631","01.000.000","saros-native-v1","test",1,&af,1};
        saros_support_files=&dep;saros_support_bytes=(off_t)strlen(B1);
        /* Missing, same-length wrong hash, wrong size, extra entries and symlinks refuse without moves. */
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.ok&&!r.moves&&has(stored,E1));
        put(asset,"blob HELIX");assert(strlen("blob HELIX")==strlen(B1));
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves&&has(stored,E1));
        put(asset,X);assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves);
        put(asset,B1);path(p,test_saros_support_root,"extra");put(p,X);
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves);assert(!unlink(p));
        path(p,root,"real-tile");put(p,B1);assert(!unlink(asset));assert(!symlink(p,asset));
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves);assert(!unlink(asset));put(asset,B1);
        hs_gate=gate_busy;assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves);hs_gate=NULL;
        swap_rename=failing_rename;fail_after=0;renames=0;
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves&&has(stored,E1));
        swap_rename=rename;fail_after=-1;
        assert(!hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok&&r.moves==1);
        path(p,game,"eboot.bin");assert(has(p,E1));
        assert(!hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok&&!r.moves);
        assert(!unlink(asset));
        assert(!hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&r.moves==1&&has(stored,E1));
        assert(!hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&!r.moves);
        put(asset,B1);assert(!hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&r.ok);
        put(asset,"blob HELIX");
        assert(!hs_apply(&ap,game,pkg,HS_ORIGINAL,cache,&r,NULL)&&r.ok&&has(stored,E1));
        assert(hs_apply(&ap,game,pkg,HS_HELIX,cache,&r,NULL)&&!r.moves);
        saros_support_files=saros_support_default;saros_support_bytes=2200624;cases+=18;
    }
    snprintf(cmd,sizeof cmd,"rm -rf '%s'",root);assert(!system(cmd));
    printf("swap logic: PASS (%u cases)\n",cases);
    return 0;
}
