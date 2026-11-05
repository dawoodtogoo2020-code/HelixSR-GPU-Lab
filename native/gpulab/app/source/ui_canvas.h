#ifndef UI_CANVAS_H
#define UI_CANVAS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* CPU 2D drawing into caller-owned 0xAARRGGBB pixels (the display's BGRA8
 * word order). Shapes are anti-aliased by signed distance; source alpha blends
 * over an opaque destination. Every call clips to the canvas clip rectangle.
 * No allocation, no libm. Single caller. */
typedef struct uc_canvas {
    uint32_t *pixels;
    int width,height,stride;
    int clip_x0,clip_y0,clip_x1,clip_y1;
} uc_canvas;

void uc_init(uc_canvas *,uint32_t *pixels,int width,int height,int stride);
/* Intersects with the whole canvas; uc_unclip restores the whole canvas. */
void uc_clip(uc_canvas *,int x,int y,int width,int height);
void uc_unclip(uc_canvas *);
/* Opaque copy of a whole width x height image with the same stride. */
void uc_copy(uc_canvas *,const uint32_t *source);
void uc_rect(uc_canvas *,int x,int y,int width,int height,uint32_t color);
void uc_vgradient(uc_canvas *,int x,int y,int width,int height,uint32_t top,uint32_t bottom);
void uc_round_rect(uc_canvas *,float x,float y,float width,float height,float radius,uint32_t color);
void uc_round_stroke(uc_canvas *,float x,float y,float width,float height,float radius,float thickness,uint32_t color);
/* Diagonal gradient, from at the top-left corner to to at the bottom-right. */
void uc_round_gradient(uc_canvas *,float x,float y,float width,float height,float radius,uint32_t from,uint32_t to);
/* Soft falloff from the shape edge out to blur pixels. */
void uc_shadow(uc_canvas *,float x,float y,float width,float height,float radius,float blur,uint32_t color);
/* Like uc_shadow, but only outside the shape, so it can go over a drawn card. */
void uc_outer_glow(uc_canvas *,float x,float y,float width,float height,float radius,float blur,uint32_t color);
void uc_circle(uc_canvas *,float cx,float cy,float radius,uint32_t color);
void uc_ring(uc_canvas *,float cx,float cy,float radius,float thickness,uint32_t color);
/* Round-capped segment. */
void uc_line(uc_canvas *,float x0,float y0,float x1,float y1,float thickness,uint32_t color);
/* Radial glow, full color alpha at the centre to zero at radius. */
void uc_glow(uc_canvas *,float cx,float cy,float radius,uint32_t color);
/* Coverage 0..255 of color at one pixel. */
void uc_blend(uc_canvas *,int x,int y,uint32_t color,unsigned coverage);
uint32_t uc_mix(uint32_t from,uint32_t to,unsigned amount);
uint32_t uc_alpha(uint32_t color,unsigned alpha);
float uc_sqrt(float);
/* sin and cos of a turn fraction, accurate to about 1e-3. */
float uc_sin_turn(float turns);
#ifdef __cplusplus
}
#endif
#endif
