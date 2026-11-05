#ifndef HELIX_SAROS_TRIPLET_H
#define HELIX_SAROS_TRIPLET_H
#include "../velocity-trial/velocity_pass.h"
#include "../output-observer/returned_output.h"
#define HT_COLOR_BYTES (64*64*8)
#define HT_DEPTH_BYTES (64*64*4)
#define HT_MOTION_BYTES (64*64*4)
#define HT_BYTES (HT_COLOR_BYTES+HT_DEPTH_BYTES+HT_MOTION_BYTES)
typedef struct {
    hvptr magic; unsigned version,bytes; hvptr render_tick; unsigned ordinal,history;
    hv_point active,source,display,roi,motion_roi;
    hvptr jitter_bits[2],previous_projection_jitter_bits[2];
    unsigned exposure_bits[2],history_phase,camera_cut;
    unsigned pitch[3],height[3],fnv[3],reserved;
    hv8 color[HT_COLOR_BYTES],depth[HT_DEPTH_BYTES],motion[HT_MOTION_BYTES];
    hx_output_shape post;
    hv_job conversion;hv_readback color_copy,depth_copy;
    unsigned collected,tries,submitted,motion_copy_valid;
} ht_job;
_Static_assert(__builtin_offsetof(ht_job,color)==160,"wire header");
_Static_assert(__builtin_offsetof(ht_job,depth)==32928,"wire depth");
_Static_assert(__builtin_offsetof(ht_job,motion)==49312,"wire motion");
#endif
