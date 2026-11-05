#ifndef SAROS_LIVE_TILE_H
#define SAROS_LIVE_TILE_H
#include <stddef.h>
#include <stdint.h>

/* Fenced CPU readback rows, beginning at the first texel of the retained ROI. */
typedef struct { const uint8_t *pixels; uint32_t bytes, pitch; } hx_tile_plane;
typedef struct {
    uint32_t active_w, active_h, x, y;
    uint32_t render_ordinal;
    float jitter_x, jitter_y, pre_exposure;
    hx_tile_plane color, depth, motion;
} hx_tile_frame;
typedef struct { size_t color, depth, motion; } hx_tile_layout;
typedef struct {
    float jitter_x, jitter_y, previous_jitter_x, previous_jitter_y;
    float pre_exposure, inv_pre_exposure, motion_x, motion_y;
    uint32_t reset;
} hx_tile_args;
/* First-frame evaluation. Caller owns all buffers, which must not alias the sources. */
int hx_prepare_live_tile(uint8_t *arena, size_t bytes, const hx_tile_layout *layout,
                         const hx_tile_frame *frame, hx_tile_args *args);
#endif
