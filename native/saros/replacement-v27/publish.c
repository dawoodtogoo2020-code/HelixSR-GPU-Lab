/* Publish the independently rendered single-layer arrays directly.
 * Both return SRVs select only slice 0; generic history also records slice 0.
 * Stock multi-frame texture arrays are not retained by the replacement.
 * No stock TSR texture or dispatch is created on a successful owned frame. */
#include "triplet.h"
typedef struct {hv8 bytes[16];void *texture;} srv_desc;
typedef struct {int size[3],source[3],target[3];unsigned src_layer,dst_layer,layers,src_mip,dst_mip,mips;} copy_desc;
#pragma GCC visibility push(hidden)
extern const hv8 ht_module[],hv_texture_vt[],hc_texture_copy_vt[];
extern const volatile hvptr ht_render_tick;
extern void *hv_texture(void*,const void*,const void*,int),*hv_srv(void*,void*,void*,const srv_desc*);
extern void hc_copy_texture(void*,void*,void*,const copy_desc*),hx_history(void*,void*,void*);
#pragma GCC visibility pop
#define P(p,n) (*(void**)((hv8*)(p)+(n)))
#define U(p,n) (*(unsigned*)((hv8*)(p)+(n)))
#define Q(p,n) (*(hvptr*)((hv8*)(p)+(n)))
static void zero(void *p,unsigned n){__asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(0):"memory","cc");}
static int registered(void*b,unsigned off,void*p,unsigned index){return index<U(b,off+8)&&P(b,off)&&((void**)P(b,off))[index]==p;}
static __attribute__((noinline)) void *prepare_output(void *b,void *tile,unsigned n){
 unsigned w=n?960:3840,h=n?540:2160;
 hvptr size=(hvptr)w|((hvptr)h<<32);
 if(!tile||P(tile,0)!=hv_texture_vt||Q(tile,0x84)!=size||Q(tile,0x8c)!=0x1a01010100010001UL||!registered(b,0x178,tile,U(tile,0x9c)))return 0;
 void *t=tile;
 srv_desc d;zero(&d,sizeof d);d.texture=t;d.bytes[2]=1;d.bytes[6]=1;d.bytes[10]=1;
 void *srv=hv_srv((hv8*)b+0x198,(hv8*)b+0x70,P(t,8),&d);
 if(!srv||P(srv,0)!=(void*)(ht_module+0x8136380)||P(srv,0x38)!=t||!registered(b,0x198,srv,U(srv,0x1c)))return 0;
 for(unsigned i=0;i<11;i++)if(((hv8*)srv)[0x28+i]!=d.bytes[i])return 0;
 return srv;
}
int hx_publish(void *result,void *b,void **tiles,hvptr tick,void *view){
 if(!result||!b||!tiles||!view||!tick||tick!=ht_render_tick)return 0;
 void *a=prepare_output(b,tiles[0],0);if(!a)return 0;
 void *c=prepare_output(b,tiles[1],1);if(!c||tick!=ht_render_tick)return 0;
 hx_history(b,view,P(a,0x38));
 zero(result,144);P(result,0)=a;Q(result,16)=3840UL|(2160UL<<32);P(result,48)=c;Q(result,64)=960UL|(540UL<<32);
 return 1;
}
