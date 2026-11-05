/* Inspect the verified TSR return inside its own RDG scope. No memory is retained or modified. */
#include <stdint.h>
#include <stddef.h>
#include "returned_output.h"

static uint32_t u32(const uint8_t *p,size_t n){return *(const uint32_t *)(p+n);}
static uint16_t u16(const uint8_t *p,size_t n){return *(const uint16_t *)(p+n);}
static uintptr_t word(const uint8_t *p,size_t n){return *(const uintptr_t *)(p+n);}
static int possible(uintptr_t p){return p>=0x10000 && p<0x400000000000ULL && !(p&7);}
int hx_returned_output(const void *stock_result,const void *module,hx_output_shape *info){
    if(!stock_result || !module || !info)return HX_OUTPUT_INVALID;
    const uint8_t *ret=stock_result,*base=module;
    uintptr_t view=word(ret,0);
    if(!possible(view) || word((const void *)view,0)!=(uintptr_t)(base+0x8136380))return HX_OUTPUT_INVALID;
    const uint8_t *srv=(const void *)view;uintptr_t texture=word(srv,0x38);
    if(!possible(texture) || word((const void *)texture,0)!=(uintptr_t)(base+0x787aa30))return HX_OUTPUT_INVALID;
    const uint8_t *t=(const void *)texture;uintptr_t name=word(t,8);
    if(name<(uintptr_t)(base+0x6ce8000) || name>=(uintptr_t)(base+0x6ce8000+0xab87d4))return HX_OUTPUT_INVALID;
    hx_output_shape s={0};s.width=u32(t,0x84);s.height=u32(t,0x88);s.format=t[0x93];s.dimension=t[0x92];
    s.layers=u16(t,0x8e);s.mips=t[0x90];s.samples=t[0x91];s.slice=u16(srv,0x2c);s.name_rva=(uint32_t)(name-(uintptr_t)base);
    for(unsigned i=0;i<4;i++)s.rect[i]=u32(ret,8+4*i);
    if(s.width<3840 || s.width>8192 || s.height<2160 || s.height>8192 || s.rect[0]!=0 || s.rect[1]!=0 || s.rect[2]!=3840 || s.rect[3]!=2160 ||
       !s.layers || s.slice>=s.layers || !s.mips || !s.samples)return HX_OUTPUT_INVALID;
    *info=s;
    /* Existing ROI readback explicitly targets slice 0 / mip 0. Multi-slice and
     * multi-mip TSR history needs a slice-aware copy pass. Report its shape only. */
    return s.dimension==0 && s.layers==1 && s.slice==0 && s.mips==1 && s.samples==1?HX_OUTPUT_SINGLE_PLANE:HX_OUTPUT_LAYERED;
}
