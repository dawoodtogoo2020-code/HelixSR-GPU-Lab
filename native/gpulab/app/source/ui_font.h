#ifndef UI_FONT_H
#define UI_FONT_H
#include <stdint.h>
#include "ui_canvas.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Anti-aliased text from coverage baked by tools/bake_font.py into
 * ui_font_data.h. Strings are NUL-terminated printable ASCII plus the symbol
 * bytes below; any other byte is skipped. Positions are a left edge and a
 * baseline in canvas pixels. tracking adds whole pixels between glyphs. */
#define UF_TIMES "\x80"
#define UF_BULLET "\x81"
#define UF_ELLIPSIS "\x82"
#define UF_MIDDOT "\x83"
#define UF_DASH "\x84"
typedef enum { UF_CAPTION=0, UF_SMALL, UF_BODY, UF_BODY_MEDIUM, UF_LABEL, UF_TITLE, UF_STAT, UF_HERO,
               UF_FACE_COUNT } uf_face;
/* Advance is 26.6 fixed point; offset indexes the coverage bytes. */
typedef struct { uint32_t offset; uint8_t width,height; int8_t left,top; uint16_t advance; } uf_glyph;
typedef struct { uint8_t size,ascent,descent; uint16_t first; } uf_face_data;

int uf_size(uf_face);
/* Height of a capital H above the baseline, for optical centring. */
int uf_cap(uf_face);
int uf_measure(uf_face,const char *text,int tracking);
/* Returns the drawn width. */
int uf_draw(uc_canvas *,uf_face,int x,int baseline,const char *text,uint32_t color,int tracking);
/* Ends with an ellipsis when the text is wider than width. */
int uf_draw_fit(uc_canvas *,uf_face,int x,int baseline,int width,const char *text,uint32_t color);
/* Right edge at x. */
int uf_draw_right(uc_canvas *,uf_face,int x,int baseline,const char *text,uint32_t color);
/* Greedy word wrap from the first baseline; the last allowed line is
 * ellipsised. Returns the number of lines drawn. */
int uf_wrap(uc_canvas *,uf_face,int x,int baseline,int width,int line_height,int max_lines,const char *text,uint32_t color);
#ifdef __cplusplus
}
#endif
#endif
