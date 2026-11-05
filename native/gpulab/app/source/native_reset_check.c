#include "reset_all.h"
#include "render_options.h"
#include "profile_file_adapter.h"
#include "profile_store.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Reset only synthetic profiles under a dedicated diagnostic directory. */
int helix_native_reset_check(FILE *log,const ap_identity *source) {
    const char *base="/data/helixsr-speed/reset-selftest";
    const char *settings="/data/helixsr-speed/reset-selftest/settings";
    const char *render="/data/helixsr-speed/reset-selftest/render-settings";
    ap_identity ids[2]={{0},{0}};(void)source;
    for(unsigned i=0;i<2;i++) {
        strcpy(ids[i].device,"ps5");strcpy(ids[i].build,"01.000.000");
        memset(ids[i].executable_hash,'e',64);ids[i].executable_hash[64]=0;
    }
    strcpy(ids[0].title,"PPSA99991");strcpy(ids[1].title,"PPSA99992");
    mkdir(base,0700);mkdir(settings,0700);mkdir(render,0700);
    for(unsigned i=0;i<2;i++) {
        pfa_store store=PFA_INITIALIZER;ps_profile p,saved;
        if(pfa_open(&store,settings,&ids[i])!=PS_OK)goto failed;
        ps_result rc=ps_load(pfa_get_io(&store),&ids[i],&p);
        if(rc==PS_OK||rc==PS_DEFAULT){p.desired=AP_ON;p.quality=3;rc=ps_save(pfa_get_io(&store),&p,p.revision,&saved);}
        ps_result closed=pfa_close(&store);if(rc!=PS_OK||closed!=PS_OK)goto failed;
        ro_options options;int r=ro_load(render,&ids[i],&options);if(r<0)goto failed;
        options.backend=RO_HELIXSR;options.output_width=3840;options.output_height=2160;
        if(ro_apply_preset(&options,RO_BALANCED)||ro_save(render,&ids[i],&options,options.revision))goto failed;
    }
    unsigned reset=0,errors=0;
    int rc=helix_reset_all(settings,render,&reset,&errors);
    fprintf(log,"reset_check result=%d reset=%u errors=%u isolated=1\n",rc,reset,errors);
    if(rc||reset<2||errors)goto failed;
    for(unsigned i=0;i<2;i++) {
        pfa_store store=PFA_INITIALIZER;ps_profile p;
        if(pfa_open(&store,settings,&ids[i])!=PS_OK)goto failed;
        ps_result r=ps_load(pfa_get_io(&store),&ids[i],&p),closed=pfa_close(&store);
        if(r!=PS_OK||closed!=PS_OK||p.desired!=AP_OFF||p.quality!=3)goto failed;
        ro_options options;
        if(ro_load(render,&ids[i],&options)!=RO_OK||options.backend!=RO_GAME_DEFAULT||options.preset!=RO_NATIVE)goto failed;
    }
    fputs("reset_check=passed profiles=2 OFF=1 game_default=1 quality_preserved=1 isolated=1\n",log);return 0;
failed:
    fputs("reset_check=failed isolated=1\n",log);return -1;
}
