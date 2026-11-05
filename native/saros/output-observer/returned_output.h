#ifndef HELIX_SAROS_RETURNED_OUTPUT_H
#define HELIX_SAROS_RETURNED_OUTPUT_H
#include <stdint.h>
typedef struct { uint32_t name_rva,format,dimension,layers,mips,samples,slice,width,height,rect[4]; } hx_output_shape;
enum { HX_OUTPUT_INVALID, HX_OUTPUT_SINGLE_PLANE, HX_OUTPUT_LAYERED };
int hx_returned_output(const void *stock_result,const void *module,hx_output_shape *out);
#endif
