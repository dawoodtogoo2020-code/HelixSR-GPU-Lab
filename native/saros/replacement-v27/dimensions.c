/* Scalar formulas from the unmodified Model E planner, with explicit float rounding. */
#include <stdint.h>
#include "dimensions.h"
#define U(p,n) (*(uint32_t*)((uint8_t*)(p)+(n)))
#define F(p,n) (*(float*)((uint8_t*)(p)+(n)))
static void texture(uint8_t *t,uint8_t *u,unsigned table,unsigned recipe,unsigned w,unsigned h,unsigned bytes){
 unsigned pitch=(w*bytes+255)&~255u;
 U(u,table+4)=(U(u,table+4)&0x3fffffffu)|((w-1)&3u)<<30;
 U(u,table+8)=((w-1)>>2)|((h-1)<<14)|0x80000000u;
 U(u,table+16)=pitch==w*bytes?0:pitch/bytes-1;
 U(t,recipe+8)=U(u,table+4);
}
void hx_dimensions(uint8_t *t,uint8_t *u,unsigned w,unsigned h,unsigned *grids){
 volatile uint32_t one_bits=0x3f800000u,wo_bits=0x45700000u,ho_bits=0x45070000u,threshold_bits=0x3d0f5c29u;
 union{uint32_t u;float f;}one={one_bits},wo={wo_bits},ho={ho_bits},th={threshold_bits};
 float rx=(float)((double)w/(double)wo.f),ry=(float)((double)h/(double)ho.f);
 float ix=(float)((double)one.f/(double)rx),iy=(float)((double)one.f/(double)ry);
 unsigned lw=w>=1920?1920:w/2,lh=h>=1080?1080:h/2;if(lw<256)lw=256;if(lh<256)lh=256;
 uint8_t *a=t+DIM_LUMA;F(a,0)=(float)((double)one.f/(double)(lw*lh));U(a,4)=lw-1;U(a,8)=lh-1;U(a,20)=w-1;U(a,24)=h-1;
 unsigned gx=(lw+7)/8,gy=(lh+7)/8;U(a,DIM_LUMA_GRID)=gx;U(a,DIM_LUMA_GRID+4)=gy;
 grids[0]=gx;grids[1]=gy;
 unsigned n=lw*lh;
 for(unsigned i=0;i<3;i++){a=t+DIM_REDUCE+i*768;U(a,0)=n;n=(n+511)/512;U(a,DIM_REDUCE_GRID)=n;grids[2+i]=n;}
 a=t+DIM_INPUT;
 U(a,40)=U(a,48)=w-1;U(a,44)=U(a,52)=h-1;
 /* SAROS normalized motion vectors are scaled to Model E's output-pixel convention. */
 F(a,56)=(float)((double)ix*(double)w);F(a,60)=(float)((double)iy*(double)h);
 F(a,72)=F(a,120)=F(a,128)=F(a,144)=rx;F(a,76)=F(a,124)=F(a,132)=F(a,148)=ry;
 U(a,136)=w;U(a,140)=h;
 a=t+DIM_OUTPUT;F(a,528)=ry;F(a,532)=(float)((double)ho.f/(double)h);F(a,536)=rx;F(a,540)=ix;
 U(a,564)=h-1;U(a,572)=w-1;F(a,584)=(float)((double)th.f*(double)(float)((double)iy*(double)ix));
 DIM_TEXTURE_UPDATES
}
