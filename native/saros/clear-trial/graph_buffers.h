#ifndef HELIX_SAROS_GRAPH_BUFFERS_H
#define HELIX_SAROS_GRAPH_BUFFERS_H

typedef unsigned char hx8;
typedef unsigned hx32;
typedef unsigned long hxptr;

typedef struct {
    void *arguments;
    void *output;
} hx_graph_buffers;

enum { HX_ARG_BYTES=64, HX_PREFIX=16, HX_BODY=256, HX_RESULT_BYTES=288 };
enum { HX_COPY_IDLE, HX_COPY_WAITING, HX_COPY_DONE, HX_COPY_ERROR };

typedef struct {
    hxptr *vtable;
    void *fence;
    hxptr misc;
    void *staging;
    hx32 state, attempts;
} hx_buffer_readback;

int hx_prepare_buffers(void *builder, const unsigned short *arg_name, const unsigned short *out_name,
                       hx_graph_buffers *buffers);
int hx_begin_readback(void *builder, void *output, hx_buffer_readback *readback);
int hx_poll_readback(hx_buffer_readback *readback, hx8 result[HX_RESULT_BYTES]);
void hx_make_expected(hx8 bytes[HX_RESULT_BYTES], int cleared);
void hx_make_arguments(hx8 data[HX_ARG_BYTES]);

#endif
