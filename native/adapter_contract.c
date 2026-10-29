/* SPDX-License-Identifier: MIT */
#include "adapter_contract.h"
#include <math.h>
#include <string.h>

static int hash_valid(const char h[HC_HASH_SIZE]) {
    unsigned i; for(i=0;i<64;i++) if(!((h[i]>='0'&&h[i]<='9')||(h[i]>='a'&&h[i]<='f'))) return 0;
    return h[64]==0;
}
static int key_valid(const char key[HC_KEY_SIZE]) {
    unsigned i;
    for (i=0; i<HC_KEY_SIZE; ++i) {
        unsigned char c=(unsigned char)key[i];
        if (!c) return i != 0;
        if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c==':'||c=='-')) return 0;
    }
    return 0;
}
static int hc_identity_valid(const hc_identity *id) {
    return id && key_valid(id->device) && key_valid(id->title) && key_valid(id->build) && hash_valid(id->executable_hash);
}
static int same_id(const hc_identity *a,const hc_identity *b) {
    return !strcmp(a->device,b->device)&&!strcmp(a->title,b->title)&&!strcmp(a->build,b->build)&&!strcmp(a->executable_hash,b->executable_hash);
}
static int bounds(uint32_t n,uint32_t lo,uint32_t hi) { return lo && lo<=hi && n>=lo && n<=hi; }
static int format_ok(unsigned i,hc_format f) {
    switch(i) {
    case HC_COLOR_IN: case HC_OUTPUT: return f==HC_FMT_RGBA16F||f==HC_FMT_RGBA32F;
    case HC_DEPTH_IN: return f==HC_FMT_D32F||f==HC_FMT_R32F;
    case HC_MOTION_IN: return f==HC_FMT_RG16F||f==HC_FMT_RG32F;
    case HC_EXPOSURE_IN: return f==HC_FMT_R16F||f==HC_FMT_R32F;
    case HC_REACTIVE_IN: case HC_COMPOSITION_IN: return f==HC_FMT_R8_UNORM||f==HC_FMT_R16F||f==HC_FMT_R32F;
    default:return 0;
    }
}
static unsigned pixel_bytes(hc_format f) {
    switch(f) { case HC_FMT_RGBA16F:return 8; case HC_FMT_RGBA32F:return 16;
    case HC_FMT_RG16F:case HC_FMT_R32F:case HC_FMT_D32F:return 4;
    case HC_FMT_RG32F:return 8; case HC_FMT_R16F:return 2; case HC_FMT_R8_UNORM:return 1;
    default:return 0; }
}
static hc_decision deny(hc_result r) { hc_decision d={r,0};return d; }
hc_decision hc_check(const hc_descriptor *d,const hc_frame *f,const hc_previous *p) {
    unsigned i; int changed; uint32_t allowed=(1u<<HC_RESOURCE_COUNT)-1u;
    if(!d||!f||!p) return deny(HC_INVALID);
    if(!hc_identity_valid(&d->identity)||!hc_identity_valid(&f->identity)||!hash_valid(d->backend_hash)||!hash_valid(d->model_hash)||!hash_valid(f->backend_hash)||!hash_valid(f->model_hash)) return deny(HC_INVALID);
    if(d->adapter_qualified!=1||d->backend_qualified!=1) return deny(HC_UNQUALIFIED);
    if((d->required_wave_size!=32&&d->required_wave_size!=64)||(f->observed_wave_size!=32&&f->observed_wave_size!=64)||(d->required_math_mask&~HC_MATH_KNOWN)||(f->available_math_mask&~HC_MATH_KNOWN)) return deny(HC_UNSUPPORTED);
    if(f->capabilities_confirmed!=1) return deny(HC_UNSAFE);
    if(d->required_wave_size!=f->observed_wave_size||(f->available_math_mask&d->required_math_mask)!=d->required_math_mask) return deny(HC_UNSUPPORTED);
    if(!same_id(&d->identity,&f->identity)||strcmp(d->backend_hash,f->backend_hash)||strcmp(d->model_hash,f->model_hash)) return deny(HC_IDENTITY_MISMATCH);
    if(d->direction<HC_MOTION_CURRENT_TO_PREVIOUS||d->direction>HC_MOTION_PREVIOUS_TO_CURRENT||d->scale<HC_SCALE_PIXELS||d->scale>HC_SCALE_NORMALIZED||d->motion_resolution<HC_MRES_INPUT||d->motion_resolution>HC_MRES_OUTPUT||d->jitter_units<HC_JITTER_PIXELS||d->jitter_units>HC_JITTER_NORMALIZED||d->depth<HC_DEPTH_NORMAL||d->depth>HC_DEPTH_INVERTED||d->depth_encoding<HC_DEPTH_DEVICE_0_1||d->depth_encoding>HC_DEPTH_LINEAR_VIEW||d->color<HC_COLOR_LINEAR||d->color>HC_COLOR_PRE_EXPOSED||d->exposure<HC_EXPOSURE_SCALAR||d->exposure>HC_EXPOSURE_TEXTURE) return deny(HC_UNSUPPORTED);
    if(d->motion_vectors_include_jitter!=0&&d->motion_vectors_include_jitter!=1) return deny(HC_INVALID);
    if((d->required_mask&HC_MANDATORY)!=HC_MANDATORY||(d->required_mask&~allowed)||(f->present_mask&~allowed)||(d->exposure==HC_EXPOSURE_TEXTURE&&!(d->required_mask&HC_BIT(HC_EXPOSURE_IN)))) return deny(HC_INVALID);
    if(!bounds(f->input_width,d->min_input_width,d->max_input_width)||!bounds(f->input_height,d->min_input_height,d->max_input_height)||!bounds(f->output_width,d->min_output_width,d->max_output_width)||!bounds(f->output_height,d->min_output_height,d->max_output_height)||f->input_width>f->output_width||f->input_height>f->output_height) return deny(HC_UNSUPPORTED);
    if(!isfinite(f->jitter_x)||!isfinite(f->jitter_y)||!isfinite(f->frame_time_ms)||f->frame_time_ms<=0||!isfinite(f->exposure_value)||f->exposure_value<=0) return deny(HC_INVALID);
    if(f->barriers_confirmed!=1||f->lifetimes_confirmed!=1||f->output_non_aliasing_confirmed!=1) return deny(HC_UNSAFE);
    if((f->present_mask&d->required_mask)!=d->required_mask) return deny(HC_MISSING_RESOURCE);
    for(i=0;i<HC_RESOURCE_COUNT;i++) if(f->present_mask&HC_BIT(i)) {
        const hc_resource *r=&f->resources[i]; uint32_t w=f->input_width,h=f->input_height;
        if(!r->handle||!r->width||!r->height) return deny(HC_MISSING_RESOURCE);
        if(!format_ok(i,r->format)||!(d->allowed_formats[i]&HC_FORMAT_BIT(r->format))) return deny(HC_UNSUPPORTED);
        if(i==HC_OUTPUT||(i==HC_MOTION_IN&&d->motion_resolution==HC_MRES_OUTPUT)){w=f->output_width;h=f->output_height;}
        if(i==HC_EXPOSURE_IN){w=1;h=1;}
        if(r->width!=w||r->height!=h) return deny(HC_UNSUPPORTED);
        if((r->bytes==0)!=(r->stride==0)) return deny(HC_INVALID);
        if(r->stride) {
            uint64_t row=(uint64_t)r->width*pixel_bytes(r->format);
            uint64_t span=(uint64_t)r->stride*(r->height-1u)+row;
            if(r->stride<row||r->bytes<span) return deny(HC_INVALID);
        }
    }
    for(i=0;i<HC_RESOURCE_COUNT;i++) if(i!=HC_OUTPUT) if((f->present_mask&HC_BIT(i))&&f->resources[i].handle==f->resources[HC_OUTPUT].handle) return deny(HC_ALIAS);
    if((p->has_previous!=0&&p->has_previous!=1)||(p->work_in_flight!=0&&p->work_in_flight!=1)) return deny(HC_INVALID);
    if(!p->has_previous) { if(p->work_in_flight) return deny(HC_INVALID); hc_decision x={HC_ACCEPT,1};return x; }
    if(!hc_identity_valid(&p->identity)||!hash_valid(p->backend_hash)||!hash_valid(p->model_hash)) return deny(HC_INVALID);
    if(f->frame_id<=p->frame_id) return deny(HC_STALE);
    changed=f->previous_frame_id!=p->frame_id||p->observed_wave_size!=f->observed_wave_size||p->available_math_mask!=f->available_math_mask||p->required_wave_size!=d->required_wave_size||p->required_math_mask!=d->required_math_mask||!same_id(&p->identity,&f->identity)||strcmp(p->backend_hash,f->backend_hash)||strcmp(p->model_hash,f->model_hash)||p->history_id!=f->history_id||p->direction!=d->direction||p->scale!=d->scale||p->motion_resolution!=d->motion_resolution||p->jitter_units!=d->jitter_units||p->depth!=d->depth||p->depth_encoding!=d->depth_encoding||p->motion_vectors_include_jitter!=d->motion_vectors_include_jitter||p->required_mask!=d->required_mask||p->present_mask!=f->present_mask||p->color!=d->color||p->exposure!=d->exposure||p->input_width!=f->input_width||p->input_height!=f->input_height||p->output_width!=f->output_width||p->output_height!=f->output_height;
    for(i=0;i<HC_RESOURCE_COUNT;i++) if((f->present_mask&HC_BIT(i)) &&
        (p->resources[i].format!=f->resources[i].format||p->resources[i].width!=f->resources[i].width||
         p->resources[i].height!=f->resources[i].height||p->resources[i].stride!=f->resources[i].stride||
         p->resources[i].bytes!=f->resources[i].bytes)) changed=1;
    if(changed&&p->work_in_flight) { hc_decision x={HC_DRAIN_REQUIRED,1};return x; }
    { hc_decision x={HC_ACCEPT,changed};return x; }
}
