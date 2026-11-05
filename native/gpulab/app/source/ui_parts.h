#ifndef UI_PARTS_H
#define UI_PARTS_H
/* The GPU Lab palette and drawing parts shared by ui_screen.c and the u07 screens (ui_editor.c, ui_frames.c). The
 * parts are ui_screen.c's own helpers; these names only export them. */
#include "ui_canvas.h"
#include "ui_font.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BG_TOP 0xff0c1222u
#define BG_BOTTOM 0xff070a12u
#define CARD 0xff0f1626u
#define CARD_EDGE 0x1cffffffu
#define ROW_FOCUS 0xff19233bu
#define DIVIDER 0x10ffffffu
#define TEXT 0xffeef2f8u
#define TEXT2 0xffa3aec2u
#define TEXT3 0xff69758eu
#define ACCENT 0xff4d9dffu
#define ACCENT2 0xff8b73ffu
#define GOOD 0xff3fd59au
#define WARN 0xfff3b44bu
#define BAD 0xffff5e6fu
#define INK 0xff0c1222u
enum { US_ICON_CROSS=0, US_ICON_CIRCLE, US_ICON_TRIANGLE, US_ICON_SQUARE };
/* Baseline that centres the face's capitals on cy. */
int us_centre(uf_face,int cy);
void us_card(uc_canvas *,int x,int y,int w,int h);
void us_focus_ring(uc_canvas *,int x,int y,int w,int h);
void us_caption(uc_canvas *,int x,int baseline,const char *text,uint32_t color);
void us_chevron(uc_canvas *,float x,float cy,int right,uint32_t color);
void us_arrow(uc_canvas *,float x,float cy,float length,uint32_t color);
void us_face_button(uc_canvas *,float cx,float cy,int icon,uint32_t disc);
void us_dpad(uc_canvas *,float cx,float cy,int vertical);
/* Returns the cap's width. */
int us_key_cap(uc_canvas *,int x,int cy,const char *label);
/* Right aligned at right; returns its left edge. */
int us_chip(uc_canvas *,int right,int cy,const char *text,uint32_t dot);
/* A footer hint label at x; returns where the next hint starts. */
int us_hint(uc_canvas *,int x,const char *label);
/* "1.70x" from output and input sizes. */
void us_scale_text(char *out,unsigned size,unsigned output,unsigned input);
/* "720p" .. "4K", else "Custom". */
const char *us_output_tag(unsigned w,unsigned h);
uint32_t us_tone(int tone);
#ifdef __cplusplus
}
#endif
#endif
