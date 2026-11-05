#include "ui_editor.h"
#include "ui_parts.h"
#include <stdio.h>
#include <string.h>

/* The frame-rate screen: one log of the selected game. Stat tiles, FPS over play time, frame time over play time, a
 * frame-time histogram and how the numbers were measured. Every value says its basis; the HelixSR block time is shown
 * as a part of each frame, never as a frame rate. */
enum { CX=72, CY=148, CW=1776, CH=812, IX=104, GRAPH_X=176, GRAPH_W=US_TIME_COLUMNS, SIDE_X=1388, SIDE_W=428,
       TILES_Y=262, TILE_H=104, FPS_Y=402, FPS_H=196, TIME_Y=646, TIME_H=196 };

static void milli_text(char *out,unsigned size,unsigned milli,unsigned decimals)
{
    if(decimals==1)snprintf(out,size,"%u.%u",(milli+50u)/1000u,((milli+50u)%1000u)/100u);
    else snprintf(out,size,"%u.%02u",(milli+5u)/1000u,((milli+5u)%1000u)/10u);
}
static void ms_text(char *out,unsigned size,unsigned us) { snprintf(out,size,"%u.%02u ms",(us+5u)/1000u,((us+5u)%1000u)/10u); }
/* A stat tile: value, caption, and its basis in small text. */
static void tile(uc_canvas *c,int x,int w,const char *value,const char *caption,const char *basis,uint32_t tone)
{
    uc_round_rect(c,(float)x,(float)TILES_Y,(float)w,(float)TILE_H,16.0f,0x0bffffffu);
    uf_draw_fit(c,UF_STAT,x+18,TILES_Y+48,w-30,value,tone);
    us_caption(c,x+18,TILES_Y+74,caption,TEXT3);
    uf_draw_fit(c,UF_SMALL,x+18,TILES_Y+96,w-30,basis,TEXT3);
}
static void axis_label(uc_canvas *c,int y,const char *text) { uf_draw_right(c,UF_CAPTION,GRAPH_X-12,y+5,text,TEXT3); }
/* A dashed reference line across a graph. */
static void reference(uc_canvas *c,int y,uint32_t colour)
{
    for(int x=GRAPH_X;x<GRAPH_X+GRAPH_W;x+=12)uc_rect(c,x,y,6,1,colour);
}
static void fps_graph(uc_canvas *c,const us_frames *f)
{
    us_caption(c,IX,FPS_Y-14,"FRAMES PER SECOND, PER SECOND OF PLAY",TEXT3);
    uc_round_rect(c,(float)GRAPH_X,(float)FPS_Y,(float)GRAPH_W,(float)FPS_H,10.0f,0x08ffffffu);
    unsigned top=70,peak=0;
    for(unsigned i=0;i<f->fps_count;++i)if(f->fps[i]>peak)peak=f->fps[i];
    while(top*10u<peak+50u)top+=30u;
    for(unsigned level=30;level<top;level+=30) {
        const int y=FPS_Y+FPS_H-(int)((unsigned long long)level*FPS_H/top);char text[8];snprintf(text,sizeof text,"%u",level);
        reference(c,y,level%60u?0x22ffffffu:0x40ffffffu);axis_label(c,y,text);
    }
    axis_label(c,FPS_Y+FPS_H,"0");
    if(f->fps_count<2){uf_draw(c,UF_SMALL,GRAPH_X+20,FPS_Y+FPS_H/2,"Less than two seconds of play",TEXT3,0);return;}
    float px=0.0f,py=0.0f;
    for(unsigned i=0;i<f->fps_count;++i) {
        const float x=(float)GRAPH_X+(float)GRAPH_W*((float)i+0.5f)/(float)f->fps_count;
        const float y=(float)(FPS_Y+FPS_H)-(float)FPS_H*(float)f->fps[i]/(10.0f*(float)top);
        if(i)uc_line(c,px,py,x,y,2.0f,ACCENT);
        px=x;py=y;
    }
}
static void time_graph(uc_canvas *c,const us_frames *f)
{
    const fl_stats *s=&f->stats;
    us_caption(c,IX,TIME_Y-14,"FRAME TIME OVER PLAY TIME (SHORTEST TO LONGEST IN EACH COLUMN)",TEXT3);
    uc_round_rect(c,(float)GRAPH_X,(float)TIME_Y,(float)GRAPH_W,(float)TIME_H,10.0f,0x08ffffffu);
    /* Up to 2.5 frames of the target cadence, or more when the worst frames need it. */
    unsigned top_us=s->target_refreshes*s->refresh_us*5u/2u,longest=0;
    for(unsigned i=0;i<US_TIME_COLUMNS;++i)if(f->longest[i]>longest)longest=f->longest[i];
    if(longest*10u>top_us)top_us=longest*10u+longest;
    if(top_us>200000u)top_us=200000u;
    if(!top_us)top_us=50000u;
    for(unsigned k=1;k<=3;++k) {
        const unsigned at=k*s->refresh_us;if(at>=top_us)break;
        const int y=TIME_Y+TIME_H-(int)((unsigned long long)at*TIME_H/top_us);char text[16];
        snprintf(text,sizeof text,"%u.%u",(at+50u)/1000u,((at+50u)%1000u)/100u);
        reference(c,y,k==s->target_refreshes?0x40ffffffu:0x22ffffffu);axis_label(c,y,text);
    }
    axis_label(c,TIME_Y+TIME_H,"0 ms");
    const unsigned slow=s->target_refreshes*s->refresh_us+s->refresh_us/2u;
    for(unsigned i=0;i<US_TIME_COLUMNS;++i) {
        if(!f->longest[i])continue;
        const unsigned lo=f->shortest[i]*10u,hi=f->longest[i]*10u;
        const int y0=TIME_Y+TIME_H-(int)((unsigned long long)(lo<top_us?lo:top_us)*TIME_H/top_us);
        const int y1=TIME_Y+TIME_H-(int)((unsigned long long)(hi<top_us?hi:top_us)*TIME_H/top_us);
        uc_rect(c,GRAPH_X+(int)i,y1,1,y0-y1+1,hi>=slow?WARN:ACCENT);
    }
}
static void histogram(uc_canvas *c,const us_frames *f)
{
    const fl_stats *s=&f->stats;
    us_caption(c,SIDE_X,FPS_Y-14,"FRAME TIME HISTOGRAM (LOG SCALE)",TEXT3);
    const int x0=SIDE_X,w=SIDE_W,y0=FPS_Y,h=FPS_H;
    uc_round_rect(c,(float)x0,(float)y0,(float)w,(float)h,10.0f,0x08ffffffu);
    unsigned most=1;
    for(unsigned i=0;i<FL_BINS;++i)if(s->histogram[i]>most)most=s->histogram[i];
    if(s->overflow>most)most=s->overflow;
    /* log2 bars: a bin with one frame still shows. */
    unsigned bits=0;while((1u<<bits)<=most&&bits<31)++bits;
    const int bar=(w-16)/(int)(FL_BINS+1);
    for(unsigned i=0;i<=FL_BINS;++i) {
        const unsigned n=i<FL_BINS?s->histogram[i]:s->overflow;if(!n)continue;
        unsigned b=0;while((1u<<b)<=n&&b<31)++b;
        const int bh=(int)((unsigned long long)(h-30)*b/bits);
        const unsigned upper=(i+1u)*s->bin_us;
        const uint32_t tone=i==FL_BINS?BAD:upper>s->target_refreshes*s->refresh_us+s->refresh_us/2u?WARN:ACCENT;
        uc_rect(c,x0+8+(int)i*bar,y0+h-8-bh,bar>2?bar-1:bar,bh,tone);
    }
    for(unsigned k=1;k<=3;++k) {
        const unsigned at=k*s->refresh_us,bin=s->bin_us?at/s->bin_us:0;if(bin>=FL_BINS)break;
        const int x=x0+8+(int)bin*bar;char text[16];snprintf(text,sizeof text,"%u.%u",(at+50u)/1000u,((at+50u)%1000u)/100u);
        for(int y=y0+6;y<y0+h-8;y+=10)uc_rect(c,x,y,1,5,0x50ffffffu);
        uf_draw(c,UF_CAPTION,x+4,y0+20,text,TEXT3,0);
    }
    char range[64];snprintf(range,sizeof range,"%u.%u ms bins, last bin: %u ms and longer",s->bin_us/1000u,(s->bin_us%1000u)/100u,
                            s->bin_us*FL_BINS/1000u);
    uf_draw_fit(c,UF_CAPTION,x0+4,y0+h+20,w-8,range,TEXT3);
}
static void notes(uc_canvas *c,const us_frames *f)
{
    us_caption(c,SIDE_X,TIME_Y+8,"HOW THIS WAS MEASURED",TEXT3);
    int y=TIME_Y+34;
    for(unsigned i=0;i<3;++i)if(f->basis[i][0]){y+=uf_wrap(c,UF_SMALL,SIDE_X,y,SIDE_W,22,5,f->basis[i],TEXT2)*22+8;}
}
static void footer(uc_canvas *c,const us_frames *f)
{
    const int fy=1024;int x=72;
    if(f->log_count>1){us_dpad(c,(float)x+15.0f,(float)fy,0);x=us_hint(c,x+40,"Older / newer log");}
    us_face_button(c,(float)x+15.0f,(float)fy,US_ICON_SQUARE,f->recorder_available?0xffdfe5efu:0x60ffffffu);
    x=us_hint(c,x+40,!f->recorder_available?"Recording unavailable":f->record_on?"Record next launch: On":"Record next launch: Off");
    us_face_button(c,(float)x+15.0f,(float)fy,US_ICON_TRIANGLE,0xffdfe5efu);x=us_hint(c,x+40,f->record_on?"Launch and record":"Launch");
    us_face_button(c,(float)x+15.0f,(float)fy,US_ICON_CIRCLE,0xffdfe5efu);us_hint(c,x+40,"Back");
}
void uk_draw(uc_canvas *c,const us_model *m)
{
    const us_frames *f=&m->frames;const fl_stats *s=&f->stats;
    us_card(c,CX,CY,CW,CH);
    const int w=uf_draw(c,UF_TITLE,IX,CY+62,"Frame rate",TEXT,0);
    uf_draw_fit(c,UF_TITLE,IX+w+14,CY+62,900,f->game,TEXT2);
    int right=CX+CW-32;
    if(f->log_count){char which[32];snprintf(which,sizeof which,"Log %u of %u",f->log_index+1,f->log_count);right=us_chip(c,right,CY+52,which,0)-12;}
    if(f->state==1)us_chip(c,right,CY+52,f->source,f->engine_source?GOOD:ACCENT);
    if(f->state==1)uf_draw_fit(c,UF_SMALL,IX,CY+98,CW-64,f->session,TEXT2);
    if(f->state!=1) {
        const char *a=f->state==2?"This log could not be read":"No frame-time log for this game yet";
        uf_draw(c,UF_LABEL,CX+CW/2-uf_measure(UF_LABEL,a,0)/2,CY+330,a,TEXT,0);
        const char *how=f->state==2?f->error:f->recorder_available?
            "Turn on Record next launch with Square, then start the game with Triangle. The HelixSR frame logger reads "
            "the game's own frame counter while you play, read-only, and saves the log when the game closes. Logs "
            "converted on the PC from a HelixSR timer recording show here too.":
            "Recording is unavailable. Existing frame-time logs and logs converted on the PC from a HelixSR "
            "timer recording can still be viewed here.";
        uf_wrap(c,UF_BODY,CX+CW/2-420,CY+380,840,30,5,how,TEXT2);
        if(f->status[0])uf_wrap(c,UF_SMALL,CX+CW/2-420,CY+560,840,24,2,f->status,TEXT3);
        footer(c,f);return;
    }
    /* Tiles. */
    const int tiles=7,gap=14,tw=(CW-64-gap*(tiles-1))/tiles;int x=IX-4;
    char value[48],basis[64];
    const char *time_basis=f->frame_time==FL_TIME_CPU_CALIBRATED?"CPU callback cadence":f->frame_time==FL_TIME_MEASURED?"game's own frame time":f->frame_time==FL_TIME_CALIBRATED?
        "GPU clock, calibrated":"GPU clock, nominal (estimate)";
    milli_text(value,sizeof value,s->average_fps_milli,1);
    tile(c,x,tw,value,"AVERAGE FPS",time_basis,TEXT);x+=tw+gap;
    if(s->low1_fps_milli)milli_text(value,sizeof value,s->low1_fps_milli,1);else snprintf(value,sizeof value,UF_DASH);
    tile(c,x,tw,value,"1% LOW FPS",s->low1_fps_milli?"slowest 1% of frames":"needs 100 frames",s->low1_fps_milli*10u<s->average_fps_milli*9u?WARN:TEXT);x+=tw+gap;
    if(s->low01_fps_milli)milli_text(value,sizeof value,s->low01_fps_milli,1);else snprintf(value,sizeof value,UF_DASH);
    tile(c,x,tw,value,"0.1% LOW FPS",s->low01_fps_milli?"slowest 0.1% of frames":"needs 1,000 frames",s->low01_fps_milli*10u<s->average_fps_milli*9u?WARN:TEXT);x+=tw+gap;
    ms_text(value,sizeof value,s->median_us);tile(c,x,tw,value,"MEDIAN FRAME",time_basis,TEXT);x+=tw+gap;
    ms_text(value,sizeof value,s->p99_us);
    snprintf(basis,sizeof basis,"max %u.%u ms",(s->max_us+50u)/1000u,((s->max_us+50u)%1000u)/100u);
    tile(c,x,tw,value,"99TH PCT FRAME",basis,TEXT);x+=tw+gap;
    if(f->frame_time==FL_TIME_CPU_CALIBRATED){
        tile(c,x,tw,UF_DASH,"MISSED REFRESHES","not measured",TEXT3);x+=tw+gap;
    }else{
    snprintf(value,sizeof value,"%u",s->missed);
    snprintf(basis,sizeof basis,"%s, %u.%02u%%",s->missed_measured?"in video":"estimated",
             s->frames?(unsigned)((unsigned long long)s->missed*100u/(s->frames*s->target_refreshes+s->missed)):0u,
             s->frames?(unsigned)((unsigned long long)s->missed*10000u/(s->frames*s->target_refreshes+s->missed)%100u):0u);
    tile(c,x,tw,value,s->missed_measured?"MISSED REFRESHES":"MISSED (ESTIMATE)",basis,s->missed?WARN:GOOD);x+=tw+gap;
    }
    if(s->helix_frames) {
        ms_text(value,sizeof value,s->helix_median_us);
        snprintf(basis,sizeof basis,"GPU, p99 %u.%u ms",(s->helix_p99_us+50u)/1000u,((s->helix_p99_us+50u)%1000u)/100u);
        tile(c,x,tw,value,"HELIXSR BLOCK",basis,TEXT);
    } else if(s->missed_measured) {
        milli_text(value,sizeof value,s->displayed_fps_milli,1);tile(c,x,tw,value,"DISPLAYED FPS","new frames per refresh",TEXT);
    } else {
        snprintf(value,sizeof value,"%u",s->pauses);tile(c,x,tw,value,"PAUSES LEFT OUT","frames over 250 ms",TEXT);
    }
    fps_graph(c,f);time_graph(c,f);histogram(c,f);notes(c,f);
    char play[64];
    snprintf(play,sizeof play,"%llu s of play",(unsigned long long)(f->play_us/1000000u));
    uf_draw_right(c,UF_CAPTION,GRAPH_X+GRAPH_W,TIME_Y+TIME_H+20,play,TEXT3);
    uf_draw(c,UF_CAPTION,GRAPH_X,TIME_Y+TIME_H+20,"0 s",TEXT3,0);
    if(f->status[0])uf_draw_fit(c,UF_SMALL,IX,CY+CH-22,CW-64,f->status,TEXT3);
    footer(c,f);
}
