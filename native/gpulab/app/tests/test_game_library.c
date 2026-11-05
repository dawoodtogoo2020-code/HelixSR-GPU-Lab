/* Host test of game_library.c (u06): built-in measurements win over the executable scan, game profiles win over both,
 * SHADOW OF THE COLOSSUS (PS4) is recognised and routed to HelixSR, malformed profiles are ignored.
 *   clang -std=c11 -Wall -Wextra -Werror -I../source ../source/game_library.c test_game_library.c && ./a.out TMPDIR */
#include "game_library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *root;
static void put(const char *rel,const char *text)
{
    char path[256];snprintf(path,sizeof path,"%s/%s",root,rel);
    FILE *f=fopen(path,"w");assert(f);fputs(text,f);fclose(f);
}

int main(int argc,char **argv)
{
    assert(argc==2);root=argv[1];
    char dir[256];
    snprintf(dir,sizeof dir,"%s/scan",root);mkdir(dir,0755);
    snprintf(dir,sizeof dir,"%s/profiles",root);mkdir(dir,0755);
    gl_finding f;gl_route r;int dev=0;

    /* AW2 is limited to the exact beta version and Performance layout. A package
     * marker for another build or an arbitrary ID must never enable the switch. */
    assert(gl_find(root,"PPSA02572","01.200.007",&f)&&f.input_width==1505&&f.input_height==847);
    assert(!gl_find(root,"PPSA02572","01.200.008",&f));
    snprintf(dir,sizeof dir,"%s/packages",root);mkdir(dir,0755);
    snprintf(dir,sizeof dir,"%s/packages/PPSA02572-01.200.007",root);mkdir(dir,0755);
    char description[96];
    put("packages/PPSA02572-01.200.007/READY","HPKG1 aw2-t12\nPerformance 1505x847 -> 2560x1440\n");
    assert(gl_package_ready(root,"PPSA02572","01.200.007",description,sizeof description));
    assert(strstr(description,"1505x847")&&!gl_package_ready(root,"PPSA02572","01.200.008",NULL,0));
    put("packages/PPSA02572-01.200.007/READY","HPKG1 unknown\nUnverified\n");
    assert(!gl_package_ready(root,"PPSA02572","01.200.007",NULL,0));

    /* CONTROL: the old executable scan says FSR 3 (linked, not active); the in-game measurement (FSR 2) must win. */
    put("scan/PPSA34547.txt","HSCAN1\ntitle=PPSA34547\nversion=01.300.300\nprimary=FSR 3\nengine=Northlight\nsha256=ab\n");
    assert(gl_find(root,"PPSA34547","01.300.300",&f)&&!strcmp(f.primary,"FSR 2")&&!strcmp(f.basis,"Runtime probe"));
    gl_recommend("PPSA34547","01.300.300",&f,&r);
    assert(r.replace&&r.measured&&!r.scan_only&&r.input_width==1506&&r.output_width==2560);
    assert(!strcmp(gl_hook_status("PPSA34547",&f,0,&dev),"In development")&&dev);
    assert(!strcmp(gl_hook_status("PPSA34547",&f,1,&dev),"Ready")&&!dev);
    /* Another version of CONTROL falls back to its scan. */
    put("scan/PPSA34547.txt","HSCAN1\ntitle=PPSA34547\nversion=01.400.000\nprimary=FSR 3\nengine=Northlight\n");
    assert(gl_find(root,"PPSA34547","01.400.000",&f)&&!strcmp(f.primary,"FSR 3")&&!strcmp(f.basis,"Executable scan")&&!f.measured);
    gl_recommend("PPSA34547","01.400.000",&f,&r);
    assert(!r.scan_only); /* A mismatched managed version keeps its managed launch policy. */

    /* Measured SAROS route wins over linked-upscaler scan strings; READY is exact. */
    put("scan/PPSA07631.txt","HSCAN1\ntitle=PPSA07631\nversion=01.000.000\nprimary=FSR 3\nengine=Unreal Engine\n");
    assert(gl_find(root,"PPSA07631","01.000.000",&f)&&f.measured&&!strcmp(f.primary,"Unreal TSR"));
    gl_recommend("PPSA07631","01.000.000",&f,&r);
    assert(r.replace&&r.measured&&!r.scan_only&&r.output_width==3840&&r.output_height==2160);
    assert(!gl_package_ready(root,"PPSA07631","01.000.000",NULL,0));
    assert(!strcmp(gl_hook_status("PPSA07631",&f,0,&dev),"In development")&&dev);
    snprintf(dir,sizeof dir,"%s/packages/PPSA07631-01.000.000",root);mkdir(dir,0755);
    put("packages/PPSA07631-01.000.000/READY","HPKG1 saros-native-v1\n4K trial\n");
    assert(gl_package_ready(root,"PPSA07631","01.000.000",description,sizeof description));
    assert(!gl_package_ready(root,"PPSA07631","01.000.001",NULL,0));
    put("packages/PPSA07631-01.000.000/READY","HPKG1 saros-unknown\nWrong\n");
    assert(!gl_package_ready(root,"PPSA07631","01.000.000",NULL,0));

    /* SHADOW OF THE COLOSSUS: checkerboard, but motion vectors were measured, so HelixSR can take over. */
    assert(gl_find(root,"CUSA08809","01.00",&f)&&!strcmp(f.primary,"Game upscale")&&f.motion);
    gl_recommend("CUSA08809","01.00",&f,&r);
    assert(r.replace&&r.measured&&r.input_width==2560&&r.input_height==1440&&r.output_width==3840&&r.output_height==2160);
    assert(!strcmp(r.route,"Replace Game upscale with HelixSR"));
    assert(!strcmp(gl_hook_status("CUSA08809",&f,0,&dev),"In development")&&dev);
    printf("SOTC: %s | %s | %ux%u -> %ux%u | %s\n",f.primary,r.route,r.input_width,r.input_height,r.output_width,r.output_height,r.basis);

    /* A scanned checkerboard game without measured motion keeps its own upscaler. */
    put("scan/CUSA00001.txt","HSCAN1\ntitle=CUSA00001\nversion=01.00\nprimary=Checkerboard\nengine=Unknown\n");
    assert(gl_find(root,"CUSA00001","01.00",&f));gl_recommend("CUSA00001","01.00",&f,&r);
    assert(!r.replace&&r.scan_only&&!strcmp(r.route,"Launch as installed"));
    assert(!strcmp(gl_hook_status("CUSA00001",&f,0,&dev),"Not available yet")&&!dev);

    /* A game profile (new game, no rebuild) wins over the scan, including its hook state. */
    put("profiles/CUSA00001.txt","HPROF1\nversion=01.00\nupscaler=Checkerboard\nengine=Decima\ninput=1920x1080\noutput=3840x2160\n"
                                 "motion=1\nhook=development\nbasis=Measured on PS5 Pro: test profile\n");
    assert(gl_find(root,"CUSA00001","01.00",&f)&&!strcmp(f.basis,"Game profile")&&!strcmp(f.engine,"Decima"));
    gl_recommend("CUSA00001","01.00",&f,&r);
    assert(r.replace&&!r.scan_only&&r.input_width==1920&&r.output_height==2160&&!strcmp(r.basis,"Measured on PS5 Pro: test profile"));
    assert(!strcmp(gl_hook_status("CUSA00001",&f,0,&dev),"In development")&&dev);
    /* ... and is ignored for another version, or when any field is malformed (odd size, input larger than output,
     * unknown hook, missing basis). */
    assert(gl_find(root,"CUSA00001","01.01",&f)==0);
    static const char *const bad[]={
        "HPROF1\nversion=01.00\nupscaler=X\nengine=Y\ninput=1919x1080\noutput=3840x2160\nmotion=1\nhook=none\nbasis=b\n",
        "HPROF1\nversion=01.00\nupscaler=X\nengine=Y\ninput=3840x2160\noutput=1920x1080\nmotion=1\nhook=none\nbasis=b\n",
        "HPROF1\nversion=01.00\nupscaler=X\nengine=Y\ninput=1920x1080\noutput=3840x2160\nmotion=1\nhook=ready\nbasis=b\n",
        "HPROF1\nversion=01.00\nupscaler=X\nengine=Y\ninput=1920x1080\noutput=3840x2160\nmotion=2\nhook=none\nbasis=b\n",
        "HPROF1\nversion=01.00\nupscaler=X\nengine=Y\ninput=1920x1080\noutput=3840x2160\nmotion=1\nhook=none\n",
        "HPROF0\nversion=01.00\nupscaler=X\nengine=Y\ninput=1920x1080\noutput=3840x2160\nmotion=1\nhook=none\nbasis=b\n"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
        put("profiles/CUSA00001.txt",bad[i]);
        assert(gl_find(root,"CUSA00001","01.00",&f)&&!strcmp(f.basis,"Executable scan"));   /* back to the scan */
    }
    /* hook=none: measured, but not in development. */
    put("profiles/CUSA00001.txt","HPROF1\nversion=01.00\nupscaler=TAA\nengine=E\ninput=1920x1080\noutput=1920x1080\nmotion=0\nhook=none\nbasis=b\n");
    assert(gl_find(root,"CUSA00001","01.00",&f)&&!f.motion&&!f.hook[0]);
    gl_recommend("CUSA00001","01.00",&f,&r);assert(!r.replace);
    assert(!strcmp(gl_hook_status("CUSA00001",&f,0,&dev),"Not available yet"));
    puts("game library: PASS");
    return 0;
}
