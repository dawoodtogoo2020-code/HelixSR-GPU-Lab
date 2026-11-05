#ifndef HELIX_SAROS_VELOCITY_PASS_H
#define HELIX_SAROS_VELOCITY_PASS_H

typedef unsigned char hv8;
typedef unsigned short hv16;
typedef unsigned hv32;
typedef unsigned long hvptr;
typedef struct { int x,y; } hv_point;
typedef struct { const hvptr *vptr; void *fence; hvptr misc; void *staging; } hv_readback;
enum { HV_IDLE, HV_BUILDING, HV_QUEUED, HV_DONE, HV_INPUT_ERROR, HV_SHADER_ERROR,
       HV_TEXTURE_ERROR, HV_VIEW_ERROR, HV_PASS_ERROR, HV_FENCE_ERROR, HV_MAP_ERROR, HV_TIMEOUT };
/* The caller retains one zeroed job on the heap through GPU retirement or title exit. */
typedef struct {
    hv_readback readback;
    hv32 state,polls,pitch,height;
    hv_point extent,source_extent,roi;
    hv8 tile[8*8*4]; /* row-major binary16 RG pairs, copied independently of pitch */
} hv_job;
int hv_enqueue(void *builder,void *view,void *tsr_inputs,hv_job *job);
int hv_poll(hv_job *job);
#endif
