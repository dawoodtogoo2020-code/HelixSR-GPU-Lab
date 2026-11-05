#ifndef K8_DISPATCH_SELECT_H
#define K8_DISPATCH_SELECT_H
#include "model_sequence.h"
#include "k8_fixture_guard.h"
#include <string.h>
/* Evaluated on this dispatch's immutable loaded CBV. General remains loaded. */
static uint32_t ms_k8_select(const struct ms_fixture *f,const struct ms_launch *l,
                             uint32_t launch,const void *original_cbv) {
    const uint32_t fallback=l->shader;
    if(fallback>=f->shader_count)return fallback;
    const struct ms_shader *g=f->shaders+fallback;
    if(!g->name || !g->code.sha256 || strcmp(g->name,"k8_hdr_mvdiff_mvhi") ||
       strcmp(g->code.sha256,"30e70fd2ed3141f0b0ea53970052efe73553fb4926472717ea45dff4a2c97201") ||
       g->code.bytes!=169040 || g->rsrc1!=1613496331u || g->rsrc2!=390u ||
       g->user_slot!=2 || g->scratch_bytes_per_wave ||
       k8_select_variant(launch,original_cbv,(size_t)l->args.bytes,l->grid,g->block)!=K8_ARGUMENT192_ZERO)return fallback;
    uint32_t selected=fallback;
    for(uint32_t i=0;i<f->shader_count;i++) {
        const struct ms_shader *c=f->shaders+i;
        if(!c->name || strcmp(c->name,"k8_argument192_zero"))continue;
        if(selected!=fallback || !c->code.sha256 ||
           strcmp(c->code.sha256,"65eb67fda08f3f9b22611b69fd32482df1dfaeb4828e0c10d83164320978cc5a") ||
           c->code.bytes!=84368 || c->rsrc1!=g->rsrc1 || c->rsrc2!=g->rsrc2 ||
           memcmp(c->block,g->block,sizeof g->block) || c->user_slot!=g->user_slot ||
           c->scratch_bytes_per_wave)return fallback;
        selected=i;
    }
    return selected;
}
#endif
