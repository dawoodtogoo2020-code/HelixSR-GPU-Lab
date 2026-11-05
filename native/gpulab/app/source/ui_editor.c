#include "ui_editor.h"
#include "ui_parts.h"
#include <stdio.h>
#include <string.h>

/* The resolution editor: a modal card over the dimmed main screen. Left, the rows that change the pair; right, a
 * preview and the model checks; below the rows, what exists for the pair (package, game side) and what Triangle will
 * actually run. Every string comes from native_main. */
enum { MX=300, MY=136, MW=1320, MH=808, PAD=40, LEFT_X=MX+PAD, LEFT_W=600, RIGHT_X=MX+PAD+LEFT_W+40,
       RIGHT_W=MX+MW-PAD-(MX+PAD+LEFT_W+40), ROWS_Y=270, ROW_H=50 };

/* A round mark: tick (good), exclamation (warn) or cross (bad). */
static void mark(uc_canvas *c,float cx,float cy,int tone)
{
    const uint32_t colour=us_tone(tone);
    uc_circle(c,cx,cy,11.0f,uc_alpha(colour,0x30));
    if(tone==US_TONE_GOOD) {
        uc_line(c,cx-5.0f,cy+0.5f,cx-1.5f,cy+4.0f,2.2f,colour);uc_line(c,cx-1.5f,cy+4.0f,cx+5.0f,cy-4.0f,2.2f,colour);
    } else if(tone==US_TONE_WARN) {
        uc_line(c,cx,cy-5.0f,cx,cy+1.5f,2.2f,colour);uc_circle(c,cx,cy+5.0f,1.4f,colour);
    } else {
        uc_line(c,cx-4.0f,cy-4.0f,cx+4.0f,cy+4.0f,2.2f,colour);uc_line(c,cx-4.0f,cy+4.0f,cx+4.0f,cy-4.0f,2.2f,colour);
    }
}
static void check_line(uc_canvas *c,int y,int tone,const char *text)
{
    mark(c,(float)RIGHT_X+11.0f,(float)y,tone);
    uf_draw_fit(c,UF_SMALL,RIGHT_X+34,us_centre(UF_SMALL,y),RIGHT_W-34,text,tone==US_TONE_BAD?TEXT:TEXT2);
}
static void row(uc_canvas *c,const us_editor *e,unsigned index,const char *label,const char *value)
{
    const int y=ROWS_Y+(int)index*ROW_H,cy=y+ROW_H/2,focused=e->row==index;
    if(focused) {
        uc_round_rect(c,(float)LEFT_X,(float)y,(float)LEFT_W,(float)ROW_H-4.0f,12.0f,ROW_FOCUS);
        uc_round_rect(c,(float)LEFT_X+8.0f,(float)y+12.0f,4.0f,(float)ROW_H-28.0f,2.0f,ACCENT);
    } else uc_rect(c,LEFT_X+20,y+ROW_H-3,LEFT_W-40,1,DIVIDER);
    uf_draw(c,UF_BODY,LEFT_X+28,us_centre(UF_BODY,cy-2),label,focused?TEXT:TEXT2,0);
    const int right=LEFT_X+LEFT_W-(focused?44:24);
    const int w=uf_draw_right(c,UF_BODY_MEDIUM,right,us_centre(UF_BODY_MEDIUM,cy-2),value,focused?TEXT:0xffd5dbe6u);
    if(focused){us_chevron(c,(float)(right-w-18),(float)cy-2.0f,0,ACCENT);us_chevron(c,(float)(right+18),(float)cy-2.0f,1,ACCENT);}
}
static void preview(uc_canvas *c,const us_editor *e,int top)
{
    const rp_pair *p=&e->pair;
    const float bx=(float)RIGHT_X,by=(float)top,bw=(float)RIGHT_W,bh=150.0f;
    /* One scale for every box, as on the main screen: a 3840 x 2160 frame fills the area. */
    const float k=bw/3840.0f<bh/2160.0f?bw/3840.0f:bh/2160.0f,fw=3840.0f*k,fh=2160.0f*k,fx=bx+(bw-fw)*0.5f,fy=by+(bh-fh)*0.5f;
    uc_round_stroke(c,fx,fy,fw,fh,8.0f,1.0f,0x18ffffffu);
    uf_draw_right(c,UF_CAPTION,(int)(fx+fw)-8,(int)fy+18,"4K",TEXT3);
    if(!p->output_width||!p->output_height||p->output_width>7680||p->output_height>4320)return;
    const float ow=(float)p->output_width*k,oh=(float)p->output_height*k,ox=fx+(fw-ow)*0.5f,oy=fy+(fh-oh)*0.5f;
    const float rw=(float)p->input_width*k,rh=(float)p->input_height*k,rx=ox+(ow-rw)*0.5f,ry=oy+(oh-rh)*0.5f;
    const uint32_t tone=e->check.valid?ACCENT:BAD;
    uc_round_rect(c,ox,oy,ow,oh,8.0f,0x0affffffu);
    uc_round_stroke(c,ox,oy,ow,oh,8.0f,1.5f,0x40ffffffu);
    if(rw>0.0f&&rh>0.0f&&rw<=fw+1.0f&&rh<=fh+1.0f) {
        uc_round_rect(c,rx,ry,rw,rh,6.0f,uc_alpha(tone,0x30));
        uc_round_stroke(c,rx,ry,rw,rh,6.0f,2.0f,tone);
    }
}
void ue_draw(uc_canvas *c,const us_model *m)
{
    const us_editor *e=&m->editor;const rp_check *k=&e->check;const rp_pair *p=&e->pair;
    uc_rect(c,0,0,US_WIDTH,US_HEIGHT,0xc0050810u);
    us_card(c,MX,MY,MW,MH);
    us_focus_ring(c,MX,MY,MW,MH);
    const us_game *g=m->game_count?&m->games[m->selected]:NULL;
    const int w=uf_draw(c,UF_TITLE,LEFT_X,MY+62,"Resolution",TEXT,0);
    uf_draw_fit(c,UF_TITLE,LEFT_X+w+14,MY+62,MW-2*PAD-w-14,g?(g->name[0]?g->name:g->title):"",TEXT2);
    uf_draw_fit(c,UF_SMALL,LEFT_X,MY+98,MW-2*PAD,
        "Render size and output for HelixSR, saved for this game. A pair runs only if the game renders at it and a "
        "package exists for it.",TEXT2);
    char value[UE_ROW_COUNT][72];
    snprintf(value[UE_START],72,"%s",e->start);
    snprintf(value[UE_OUTPUT],72,"%u " UF_TIMES " %u  (%s)",p->output_width,p->output_height,e->output_name);
    snprintf(value[UE_OUTPUT_WIDTH],72,"%u",p->output_width);snprintf(value[UE_OUTPUT_HEIGHT],72,"%u",p->output_height);
    const unsigned scale=p->output_width?(unsigned)(((unsigned long long)p->input_width*1000u+p->output_width/2u)/p->output_width):0u;
    snprintf(value[UE_SCALE],72,"%u.%u%%",scale/10u,scale%10u);
    snprintf(value[UE_INPUT_WIDTH],72,"%u",p->input_width);snprintf(value[UE_INPUT_HEIGHT],72,"%u",p->input_height);
    static const char *const labels[UE_ROW_COUNT]={"Start from","Output","Output width","Output height","Render scale",
                                                     "Render width","Render height"};
    for(unsigned i=0;i<UE_ROW_COUNT;++i)row(c,e,i,labels[i],value[i]);
    /* What exists for this pair, and what Triangle runs. */
    int y=ROWS_Y+UE_ROW_COUNT*ROW_H+22;
    uf_draw_fit(c,UF_SMALL,LEFT_X+4,y,LEFT_W,e->saved_text,e->refused?BAD:e->saved?GOOD:TEXT3);
    y+=34;us_caption(c,LEFT_X+4,y,"HELIXSR PACKAGE FOR THIS PAIR",TEXT3);y+=26;
    const int package_tone=e->package==UE_PACKAGE_READY?US_TONE_GOOD:e->package==UE_PACKAGE_NONE?US_TONE_BAD:US_TONE_WARN;
    mark(c,(float)LEFT_X+15.0f,(float)y,package_tone);
    uf_wrap(c,UF_BODY_MEDIUM,LEFT_X+38,us_centre(UF_BODY_MEDIUM,y),LEFT_W-38,26,2,e->package_text,TEXT);
    y+=64;us_caption(c,LEFT_X+4,y,"GAME SIDE",TEXT3);y+=26;
    mark(c,(float)LEFT_X+15.0f,(float)y,e->game_measured?US_TONE_GOOD:US_TONE_BAD);
    uf_wrap(c,UF_BODY_MEDIUM,LEFT_X+38,us_centre(UF_BODY_MEDIUM,y),LEFT_W-38,26,2,e->game_text,TEXT);
    y+=60;
    uc_rect(c,LEFT_X+4,y-18,LEFT_W-8,1,DIVIDER);
    uf_wrap(c,UF_SMALL,LEFT_X+4,y+8,LEFT_W-8,24,3,e->launch_text,TEXT2);
    /* Preview and model checks. */
    us_caption(c,RIGHT_X,ROWS_Y+16,"PREVIEW",TEXT3);
    preview(c,e,ROWS_Y+30);
    char render[40],output[40];
    snprintf(render,sizeof render,"%u " UF_TIMES " %u",p->input_width,p->input_height);
    snprintf(output,sizeof output,"%u " UF_TIMES " %u",p->output_width,p->output_height);
    const int line=ROWS_Y+220,rw=uf_draw(c,UF_BODY_MEDIUM,RIGHT_X,line,render,TEXT,0);
    us_arrow(c,(float)(RIGHT_X+rw+14),(float)line-8.0f,26.0f,TEXT3);
    uf_draw(c,UF_BODY_MEDIUM,RIGHT_X+rw+54,line,output,TEXT,0);
    char factor[32],second[32],pixels[16];
    us_scale_text(factor,sizeof factor,p->output_width,p->input_width);
    us_scale_text(second,sizeof second,p->output_height,p->input_height);
    snprintf(pixels,sizeof pixels,"%u%%",(k->pixels_permille+5u)/10u);
    const int stats=ROWS_Y+274,column=RIGHT_X+RIGHT_W/2+12;
    if(strcmp(factor,second)){char both[64];snprintf(both,sizeof both,"%s / %s",factor,second);uf_draw(c,UF_TITLE,RIGHT_X,stats-2,both,TEXT,0);}
    else uf_draw(c,UF_STAT,RIGHT_X,stats,factor,TEXT,0);
    us_caption(c,RIGHT_X,stats+22,strcmp(factor,second)?"UPSCALE W / H":"UPSCALE PER AXIS",TEXT3);
    uf_draw(c,UF_STAT,column,stats,pixels,TEXT,0);
    us_caption(c,column,stats+22,"PIXELS RENDERED",TEXT3);
    int cy=ROWS_Y+342;
    /* Two groups: what decides whether the pair can be saved, and what a package for it would need. A missing port
     * is amber, not red: it does not block saving a request. */
    us_caption(c,RIGHT_X,cy,"CAN BE SAVED",TEXT3);cy+=28;
    char text[160];
    if(k->limits_ok)snprintf(text,sizeof text,"Size: output 720p to 3840 " UF_TIMES " 2160, render 640 " UF_TIMES " 360 to the output");
    else snprintf(text,sizeof text,"%s",k->problem);
    check_line(c,cy,k->limits_ok?US_TONE_GOOD:US_TONE_BAD,text);cy+=34;
    snprintf(text,sizeof text,"Shape: render and output %u.%u%% apart (at most 1%%)",k->aspect_permille/10u,k->aspect_permille%10u);
    check_line(c,cy,k->aspect_permille<=RP_ASPECT_PERMILLE?US_TONE_GOOD:US_TONE_BAD,text);cy+=42;
    us_caption(c,RIGHT_X,cy,"WHAT A PACKAGE FOR IT NEEDS (HELIXSR 1.8.1, PS5 PORT)",TEXT3);cy+=28;
    check_line(c,cy,k->ultra_network?US_TONE_WARN:US_TONE_GOOD,k->ultra_network?
        "Ultra Performance network (over 2.5" UF_TIMES " wide): not ported to PS5":"Main network (NCHW8): ported to PS5");cy+=34;
    check_line(c,cy,k->kernel==RP_KERNEL_FAST?US_TONE_GOOD:US_TONE_WARN,k->kernel==RP_KERNEL_FAST?
        "Output kernel dfn_fastpath (scale 1.7" UF_TIMES " or more): ported (k5f)":k->kernel==RP_KERNEL_MID?
        "Output kernel dfn_mid (scale 1.49-1.7" UF_TIMES "): not ported to PS5":"Output kernel dfn_3x3_256 (under 1.49" UF_TIMES "): not ported to PS5");
    cy+=34;
    check_line(c,cy,k->motion_ok?US_TONE_GOOD:US_TONE_WARN,k->motion_ok?"Motion helper: scale between 1 and 2, exact pixel mapping":
        "Motion helper: needs a scale between 1 and 2 (exact mapping)");cy+=34;
    snprintf(text,sizeof text,k->tiles_exact?"Network %u " UF_TIMES " %u: the r17 encoder tiles divide it":
             "Network %u " UF_TIMES " %u: needs the encoder edge guards (4K work)",k->network_width,k->network_height);
    check_line(c,cy,k->tiles_exact?US_TONE_GOOD:US_TONE_WARN,text);
    /* Footer. */
    const int fy=1024;int x=72;
    us_dpad(c,(float)x+15.0f,(float)fy,1);x=us_hint(c,x+40,"Row");
    us_dpad(c,(float)x+15.0f,(float)fy,0);x=us_hint(c,x+40,"Change (hold to repeat)");
    x+=us_key_cap(c,x,fy,"L1")+8;x+=us_key_cap(c,x,fy,"R1")+12;x=us_hint(c,x,"Bigger steps");
    us_face_button(c,(float)x+15.0f,(float)fy,US_ICON_CROSS,k->valid?0xffdfe5efu:0x60ffffffu);
    x=us_hint(c,x+40,k->valid?"Save for this game":"Save (fix size or shape first)");
    us_face_button(c,(float)x+15.0f,(float)fy,US_ICON_CIRCLE,0xffdfe5efu);us_hint(c,x+40,"Cancel");
}
