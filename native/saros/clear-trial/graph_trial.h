#ifndef HELIX_SAROS_GRAPH_TRIAL_H
#define HELIX_SAROS_GRAPH_TRIAL_H
#include "graph_buffers.h"

enum { HX_CAPTURE_UNSET, HX_CAPTURE_OK, HX_CAPTURE_REJECTED };
typedef struct {
    hxptr reserved;
    void *storage;
} hx_map_facade;
typedef struct {
    hx_map_facade facade;
    void *storage[9];      /* Native executor reads the shader-array pointer at +0x40. */
    void *slots[16];       /* The verified FShader instance uses 15. */
    hx32 capture_status;
    hx32 started;
    hx_graph_buffers expected;
    hx_buffer_readback copy;
    hxptr names[8]; /* two UTF-16 names, game-owned for the whole graph */
} hx_graph_trial;

int hx_begin_trial(void *builder,void *owned_shader,hx_graph_trial *job);
int hx_poll_trial(hx_graph_trial *job,hx8 bytes[HX_RESULT_BYTES]);
#endif
