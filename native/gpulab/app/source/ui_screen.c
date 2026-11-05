#include "ui_screen.h"
#include "ui_canvas.h"
#include "ui_font.h"
#include "ui_parts.h"
#include "ui_editor.h"
#include <stdio.h>
#include <string.h>

/* Layout, in 1920x1080 pixels. */
enum { MARGIN=72, TOP=148, BOTTOM=960, LIB_X=72, LIB_W=468, MAIN_X=572, MAIN_W=764, SIDE_X=1368, SIDE_W=480,
       ROW_X=588, ROW_W=732, ROW_H=48, LABEL_X=616, VALUE_R=1296, ITEM_H=84, ITEM_STEP=90, ITEMS_SHOWN=8,
       LIST_Y=212, SETTINGS_Y=280, UPSCALER_H=230, RESOLUTION_Y=396, RESOLUTION_H=290, STATUS_Y=704 };

static int centre(uf_face face,int cy) { return cy+(uf_cap(face)+1)/2; }
/* Cards are drawn once into the background; focus adds a ring over one. */
static void card(uc_canvas *c,int x,int y,int w,int h)
{
    uc_shadow(c,(float)x,(float)y+12.0f,(float)w,(float)h,22.0f,30.0f,0x70000000u);
    uc_round_rect(c,(float)x,(float)y,(float)w,(float)h,22.0f,CARD);
    uc_round_stroke(c,(float)x,(float)y,(float)w,(float)h,22.0f,1.0f,CARD_EDGE);
}
static void focus_ring(uc_canvas *c,int x,int y,int w,int h)
{
    uc_outer_glow(c,(float)x,(float)y,(float)w,(float)h,22.0f,18.0f,uc_alpha(ACCENT,0x40));
    uc_round_stroke(c,(float)x,(float)y,(float)w,(float)h,22.0f,2.0f,uc_alpha(ACCENT,0xc0));
}
static void caption(uc_canvas *c,int x,int baseline,const char *text,uint32_t color)
{
    uf_draw(c,UF_CAPTION,x,baseline,text,color,2);
}
static void chevron(uc_canvas *c,float x,float cy,int right,uint32_t color)
{
    float tip=right?x+5.0f:x-5.0f,tail=right?x-2.0f:x+2.0f;
    uc_line(c,tail,cy-7.0f,tip,cy,2.6f,color);
    uc_line(c,tip,cy,tail,cy+7.0f,2.6f,color);
}
static void arrow(uc_canvas *c,float x,float cy,float length,uint32_t color)
{
    uc_line(c,x,cy,x+length,cy,2.4f,color);
    uc_line(c,x+length-7.0f,cy-6.0f,x+length,cy,2.4f,color);
    uc_line(c,x+length-7.0f,cy+6.0f,x+length,cy,2.4f,color);
}
/* PlayStation face buttons as glyphs on a light disc. */
enum { ICON_CROSS=0, ICON_CIRCLE, ICON_TRIANGLE, ICON_SQUARE };
static void face_button(uc_canvas *c,float cx,float cy,int icon,uint32_t disc)
{
    uc_circle(c,cx,cy,15.0f,disc);
    switch(icon) {
    case ICON_CROSS:
        uc_line(c,cx-5.5f,cy-5.5f,cx+5.5f,cy+5.5f,2.4f,INK);uc_line(c,cx-5.5f,cy+5.5f,cx+5.5f,cy-5.5f,2.4f,INK);break;
    case ICON_CIRCLE: uc_ring(c,cx,cy,7.0f,2.4f,INK);break;
    case ICON_TRIANGLE:
        uc_line(c,cx,cy-6.5f,cx+7.0f,cy+5.5f,2.2f,INK);uc_line(c,cx+7.0f,cy+5.5f,cx-7.0f,cy+5.5f,2.2f,INK);
        uc_line(c,cx-7.0f,cy+5.5f,cx,cy-6.5f,2.2f,INK);break;
    default: uc_round_stroke(c,cx-6.0f,cy-6.0f,12.0f,12.0f,1.5f,2.2f,INK);break;
    }
}
/* D-pad with the active axis lit. */
static void dpad(uc_canvas *c,float cx,float cy,int vertical)
{
    uc_round_rect(c,cx-5.0f,cy-14.0f,10.0f,28.0f,3.0f,0x50ffffffu);
    uc_round_rect(c,cx-14.0f,cy-5.0f,28.0f,10.0f,3.0f,0x50ffffffu);
    if(vertical){uc_round_rect(c,cx-5.0f,cy-14.0f,10.0f,9.0f,3.0f,TEXT);uc_round_rect(c,cx-5.0f,cy+5.0f,10.0f,9.0f,3.0f,TEXT);}
    else{uc_round_rect(c,cx-14.0f,cy-5.0f,9.0f,10.0f,3.0f,TEXT);uc_round_rect(c,cx+5.0f,cy-5.0f,9.0f,10.0f,3.0f,TEXT);}
}
/* A labelled key cap such as L1 or OPTIONS; returns its width. */
static int key_cap(uc_canvas *c,int x,int cy,const char *label)
{
    int w=uf_measure(UF_CAPTION,label,1)+20;
    uc_round_stroke(c,(float)x,(float)cy-14.0f,(float)w,28.0f,8.0f,1.6f,0xa0ffffffu);
    uf_draw(c,UF_CAPTION,x+10,centre(UF_CAPTION,cy),label,TEXT,1);
    return w;
}
/* Pill with a status dot; right aligned at x; returns its left edge. */
static int chip(uc_canvas *c,int right,int cy,const char *text,uint32_t dot)
{
    int w=uf_measure(UF_BODY_MEDIUM,text,0)+40+(dot?18:0),x=right-w;
    uc_round_rect(c,(float)x,(float)cy-22.0f,(float)w,44.0f,22.0f,0x0dffffffu);
    uc_round_stroke(c,(float)x,(float)cy-22.0f,(float)w,44.0f,22.0f,1.0f,0x1affffffu);
    if(dot)uc_circle(c,(float)x+26.0f,(float)cy,5.0f,dot);
    uf_draw(c,UF_BODY_MEDIUM,x+20+(dot?18:0),centre(UF_BODY_MEDIUM,cy),text,TEXT,0);
    return x;
}
void us_draw_mark(uc_canvas *c,float x,float y,float size)
{
    const float k=size/60.0f;
    uc_shadow(c,x,y+6.0f*k,size,size,18.0f*k,20.0f*k,uc_alpha(ACCENT,0x50));
    uc_round_gradient(c,x,y,size,size,18.0f*k,ACCENT,ACCENT2);
    /* An upright double helix, open at both ends, with rungs: the HelixSR mark. */
    const float y0=y+13.0f*k,length=34.0f*k,cx=x+30.0f*k,amplitude=11.0f*k,turns=1.0f,start=0.25f;
    const int steps=36;
    for(int i=2;i<steps;i+=4) {
        float t=(float)i/(float)steps,py=y0+length*t,s=uc_sin_turn(turns*t+start);
        if(s*s>0.2f)uc_line(c,cx+amplitude*s,py,cx-amplitude*s,py,2.2f*k,0x8cffffffu);
    }
    for(int strand=1;strand>=0;--strand)for(int i=0;i<steps;++i) {
        float a=(float)i/(float)steps,b=(float)(i+1)/(float)steps,phase=start+(strand?0.5f:0.0f);
        uc_line(c,cx+amplitude*uc_sin_turn(turns*a+phase),y0+length*a,cx+amplitude*uc_sin_turn(turns*b+phase),y0+length*b,
                3.6f*k,strand?0xa8ffffffu:0xffffffffu);
    }
}
static void draw_background(uc_canvas *c)
{
    uc_vgradient(c,0,0,US_WIDTH,US_HEIGHT,BG_TOP,BG_BOTTOM);
    uc_glow(c,240.0f,-160.0f,980.0f,uc_alpha(ACCENT,0x2c));
    uc_glow(c,1780.0f,1220.0f,960.0f,uc_alpha(ACCENT2,0x22));
    uc_glow(c,1460.0f,-260.0f,620.0f,uc_alpha(ACCENT2,0x12));
    us_draw_mark(c,(float)MARGIN,50.0f,60.0f);
    int w=uf_draw(c,UF_TITLE,MARGIN+80,centre(UF_TITLE,80),"HelixSR",TEXT,0);
    uf_draw(c,UF_TITLE,MARGIN+80+w+12,centre(UF_TITLE,80),"GPU Lab",TEXT2,0);
    card(c,LIB_X,TOP,LIB_W,BOTTOM-TOP);
    card(c,MAIN_X,SETTINGS_Y,MAIN_W,BOTTOM-SETTINGS_Y);
    card(c,SIDE_X,TOP,SIDE_W,UPSCALER_H);
    card(c,SIDE_X,RESOLUTION_Y,SIDE_W,RESOLUTION_H);
    card(c,SIDE_X,STATUS_Y,SIDE_W,BOTTOM-STATUS_Y);
}
static uint32_t monogram_colour(const char *title,int bright)
{
    static const uint32_t dark[6]={0xff23427fu,0xff3e3285u,0xff185a61u,0xff6e4a1au,0xff76293fu,0xff1b5c3cu};
    static const uint32_t light[6]={0xff4d9dffu,0xff8b73ffu,0xff35c2b5u,0xfff3b44bu,0xffff6f91u,0xff3fd59au};
    unsigned hash=2166136261u;
    for(const unsigned char *p=(const unsigned char *)title;*p;++p)hash=(hash^*p)*16777619u;
    return bright?light[hash%6u]:dark[hash%6u];
}
static void initials(const char *name,char out[3])
{
    unsigned n=0;int word_start=1;
    for(const char *p=name;*p&&n<2;++p) {
        char ch=*p;int alnum=(ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9');
        if(alnum&&word_start){out[n++]=ch>='a'&&ch<='z'?(char)(ch-32):ch;word_start=0;}
        else if(!alnum)word_start=1;
    }
    if(n==1&&name[0]&&name[1]) {
        char ch=name[1];
        if((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z'))out[n++]=ch>='a'&&ch<='z'?(char)(ch-32):ch;
    }
    if(!n)out[n++]='?';
    out[n]='\0';
}
static void library(uc_canvas *c,const us_model *m)
{
    const int focused=m->menu==US_MENU_LIBRARY&&m->game_count;
    if(focused)focus_ring(c,LIB_X,TOP,LIB_W,BOTTOM-TOP);
    caption(c,LIB_X+28,TOP+40,"GAME LIBRARY",TEXT3);
    char count[32];snprintf(count,sizeof count,m->game_count==1?"%u game":"%u games",m->game_count);
    uf_draw_right(c,UF_SMALL,LIB_X+LIB_W-28,TOP+40,count,TEXT3);
    if(!m->game_count) {
        const int cx=LIB_X+LIB_W/2;
        uc_round_stroke(c,(float)cx-34.0f,360.0f,68.0f,68.0f,20.0f,2.0f,0x40ffffffu);
        uc_line(c,(float)cx-12.0f,394.0f,(float)cx+12.0f,394.0f,2.6f,TEXT3);
        uc_line(c,(float)cx,382.0f,(float)cx,406.0f,2.6f,TEXT3);
        const char *title="No games in the catalog";
        uf_draw(c,UF_LABEL,cx-uf_measure(UF_LABEL,title,0)/2,478,title,TEXT,0);
        uf_wrap(c,UF_SMALL,LIB_X+48,516,LIB_W-96,27,4,"Add a verified game entry to /data/helixsr-speed/catalog.tsv, then reopen GPU Lab.",TEXT2);
        return;
    }
    unsigned first=0;
    if(m->game_count>ITEMS_SHOWN) {
        first=m->selected>ITEMS_SHOWN/2?m->selected-ITEMS_SHOWN/2:0;
        if(first>m->game_count-ITEMS_SHOWN)first=m->game_count-ITEMS_SHOWN;
    }
    for(unsigned i=first;i<m->game_count&&i<first+ITEMS_SHOWN;++i) {
        const us_game *g=&m->games[i];
        const int y=LIST_Y+(int)(i-first)*ITEM_STEP,x=LIB_X+16,w=LIB_W-32,chosen=i==m->selected;
        if(chosen) {
            uc_round_rect(c,(float)x,(float)y,(float)w,(float)ITEM_H,16.0f,ROW_FOCUS);
            if(focused)uc_round_stroke(c,(float)x,(float)y,(float)w,(float)ITEM_H,16.0f,2.0f,ACCENT);
            else uc_round_rect(c,(float)x,(float)y+22.0f,4.0f,40.0f,2.0f,ACCENT);
        }
        char letters[3];initials(g->name[0]?g->name:g->title,letters);
        const uint32_t tile=g->kind==US_INSTALLED?0xff1c2436u:monogram_colour(g->title,0);
        uc_round_gradient(c,(float)x+16.0f,(float)y+14.0f,56.0f,56.0f,16.0f,tile,
                          uc_mix(tile,g->kind==US_INSTALLED?0xff2a3550u:monogram_colour(g->title,1),chosen?150u:70u));
        uf_draw(c,UF_BODY_MEDIUM,x+44-uf_measure(UF_BODY_MEDIUM,letters,0)/2,centre(UF_BODY_MEDIUM,y+42),letters,
                g->kind==US_INSTALLED?TEXT2:TEXT,0);
        const int text_x=x+88,text_w=w-88-(chosen&&focused?52:20);
        uf_draw_fit(c,UF_LABEL,text_x,y+38,text_w,g->name[0]?g->name:g->title,chosen?TEXT:TEXT2);
        /* Whether HelixSR can run in this game, in green when it can. */
        const char *found=g->ready?"HelixSR ready":"HelixSR not available yet";
        const int found_w=uf_draw(c,UF_SMALL,text_x,y+64,found,g->ready?GOOD:TEXT3,0);
        char meta[160];snprintf(meta,sizeof meta,"  " UF_MIDDOT "  v%s",g->build);
        uf_draw_fit(c,UF_SMALL,text_x+found_w,y+64,text_w-found_w,meta,TEXT3);
        if(chosen&&focused){chevron(c,(float)(x+w-36),(float)y+42.0f,0,ACCENT);chevron(c,(float)(x+w-18),(float)y+42.0f,1,ACCENT);}
    }
    if(m->game_count>ITEMS_SHOWN) {
        const float track=(float)(ITEMS_SHOWN*ITEM_STEP-6),top=(float)LIST_Y;
        const float thumb=track*(float)ITEMS_SHOWN/(float)m->game_count;
        const float at=top+(track-thumb)*(float)first/(float)(m->game_count-ITEMS_SHOWN);
        uc_round_rect(c,(float)(LIB_X+LIB_W-10),top,4.0f,track,2.0f,0x14ffffffu);
        uc_round_rect(c,(float)(LIB_X+LIB_W-10),at,4.0f,thumb,2.0f,0x60ffffffu);
    }
}
static const char *output_tag(unsigned w,unsigned h)
{
    if(w==1280&&h==720)return "720p";
    if(w==1920&&h==1080)return "1080p";
    if(w==2560&&h==1440)return "1440p";
    if(w==3840&&h==2160)return "4K";
    return "Custom";
}
static void hero(uc_canvas *c,const us_model *m)
{
    const us_game *g=m->game_count?&m->games[m->selected]:NULL;
    const int button_w=252,button_x=MAIN_X+MAIN_W-button_w,button_y=162,button_h=66;
    /* Long names step down a size before they are ellipsised. */
    const char *name=g?(g->name[0]?g->name:g->title):"No game selected";
    const int name_w=button_x-MAIN_X-28;
    if(uf_measure(UF_HERO,name,0)<=name_w)uf_draw(c,UF_HERO,MAIN_X,206,name,TEXT,0);
    else uf_draw_fit(c,UF_STAT,MAIN_X,202,name_w,name,TEXT);
    int x=MAIN_X;
    if(g) {
        char meta[200];
        snprintf(meta,sizeof meta,"%s  " UF_MIDDOT "  v%s  " UF_MIDDOT "  %s",g->title,g->build,m->pro?"PS5 Pro":"PS5");
        x+=uf_draw_fit(c,UF_BODY,MAIN_X,250,MAIN_W-260,meta,TEXT2)+22;
        const int installed=g->kind==US_INSTALLED;
        const char *state=installed?"Not scanned":!m->settings_ok?"Settings unavailable":m->hook_ready?(m->helix_on?"HelixSR on":"HelixSR off"):"HelixSR not available yet";
        const uint32_t tone=installed?TEXT2:!m->settings_ok?BAD:m->hook_ready?GOOD:TEXT2;
        const int w=uf_measure(UF_SMALL,state,0)+40;
        uc_round_rect(c,(float)x,226.0f,(float)w,34.0f,17.0f,uc_alpha(tone,0x26));
        uc_circle(c,(float)x+16.0f,243.0f,4.5f,tone);
        uf_draw(c,UF_SMALL,x+28,centre(UF_SMALL,243),state,tone,0);
    }
    /* Triangle launches from anywhere; the button mirrors launch_selected's gate.
     * An installed, uncatalogued game launches without a profile. */
    /* ON launches only when a HelixSR package for this game is on the console (hook_ready). */
    const int ready=g&&(m->settings_ok||g->kind==US_INSTALLED)&&(!m->helix_on||m->hook_ready);
    const char *label=m->unsaved&&!m->helix_on?"Save and launch":m->scan_launch?"Launch and scan":"Launch game";
    if(ready) {
        uc_shadow(c,(float)button_x,(float)button_y+8.0f,(float)button_w,(float)button_h,33.0f,22.0f,uc_alpha(ACCENT,0x60));
        uc_round_gradient(c,(float)button_x,(float)button_y,(float)button_w,(float)button_h,33.0f,ACCENT,ACCENT2);
    } else {
        uc_round_rect(c,(float)button_x,(float)button_y,(float)button_w,(float)button_h,33.0f,0x12ffffffu);
        uc_round_stroke(c,(float)button_x,(float)button_y,(float)button_w,(float)button_h,33.0f,1.0f,0x1affffffu);
    }
    const int cy=button_y+button_h/2;
    face_button(c,(float)button_x+38.0f,(float)cy,ICON_TRIANGLE,ready?0xffffffffu:0x60ffffffu);
    if(g&&m->settings_ok&&m->helix_on) {
        uf_draw(c,UF_BODY_MEDIUM,button_x+64,cy-2,label,m->hook_ready?0xffffffffu:TEXT2,0);
        uf_draw(c,UF_CAPTION,button_x+64,cy+20,m->hook_ready?"WITH HELIXSR":"TURN HELIXSR OFF",m->hook_ready?0xffffffffu:TEXT3,1);
    } else uf_draw(c,UF_BODY_MEDIUM,button_x+64,centre(UF_BODY_MEDIUM,cy),label,ready?0xffffffffu:TEXT3,0);
}
/* The HelixSR row: what runs in the game. Cross switches it: ON = the HelixSR package, OFF = the game's own upscaler. */
static void setting_row(uc_canvas *c,const us_model *m,int y)
{
    const int focused=m->menu==US_MENU_SETTINGS&&m->game_count&&m->focus==US_HELIX,cy=y+ROW_H/2,usable=m->settings_ok&&!m->blank;
    if(focused) {
        uc_round_rect(c,(float)ROW_X,(float)y,(float)ROW_W,(float)ROW_H,12.0f,ROW_FOCUS);
        uc_round_rect(c,(float)ROW_X+8.0f,(float)y+12.0f,4.0f,(float)ROW_H-24.0f,2.0f,ACCENT);
    }
    uf_draw(c,UF_BODY,LABEL_X,centre(UF_BODY,cy),"HelixSR",!usable?TEXT3:focused?TEXT:0xffd5dbe6u,0);
    if(!usable||!m->hook_ready) {
        uf_draw_right(c,UF_BODY_MEDIUM,VALUE_R,centre(UF_BODY_MEDIUM,cy),m->blank?UF_DASH:"Not available yet",TEXT3);
        return;
    }
    const int w=uf_draw_right(c,UF_BODY_MEDIUM,VALUE_R,centre(UF_BODY_MEDIUM,cy),m->helix_on?"On":"Off: game's own upscaler",focused?TEXT:TEXT2);
    uc_circle(c,(float)(VALUE_R-w-16),(float)cy,5.0f,m->helix_on?GOOD:TEXT3);
}
/* The HelixSR HUD row: the FPS box the game's hook draws. Cross switches it; in the game L3 + R3 does the same. */
static void hud_row(uc_canvas *c,const us_model *m,int y)
{
    const int focused=m->menu==US_MENU_SETTINGS&&m->game_count&&m->focus==US_HUD,cy=y+ROW_H/2,usable=m->hud_available&&!m->blank;
    if(focused) {
        uc_round_rect(c,(float)ROW_X,(float)y,(float)ROW_W,(float)ROW_H,12.0f,ROW_FOCUS);
        uc_round_rect(c,(float)ROW_X+8.0f,(float)y+12.0f,4.0f,(float)ROW_H-24.0f,2.0f,ACCENT);
    }
    uf_draw(c,UF_BODY,LABEL_X,centre(UF_BODY,cy),"HelixSR HUD",!usable?TEXT3:focused?TEXT:0xffd5dbe6u,0);
    if(!usable) {
        uf_draw_right(c,UF_BODY_MEDIUM,VALUE_R,centre(UF_BODY_MEDIUM,cy),m->blank?UF_DASH:"Not available for this game",TEXT3);
        return;
    }
    const int w=uf_draw_right(c,UF_BODY_MEDIUM,VALUE_R,centre(UF_BODY_MEDIUM,cy),m->hud_on?"On: FPS box in the game":"Off",focused?TEXT:TEXT2);
    uc_circle(c,(float)(VALUE_R-w-16),(float)cy,5.0f,m->hud_on?GOOD:TEXT3);
}
/* A row that opens a screen: label, value and a chevron, highlighted like the HelixSR row when focused. */
static void tool_row(uc_canvas *c,const us_model *m,int y,unsigned field,const char *label,const char *value,int tone,int divider)
{
    const int focused=m->menu==US_MENU_SETTINGS&&m->game_count&&m->focus==field,cy=y+ROW_H/2;
    if(focused) {
        uc_round_rect(c,(float)ROW_X,(float)y,(float)ROW_W,(float)ROW_H,12.0f,ROW_FOCUS);
        uc_round_rect(c,(float)ROW_X+8.0f,(float)y+12.0f,4.0f,(float)ROW_H-24.0f,2.0f,ACCENT);
    } else if(divider)uc_rect(c,LABEL_X,y+ROW_H,VALUE_R-LABEL_X,1,DIVIDER);
    uf_draw(c,UF_BODY,LABEL_X,centre(UF_BODY,cy),label,m->blank?TEXT3:focused?TEXT:0xffd5dbe6u,0);
    if(m->blank){uf_draw_right(c,UF_BODY_MEDIUM,VALUE_R,centre(UF_BODY_MEDIUM,cy),UF_DASH,TEXT3);return;}
    chevron(c,(float)VALUE_R-4.0f,(float)cy,1,focused?ACCENT:TEXT3);
    const int right=VALUE_R-22,left=LABEL_X+190;
    const int w=uf_measure(UF_BODY_MEDIUM,value,0)<right-left-20?uf_draw_right(c,UF_BODY_MEDIUM,right,centre(UF_BODY_MEDIUM,cy),value,focused?TEXT:TEXT2):
        (uf_draw_fit(c,UF_BODY_MEDIUM,left+20,centre(UF_BODY_MEDIUM,cy),right-left-20,value,focused?TEXT:TEXT2),right-left-20);
    if(tone)uc_circle(c,(float)(right-w-14),(float)cy,5.0f,us_tone(tone));
}
static void info_row(uc_canvas *c,int y,const char *label,const char *value,int divider)
{
    const int cy=y+ROW_H/2;
    if(divider)uc_rect(c,LABEL_X,y+ROW_H,VALUE_R-LABEL_X,1,DIVIDER);
    uf_draw(c,UF_BODY,LABEL_X,centre(UF_BODY,cy),label,TEXT2,0);
    uf_draw_fit(c,UF_BODY_MEDIUM,LABEL_X+250,centre(UF_BODY_MEDIUM,cy),VALUE_R-LABEL_X-250,value,TEXT2);
}
static void settings(uc_canvas *c,const us_model *m)
{
    int y=SETTINGS_Y+24;
    if(m->menu==US_MENU_SETTINGS&&m->game_count)focus_ring(c,MAIN_X,SETTINGS_Y,MAIN_W,BOTTOM-SETTINGS_Y);
    caption(c,LABEL_X,y+16,"UPSCALER",TEXT3);
    uf_draw_right(c,UF_SMALL,VALUE_R,y+16,m->scan_only?"No HelixSR switch for this game":"Cross switches HelixSR ON / OFF",TEXT3);
    y+=30;setting_row(c,m,y);y+=ROW_H+14;
    caption(c,LABEL_X,y+16,"IN-GAME HUD",TEXT3);
    uf_draw_right(c,UF_SMALL,VALUE_R,y+16,m->hud_available&&!m->blank?
                  "Cross switches it  " UF_MIDDOT "  in the game: hold L3 + R3":"Built-in HUD unavailable for this game",TEXT3);
    y+=30;hud_row(c,m,y);y+=ROW_H+14;
    caption(c,LABEL_X,y+16,"HELIXSR PACKAGE",TEXT3);
    uf_draw_right(c,UF_SMALL,VALUE_R,y+16,"Read-only: what the package on the console runs",TEXT3);
    y+=30;
    char render[48],output[48];
    if(m->hook_ready&&m->input_width&&m->output_width&&m->output2_width) {
        /* u07: a package with two layouts (r18: Performance and Cinematic). */
        snprintf(render,sizeof render,"%u " UF_TIMES " %u  /  %u " UF_TIMES " %u",m->input_width,m->input_height,m->input2_width,m->input2_height);
        if(m->output_width==m->output2_width&&m->output_height==m->output2_height)
            snprintf(output,sizeof output,"%u " UF_TIMES " %u  (%s)",m->output_width,m->output_height,output_tag(m->output_width,m->output_height));
        else
            snprintf(output,sizeof output,"%u " UF_TIMES " %u  /  %u " UF_TIMES " %u",m->output_width,m->output_height,m->output2_width,m->output2_height);
    } else if(m->hook_ready&&m->input_width&&m->output_width) {
        snprintf(render,sizeof render,"%u " UF_TIMES " %u",m->input_width,m->input_height);
        snprintf(output,sizeof output,"%u " UF_TIMES " %u  (%s)",m->output_width,m->output_height,
                 output_tag(m->output_width,m->output_height));
    } else {snprintf(render,sizeof render,UF_DASH);snprintf(output,sizeof output,UF_DASH);}
    info_row(c,y,"Build",m->hook_ready&&m->package[0]?m->package:"No HelixSR package for this game",1);y+=ROW_H;
    info_row(c,y,m->selected<m->game_count&&!strcmp(m->games[m->selected].title,"PPSA07631")?"Render examples":"Render resolution",render,1);y+=ROW_H;
    info_row(c,y,"Output resolution",output,0);y+=ROW_H+14;
    /* u07: the per-game resolution request and the frame-rate log; Cross opens their screens. */
    caption(c,LABEL_X,y+16,"RESOLUTION REQUEST AND FRAME RATE",TEXT3);
    uf_draw_right(c,UF_SMALL,VALUE_R,y+16,"Cross opens",TEXT3);
    y+=30;tool_row(c,m,y,US_RESOLUTION,"Resolution",m->resolution_value,m->resolution_tone,1);y+=ROW_H;
    tool_row(c,m,y,US_FRAMES,"Frame rate",m->frames_value,m->frames_tone,0);
}
static void scale_text(char *out,size_t size,unsigned output,unsigned input)
{
    unsigned hundredths=input?(output*100u+input/2u)/input:0u;
    snprintf(out,size,"%u.%02u" UF_TIMES,hundredths/100u,hundredths%100u);
}
/* What the scan or a measurement found, the HelixSR route, and the hook. */
static void upscaler_card(uc_canvas *c,const us_model *m)
{
    const int top=TOP,x=SIDE_X+28,w=SIDE_W-56;
    const us_game *g=m->game_count?&m->games[m->selected]:NULL;
    caption(c,x,top+40,m->scan_only?"UPSCALER CANDIDATE":"GAME UPSCALER",TEXT3);
    if(g&&g->kind==US_KNOWN&&m->basis[0])uf_draw_right(c,UF_SMALL,x+w,top+40,m->basis,TEXT3);
    if(!g){uf_draw(c,UF_TITLE,x,top+84,"No game selected",TEXT3,0);return;}
    if(g->kind==US_KNOWN) {
        const int found=uf_draw(c,UF_TITLE,x,top+84,g->upscaler,TEXT,0);
        if(m->engine[0]&&strcmp(m->engine,"Unknown")) {
            char engine[48];snprintf(engine,sizeof engine,"  " UF_MIDDOT "  %s",m->engine);
            uf_draw_fit(c,UF_BODY,x+found,top+84,w-found,engine,TEXT2);
        }
    } else uf_draw(c,UF_TITLE,x,top+84,"Not scanned yet",TEXT2,0);
    caption(c,x,top+124,"HELIXSR ROUTE",TEXT3);
    uf_draw_fit(c,UF_BODY_MEDIUM,x,top+152,w,m->route,g->kind==US_KNOWN?TEXT:TEXT2);
    caption(c,x,top+188,"RUNTIME HOOK",TEXT3);
    const uint32_t tone=m->hook_ready?GOOD:m->hook_in_development?WARN:TEXT3;
    uc_circle(c,(float)x+6.0f,(float)top+208.0f,5.0f,tone);
    uf_draw(c,UF_BODY_MEDIUM,x+20,centre(UF_BODY_MEDIUM,top+208),m->hook,m->hook_ready||m->hook_in_development?TEXT:TEXT2,0);
}
static void resolution(uc_canvas *c,const us_model *m)
{
    const int top=RESOLUTION_Y,x=SIDE_X+28;
    caption(c,x,top+40,"HELIXSR RESOLUTION",TEXT3);
    const float bx=(float)x,by=(float)top+56.0f,bw=(float)SIDE_W-56.0f,bh=112.0f;
    if(!m->hook_ready||!m->output_width||!m->input_width||m->blank) {
        uc_round_rect(c,bx,by,bw,bh,12.0f,0x08ffffffu);
        uc_round_stroke(c,bx,by,bw,bh,12.0f,1.5f,0x26ffffffu);
        const char *a=!m->game_count?"No game selected":m->blank?"Not scanned yet":"No HelixSR package";
        const char *b=!m->game_count?"":m->blank?"Launch once to scan its upscaler":"The game uses its own resolution";
        uf_draw(c,UF_LABEL,(int)(bx+bw*0.5f)-uf_measure(UF_LABEL,a,0)/2,(int)(by+bh*0.5f)-2,a,TEXT2,0);
        uf_draw(c,UF_SMALL,(int)(bx+bw*0.5f)-uf_measure(UF_SMALL,b,0)/2,(int)(by+bh*0.5f)+28,b,TEXT3,0);
        return;
    }
    /* One scale for every box: a 3840 x 2160 frame fills the area, so a 4K output draws larger than 1440p or 1080p,
     * and the render box keeps its true size inside the output. */
    const float k=bw/3840.0f<bh/2160.0f?bw/3840.0f:bh/2160.0f,fw=3840.0f*k,fh=2160.0f*k,fx=bx+(bw-fw)*0.5f,fy=by+(bh-fh)*0.5f;
    const float ow=(float)m->output_width*k,oh=(float)m->output_height*k,ox=fx+(fw-ow)*0.5f,oy=fy+(fh-oh)*0.5f;
    const float rw=(float)m->input_width*k,rh=(float)m->input_height*k,rx=ox+(ow-rw)*0.5f,ry=oy+(oh-rh)*0.5f;
    uc_round_stroke(c,fx,fy,fw,fh,8.0f,1.0f,0x18ffffffu);
    uf_draw_right(c,UF_CAPTION,(int)(fx+fw)-8,(int)fy+18,"4K",TEXT3);
    uc_round_rect(c,ox,oy,ow,oh,8.0f,0x0affffffu);
    uc_round_stroke(c,ox,oy,ow,oh,8.0f,1.5f,0x40ffffffu);
    uc_round_rect(c,rx,ry,rw,rh,6.0f,uc_alpha(ACCENT,0x30));
    uc_round_stroke(c,rx,ry,rw,rh,6.0f,2.0f,ACCENT);
    char render[48],output[48];
    snprintf(render,sizeof render,"%u " UF_TIMES " %u",m->input_width,m->input_height);
    snprintf(output,sizeof output,"%u " UF_TIMES " %u",m->output_width,m->output_height);
    const int line=top+206,w=uf_draw(c,UF_BODY_MEDIUM,x,line,render,TEXT,0);
    arrow(c,(float)(x+w+14),(float)line-8.0f,26.0f,TEXT3);
    uf_draw(c,UF_BODY_MEDIUM,x+w+54,line,output,TEXT,0);
    char factor[32],pixels[24],second[32];
    scale_text(factor,sizeof factor,m->output_width,m->input_width);
    scale_text(second,sizeof second,m->output_height,m->input_height);
    const unsigned long long area=(unsigned long long)m->output_width*m->output_height;
    const unsigned percent=area?(unsigned)(((unsigned long long)m->input_width*m->input_height*100u+area/2u)/area):0u;
    snprintf(pixels,sizeof pixels,"%u%%",percent);
    const int stats=top+250,column=x+(SIDE_W-56)/2+12;
    if(strcmp(factor,second)) {
        char both[64];snprintf(both,sizeof both,"%s / %s",factor,second);
        uf_draw(c,UF_TITLE,x,stats-2,both,TEXT,0);
    } else uf_draw(c,UF_STAT,x,stats,factor,TEXT,0);
    caption(c,x,stats+22,strcmp(factor,second)?"UPSCALE W / H":"UPSCALE PER AXIS",TEXT3);
    uf_draw(c,UF_STAT,column,stats,pixels,TEXT,0);
    caption(c,column,stats+22,"PIXELS RENDERED",TEXT3);
}
static void status(uc_canvas *c,const us_model *m)
{
    const int top=STATUS_Y,x=SIDE_X+28,w=SIDE_W-56;
    caption(c,x,top+40,"STATUS",TEXT3);
    const uint32_t tone=!m->game_count||(!m->settings_ok&&!m->blank)?BAD:m->blank?TEXT2:m->hook_ready?GOOD:WARN;
    uc_circle(c,(float)x+9.0f,(float)top+67.0f,9.0f,uc_alpha(tone,0x40));
    uc_circle(c,(float)x+9.0f,(float)top+67.0f,4.0f,tone);
    const int lines=uf_wrap(c,UF_BODY_MEDIUM,x+30,top+75,w-30,28,2,m->status,TEXT);
    uf_wrap(c,UF_SMALL,x+30,top+75+lines*28+4,w-30,24,2,m->detail,TEXT2);
    const int line=BOTTOM-86;
    uc_rect(c,x,line,w,1,DIVIDER);
    const uint32_t note=ACCENT;
    uc_circle(c,(float)x+11.0f,(float)line+28.0f,11.0f,uc_alpha(note,0x30));
    uc_line(c,(float)x+11.0f,(float)line+24.0f,(float)x+11.0f,(float)line+33.0f,2.4f,note);
    uc_circle(c,(float)x+11.0f,(float)line+19.5f,1.6f,note);
    uf_wrap(c,UF_SMALL,x+34,line+34,w-34,24,2,m->feedback,TEXT);
}
static int hint(uc_canvas *c,int x,const char *label)
{
    return x+uf_draw(c,UF_BODY,x,centre(UF_BODY,1024),label,TEXT2,0)+40;
}
static void footer(uc_canvas *c,const us_model *m)
{
    const int cy=1024;int x=MARGIN;
    if(m->game_count) {
        dpad(c,(float)x+15.0f,(float)cy,1);x=hint(c,x+40,"Game");
        dpad(c,(float)x+15.0f,(float)cy,0);x=hint(c,x+40,"Menu");
        const char *cross=m->menu==US_MENU_LIBRARY?"Details":m->focus==US_RESOLUTION?"Edit resolution":
            m->focus==US_FRAMES?"Frame rate":m->focus==US_HUD?
            (m->hud_available&&!m->blank?"Switch the HUD":"HUD unavailable"):
            m->scan_only?"HelixSR unavailable":"Switch HelixSR";
        face_button(c,(float)x+15.0f,(float)cy,ICON_CROSS,0xffdfe5efu);x=hint(c,x+40,cross);
        face_button(c,(float)x+15.0f,(float)cy,ICON_TRIANGLE,0xffdfe5efu);x=hint(c,x+40,"Launch");
        face_button(c,(float)x+15.0f,(float)cy,ICON_SQUARE,0xffdfe5efu);x=hint(c,x+40,"Frame rate");
    }
    x+=key_cap(c,x,cy,"OPTIONS")+12;hint(c,x,"Exit");
}
/* Shared parts (ui_parts.h). */
int us_centre(uf_face face,int cy) { return centre(face,cy); }
void us_card(uc_canvas *c,int x,int y,int w,int h) { card(c,x,y,w,h); }
void us_focus_ring(uc_canvas *c,int x,int y,int w,int h) { focus_ring(c,x,y,w,h); }
void us_caption(uc_canvas *c,int x,int baseline,const char *text,uint32_t color) { caption(c,x,baseline,text,color); }
void us_chevron(uc_canvas *c,float x,float cy,int right,uint32_t color) { chevron(c,x,cy,right,color); }
void us_arrow(uc_canvas *c,float x,float cy,float length,uint32_t color) { arrow(c,x,cy,length,color); }
void us_face_button(uc_canvas *c,float cx,float cy,int icon,uint32_t disc) { face_button(c,cx,cy,icon,disc); }
void us_dpad(uc_canvas *c,float cx,float cy,int vertical) { dpad(c,cx,cy,vertical); }
int us_key_cap(uc_canvas *c,int x,int cy,const char *label) { return key_cap(c,x,cy,label); }
int us_chip(uc_canvas *c,int right,int cy,const char *text,uint32_t dot) { return chip(c,right,cy,text,dot); }
int us_hint(uc_canvas *c,int x,const char *label) { return hint(c,x,label); }
void us_scale_text(char *out,unsigned size,unsigned output,unsigned input) { scale_text(out,size,output,input); }
const char *us_output_tag(unsigned w,unsigned h) { return output_tag(w,h); }
uint32_t us_tone(int tone) { return tone==US_TONE_GOOD?GOOD:tone==US_TONE_WARN?WARN:tone==US_TONE_BAD?BAD:TEXT3; }
void us_init(us_screen *s,uint32_t *pixels,uint32_t *background)
{
    memset(s,0,sizeof *s);s->pixels=pixels;s->background=background;
    uc_canvas c;uc_init(&c,background,US_WIDTH,US_HEIGHT,US_WIDTH);draw_background(&c);
}
int us_render(us_screen *s,const us_model *m)
{
    if(s->valid&&!memcmp(&s->drawn,m,sizeof *m))return 0;
    uc_canvas c;uc_init(&c,s->pixels,US_WIDTH,US_HEIGHT,US_WIDTH);
    uc_copy(&c,s->background);
    int right=chip(&c,1848,80,m->pro?"PS5 Pro":"PS5",0)-12;
    right=chip(&c,right,80,m->controller?"Controller connected":"No controller",m->controller?GOOD:WARN)-12;
    chip(&c,right,80,m->hook_ready?"HelixSR package ready":"Runtime inactive",m->hook_ready?GOOD:TEXT3);
    if(m->screen==US_SCREEN_FRAMES)uk_draw(&c,m);
    else {
        library(&c,m);hero(&c,m);settings(&c,m);upscaler_card(&c,m);resolution(&c,m);status(&c,m);
        if(m->screen==US_SCREEN_EDITOR)ue_draw(&c,m);else footer(&c,m);
    }
    s->drawn=*m;s->valid=1;
    return 1;
}
