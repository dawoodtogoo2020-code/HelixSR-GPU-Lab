/* Exact-build, private raw buffer pass. Never edits a published pass or shader map. */
#include "graph_trial.h"
typedef struct {hx32 count,dst,src,pad;hx_graph_trial *capture;} args;
typedef struct {void *shader,*map;} ref;
typedef struct {hx8 bytes[16];void *texture;} texture_view;
extern void *hv_uav(void *,void *,void *,const texture_view *,const hx8 *) __attribute__((visibility("hidden")));
extern void *hv_srv(void *,void *,void *,const texture_view *) __attribute__((visibility("hidden")));
#pragma GCC visibility push(hidden)
extern void hx_stock_2e3b98(void *,hxptr),hx_stock_7493a2(void *,unsigned),hx_stock_15e0(void *,unsigned),hx_stock_19f5dc(void *,unsigned);
extern void hx_stock_18cc464(void),hx_stock_18cc55e(void),hx_stock_3fbafc(void *),hx_stock_3fae34(void *,void *);
extern ref hx_stock_40a0a6(void *,void *,unsigned);
extern hx8 hx_stock_81363d8[],hx_stock_818e6a8[],hx_stock_7879738[],hx_stock_7879758[],hx_stock_8eee678[],hx_stock_8eeed38[],hx_stock_6dbc5d0[];
extern int hx_stock_8dd561c,hx_stock_8dd5644;
extern unsigned short hx_stock_8ed28d0[];
extern void *hx_stock_8ed9ae0[],*hx_stock_8eeed80;
extern hx8 hx_stock_8ed296b,hx_stock_8ed29b2,hx_stock_8ed29e1;
#pragma GCC visibility pop
#define P(p,n) (*(void**)((hx8*)(p)+(n)))
#define U(p,n) (*(hx32*)((hx8*)(p)+(n)))
#define Q(p,n) (*(hxptr*)((hx8*)(p)+(n)))
#define B(p,n) (*(hx8*)((hx8*)(p)+(n)))
static void zero(void *p,unsigned n){__asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(0):"memory","cc");}
static void *arena(void *b,unsigned n,unsigned align){
 hxptr p=(Q(b,0x70)+align-1)&-(hxptr)align;
 if(p+n>Q(b,0x78)){hx_stock_2e3b98((hx8*)b+0x70,n+align);p=(Q(b,0x70)+align-1)&-(hxptr)align;}
 Q(b,0x70)=p+n;return(void*)p;
}
static unsigned append(void *b,unsigned off,void *p,void(*grow)(void*,unsigned)){
 unsigned i=U(b,off+8)++;if(i+1>U(b,off+12)){grow((hx8*)b+off,i);i=U(b,off+8)-1;}
 ((void**)P(b,off))[i]=p;return i;
}
static void *view(void *b,void *parent,int writable){
 void *v=arena(b,writable?0x38:0x48,8);zero(v,writable?0x38:0x48);
 P(v,0)=writable?(void*)hx_stock_81363d8:(void*)hx_stock_818e6a8;P(v,8)=P(parent,8);
 B(v,0x18)=writable?2:3;Q(v,0x1c)=~(hxptr)0;
 if(writable){B(v,0x24)=0;P(v,0x30)=parent;}else{Q(v,0x2c)=0xffffffff00000000UL;P(v,0x40)=parent;}
 U(v,0x1c)=append(b,0x198,v,hx_stock_7493a2);return v;
}
void hx_private_memcpy(void *b,void *dst,void *src,const args *a){
 hx_graph_trial *g=a->capture;
 int tex=a->count==8,out=a->count==16;
 if(!g||g->capture_status||!b||!dst||!src||(!out&&(U(dst,0x68)!=0x139||U(dst,0x60)!=4||!U(dst,0x64)))||
    g->expected.arguments!=src||g->expected.output!=dst){if(g)g->capture_status=2;return;}
 if(out){
  if(B(dst,0x93)!=26||Q(dst,0x8c)!=0x1a01010100010001UL||(Q(dst,0x84)!=(3840UL|2160UL<<32)&&Q(dst,0x84)!=(960UL|540UL<<32))||U(src,0x68)!=0x139||U(src,0x60)!=4){g->capture_status=2;return;}
 }else if(tex){
  unsigned fmt=B(src,0x93);
  if((fmt!=10&&fmt!=13&&fmt!=15)|| (Q(src,0x8c)&0x00ffffffffffffffUL)!=0x10100010001UL||
     U(src,0x84)<64||U(src,0x88)<64||a->dst>U(src,0x84)-64||a->src>U(src,0x88)-64){g->capture_status=2;return;}
 }else if(a->count!=4||a->dst||a->src||U(src,0x68)!=0x111||U(src,0x60)!=4||!U(src,0x64)){
  g->capture_status=2;return;
 }
 if(hx_stock_8dd561c<0||hx_stock_8dd561c>=8){g->capture_status=2;return;}
 unsigned platform=hx_stock_8ed28d0[hx_stock_8dd561c+0x174];
 if(platform>=64){g->capture_status=2;return;}
 void *map=hx_stock_8ed9ae0[platform];hx_stock_18cc464();ref r=hx_stock_40a0a6(map,hx_stock_8eee678,12);
 hx_stock_18cc464();hx_stock_18cc55e();
 if(!r.shader||!r.map||U(r.shader,0xd4)!=15||g->facade.storage!=g->storage||
    g->storage[8]!=g->slots||!g->slots[15]){g->capture_status=2;return;}
#ifdef HX_DYNAMIC_GRID
 if(!U(g,0x158)||!U(g,0x15c)||!U(g,0x160)||U(g,0x158)>65535||U(g,0x15c)>65535||U(g,0x160)>65535){g->capture_status=2;return;}
#endif
 void *p=arena(b,0xa0,16);zero(p,0xa0);P(p,0)=hx_stock_7879738;
 append(b,0x98,p,hx_stock_15e0);
 if(out){
  texture_view tv;hx8 flags=0;zero(&tv,sizeof tv);tv.texture=dst;tv.bytes[0]=B(dst,0x94);
  void *uv=hv_uav((hx8*)b+0x198,(hx8*)b+0x70,P(dst,8),&tv,&flags);
  if(!uv||P(uv,0x38)!=dst){g->capture_status=2;return;}
  P(p,0x58)=uv;P(p,0x88)=view(b,src,0);
 }else P(p,0x50)=view(b,dst,1);
 if(tex||out){
  U(p,0x10)=a->dst;U(p,0x14)=a->src;U(p,0x18)=a->pad&65535;U(p,0x1c)=a->pad>>16;
  if(tex){texture_view tv;zero(&tv,sizeof(tv));tv.texture=src;tv.bytes[2]=1;
  void *sv=hv_srv((hx8*)b+0x198,(hx8*)b+0x70,P(src,8),&tv);
  if(!sv||P(sv,0x38)!=src){g->capture_status=2;return;}
  P(p,0x90)=sv;}
  void *f=arena(b,0xf0,8);zero(f,0xf0);
  P(f,0)=(hx8*)f+0xe0;U(f,8)=1;Q(f,0xe0)=0x0010000000000000UL;
  P(f,0x10)=(hx8*)f+0xe8;U(f,0x18)=2;
  U(f,0xe8)=out?0x0e000048:0x12000040;U(f,0xec)=out?0x11000078:0x0c000080;U(f,0xd4)=15;r.shader=f;
 }else{U(p,0x14)=4;P(p,0x88)=view(b,src,0);}
 /* Frozen shader 15 has exactly the two nonzero resource bindings above. */

 hx_stock_3fbafc(b);
 unsigned flags=2,pipeline=1;
 if(hx_stock_8dd5644==2&&hx_stock_8ed296b==1&&hx_stock_8ed29b2&&(hx_stock_8ed29e1&1)){flags=4;pipeline=2;}
 void *pass=arena(b,0x1e8,8);zero(pass,0x1e8);
 void *meta=hx_stock_8eeed80;if(meta)__atomic_fetch_add((hx32*)((hx8*)meta+8),1,__ATOMIC_SEQ_CST);
 P(pass,0)=hx_stock_7879758;P(pass,0x10)=(hx8*)p+0x10;P(pass,0x18)=meta;P(pass,0x20)=hx_stock_8eeed38;
 *(unsigned short*)((hx8*)pass+0x28)=flags;B(pass,0x2a)=2;B(pass,0x2b)=pipeline;
 for(unsigned i=0;i<4;i++)U(pass,0x2c+4*i)=(int)(signed char)hx_stock_6dbc5d0[i];
 for(unsigned i=0x3c;i<=0x4c;i+=4)U(pass,i)=~0u;
 Q(pass,0xd0)=Q(pass,0xe8)=0x100000000UL;Q(pass,0x120)=0x400000000UL;P(pass,0x128)=pass;
 U(pass,0x148)=U(pass,0x150)=~0u;B(pass,0x14c)=B(pass,0x154)=1;
 U(pass,0x178)=1;B(pass,0x17c)=pipeline;B(pass,0x17d)=1;
 P(pass,0x1b8)=hx_stock_8eeed38;P(pass,0x1c0)=(hx8*)p+0x10;P(pass,0x1c8)=r.shader;P(pass,0x1d0)=g;
 U(pass,0x1d8)=U(pass,0x1dc)=U(pass,0x1e0)=1;
#ifdef HX_DYNAMIC_GRID
 U(pass,0x1d8)=U(g,0x158);U(pass,0x1dc)=U(g,0x15c);U(pass,0x1e0)=U(g,0x160);
#endif
 g->capture_status=1;
 U(pass,0x2c)=append(b,0x168,pass,hx_stock_19f5dc);hx_stock_3fae34(b,pass);
}
