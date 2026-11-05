#include "triplet.h"
#include "full_profile.h"
/* Preserve the exact texels from the same render callback. All pitches below are bytes. */
#include "live_tile.h"

static uint32_t bits(float v) {
    union {float f; uint32_t u;} n = {.f=v};
    return n.u;
}
int hx_prepare_triplet(uint8_t *arena,const ht_job *j,hx_tile_args *args) {
    if(!j || !arena || !args || !j->render_tick ||
       j->active.x<64 || j->active.y<64 || j->roi.x<0 || j->roi.y<0 ||
       j->roi.x>j->active.x-64 || j->roi.y>j->active.y-64)return 0;
    union {uint64_t u;double d;} dx={.u=j->jitter_bits[0]},dy={.u=j->jitter_bits[1]};
    union {unsigned u;float f;} exp={.u=j->exposure_bits[0]};
    float jx=(float)dx.d,jy=(float)dy.d;
    if((bits(jx)&0x7fffffff)>0x41800000 || (bits(jy)&0x7fffffff)>0x41800000 ||
       exp.u<0x00800000 || exp.u>0x47800000)return 0;
    const volatile uint32_t one_word=0x3f800000;
    union {uint32_t u;float f;} one={.u=one_word};
    args->jitter_x=jx;
    args->jitter_y=jy;
    args->previous_jitter_x=0;
    args->previous_jitter_y=0;
    args->pre_exposure=exp.f;
    args->inv_pre_exposure=one.f/exp.f;
    /* Original full-view normalized motion, with 128 output pixels per 64 input pixels. */
    args->motion_x=0;
    args->motion_y=0;
    args->reset=1;
    return 1;
}
