/* Host-only representative screen snapshots. This does not interact with a console. */
#include "ui_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_ppm(const char *path,const uint32_t *p) {
    FILE *f=fopen(path,"wb");if(!f)return 1;
    int bad=fprintf(f,"P6\n%d %d\n255\n",US_WIDTH,US_HEIGHT)<0;
    unsigned char row[US_WIDTH*3];
    for(int y=0;y<US_HEIGHT&&!bad;y++) {
        for(int x=0;x<US_WIDTH;x++){uint32_t v=p[y*US_WIDTH+x];row[x*3]=v>>16;row[x*3+1]=v>>8;row[x*3+2]=v;}
        if(fwrite(row,1,sizeof row,f)!=sizeof row)bad=1;
    }
    if(fclose(f))bad=1;return bad;
}
int main(int argc,char **argv) {
    if(argc!=2||argv[1][0]!='/'){fputs("Provide an absolute output directory.\n",stderr);return 1;}
    uint32_t *pixels=malloc((size_t)US_WIDTH*US_HEIGHT*sizeof *pixels),*background=malloc((size_t)US_WIDTH*US_HEIGHT*sizeof *background);
    if(!pixels||!background){free(pixels);free(background);return 1;}
    static us_screen screen;static us_model m;us_init(&screen,pixels,background);
    memset(&m,0,sizeof m);m.game_count=1;m.menu=US_MENU_SETTINGS;m.focus=US_HELIX;
    m.controller=1;m.settings_ok=1;m.reset_armed=-1;
    us_game *g=&m.games[0];
    snprintf(g->name,sizeof g->name,"Alan Wake 2");snprintf(g->title,sizeof g->title,"PPSA02572");
    snprintf(g->build,sizeof g->build,"01.200.007");snprintf(g->upscaler,sizeof g->upscaler,"FSR 2");g->kind=US_KNOWN;g->ready=1;
    m.input_width=1505;m.input_height=847;m.output_width=2560;m.output_height=1440;
    snprintf(m.package,sizeof m.package,"HelixSR t12 Performance");
    snprintf(m.engine,sizeof m.engine,"Northlight");snprintf(m.basis,sizeof m.basis,"Installed package");
    snprintf(m.route,sizeof m.route,"Replace FSR 2 with HelixSR");snprintf(m.hook,sizeof m.hook,"Candidate");m.hook_ready=1;
    snprintf(m.status,sizeof m.status,"Host rendering: candidate, not console tested");
    snprintf(m.detail,sizeof m.detail,"Performance: 1505x847 to 2560x1440");
    snprintf(m.resolution_value,sizeof m.resolution_value,"1505x847 -> 2560x1440");
    snprintf(m.frames_value,sizeof m.frames_value,"No recording");
    snprintf(m.feedback,sizeof m.feedback,"Preview only. Choices shown are not console results.");
    int failed=0;
    for(int on=0;on<=1;on++) {
        m.helix_on=on;char path[1024];
        if(snprintf(path,sizeof path,"%s/aw2-%s-host-candidate.ppm",argv[1],on?"on":"off")>=(int)sizeof path||
           !us_render(&screen,&m)||write_ppm(path,pixels))failed=1;
        else printf("%s\n",path);
    }
    m.focus=US_HUD;
    char path[1024];
    if(snprintf(path,sizeof path,"%s/aw2-hud-unavailable-host-candidate.ppm",argv[1])>=(int)sizeof path||
       !us_render(&screen,&m)||write_ppm(path,pixels))failed=1;
    m.hud_available=1;m.hud_on=1;
    snprintf(g->name,sizeof g->name,"Shadow of the Colossus");snprintf(g->title,sizeof g->title,"CUSA08809");
    snprintf(g->build,sizeof g->build,"01.00");snprintf(g->upscaler,sizeof g->upscaler,"Game upscale");
    m.pro=1;m.input_width=2560;m.input_height=1440;m.output_width=3840;m.output_height=2160;
    snprintf(m.package,sizeof m.package,"HelixSR live3");snprintf(m.engine,sizeof m.engine,"Bluepoint");
    snprintf(m.route,sizeof m.route,"Replace Game upscale with HelixSR");
    snprintf(m.detail,sizeof m.detail,"Synthetic layout preview only");
    snprintf(m.resolution_value,sizeof m.resolution_value,"2560x1440 -> 3840x2160");
    if(snprintf(path,sizeof path,"%s/sotc-hud-available-host-candidate.ppm",argv[1])>=(int)sizeof path||
       !us_render(&screen,&m)||write_ppm(path,pixels))failed=1;
    m.screen=US_SCREEN_FRAMES;
    snprintf(m.frames.game,sizeof m.frames.game,"CONTROL");
    snprintf(m.frames.status,sizeof m.frames.status,"Record next launch: recorder not included in this build.");
    if(snprintf(path,sizeof path,"%s/control-recorder-unavailable-host-candidate.ppm",argv[1])>=(int)sizeof path||
       !us_render(&screen,&m)||write_ppm(path,pixels))failed=1;
    free(background);free(pixels);return failed;
}
