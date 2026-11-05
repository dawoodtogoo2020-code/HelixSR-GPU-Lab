/* SAROS diagnostic for one tagged view. All three planes belong to the same stock graph. */
#include "triplet.h"
#pragma GCC visibility push(hidden)
extern const hv8 ht_module[];extern hv32 phase;extern hvptr payload;extern const volatile hvptr ht_render_tick;
extern void *game_alloc(hvptr);
extern const hv8 hv_texture_vt[],hv_depth_name[],hv_velocity_name[],hv_output_name[],hv_readback_vt[],hv_fence_vt[],hv_readback_name[];
extern const hv8 ht_color_name[],ht_depth_name[],ht_type[],ht_param_meta[],ht_pass_vt[],ht_uav_vt[],ht_formats[];
extern void *hv_texture(void *,void *,const void *,int);
typedef struct { void *shader,*section; } shader_ref;
extern void ht_type_init(void);extern shader_ref hv_lookup(void *,const void *,int);
extern void ht_depth_pass(void *,void *,void *,void *);
extern void hv_make_name(hvptr *,const void *,int),hv_read_init(hv_readback *,hvptr),hv_read_destroy(hv_readback *);
extern void ht_native_copy(void *,hv_readback *,void *,hv_point,hv_point),hv_unmap(hv_readback *);
extern hv8 *hv_map(hv_readback *,int *,int *);
extern unsigned *hx_trip_phase(void);extern hvptr *hx_trip_payload(void);
#pragma GCC visibility pop
#define phase (*hx_trip_phase())
#define payload (*hx_trip_payload())
static hv8 *at(void *p,unsigned n){return (hv8*)p+n;}
static unsigned num(void *p,unsigned n){return *(unsigned*)at(p,n);}
static hvptr word(void *p,unsigned n){return *(hvptr*)at(p,n);}
static void *ptr(void *p,unsigned n){return *(void**)at(p,n);}
static hv_point pt(void *p,unsigned n){return *(hv_point*)at(p,n);}
static int eq(hv_point a,hv_point b){return a.x==b.x&&a.y==b.y;}
static int valid(const void*p){hvptr n=(hvptr)p;return n>=65536&&n<0x400000000000UL&&!(n&7);}
static void zero(void *p,unsigned n){__asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(0):"memory","cc");}
static void set(unsigned p){__atomic_store_n(&phase,p,__ATOMIC_RELEASE);}
static int shape(void *t,const void *name,unsigned fmt,unsigned texel_bytes){
    if(!valid(t)||ptr(t,0)!=hv_texture_vt||ptr(t,8)!=name||word(t,0x8c)!=((hvptr)fmt<<56|0x10100010001UL))return 0;
    const unsigned *blocks=(const unsigned*)(ht_formats+fmt*48+12);return blocks[0]==1&&blocks[1]==1&&blocks[2]==1&&blocks[3]==texel_bytes;
}
static int eligible(void*v,void*in){
    if(!v||!in||num(v,0x2660)!=4||num(v,0x2664)!=1||word(v,0x348)||word(v,0x350)!=(3840UL|2160UL<<32)||word(v,0x2700)
#ifndef HT_CONFIRMED_HISTORY
       ||(at(v,0xcf48)[0]&1)
#endif
       )return 0;
    hv_point s=pt(v,0x2708);if(s.x<64||s.x>2304||s.y<64||s.y>1296)return 0;
    void *vs=ptr(v,0x2710);
#ifdef HT_CONFIRMED_HISTORY
    if(!valid(vs)||!valid(ptr(v,0xde58))||!valid(ptr(v,0x18)))return 0;
#else
    if(!valid(vs)||num(vs,0x1054)<16||!valid(ptr(v,0xde58))||!valid(ptr(v,0x18)))return 0;
#endif
    unsigned starts[3];starts[0]=8;starts[1]=0x20;starts[2]=0x38;
    for(unsigned i=0;i<3;i++){
        unsigned off=starts[i];void *tex=ptr(in,off);if(!valid(tex)||ptr(tex,0)!=hv_texture_vt||word(tex,0x84)!=(2304UL|1296UL<<32)||word(in,off+8)||!eq(pt(in,off+16),s))return 0;
    }
    return shape(ptr(in,8),ht_color_name,10,8)&&ptr(ptr(in,0x20),8)==hv_depth_name && word(ptr(in,0x20),0x8c)==(11UL<<56|0x10100010001UL) && ptr(ptr(in,0x38),8)==hv_velocity_name && word(ptr(in,0x38),0x8c)==(19UL<<56|0x10100010001UL);
}
int ht_motion_shape(void *t){return shape(t,hv_output_name,15,4);}
static void *depth_output(void *b,void *v,void *d,ht_job *j){
    hv8 desc[56];zero(desc,sizeof desc);*(hvptr*)desc=0x20008;*(unsigned*)at(desc,8)=1;*(unsigned*)at(desc,0x18)=0x3f800000;
    *(hv_point*)at(desc,0x24)=j->source;*(hvptr*)at(desc,0x2c)=0x0d00010100010001UL;desc[0x35]=0xff;
    void *t=hv_texture(b,desc,ht_depth_name,0);if(!shape(t,ht_depth_name,13,4)||!eq(pt(t,0x84),j->source)||t==d)return 0;
    unsigned old=num(b,0x170);ht_depth_pass(b,v,d,t);unsigned n=num(b,0x170);if(n<=old)return 0;
    void *pass=((void**)ptr(b,0x168))[n-1];
    if(!valid(pass)||ptr(pass,0)!=ht_pass_vt||ptr(pass,0x1b8)!=ht_param_meta)return 0;
    void *selected=ptr(pass,0x1c8);if(!valid(selected)||num(selected,0xd4)!=0)return 0;
    void *p=ptr(pass,0x1c0);if(!valid(p)||ptr(p,0)!=d||ptr(p,0x10)!=ptr(v,0x18))return 0;
    void *u=ptr(p,8);return valid(u)&&ptr(u,0)==ht_uav_vt&&ptr(u,0x38)==t?t:0;
}
int ht_current(void *b,void *v,void *in,ht_job *j,void **textures){
    if(!eligible(v,in))return 0;
    zero(j,sizeof(*j));
    j->render_tick=ht_render_tick;j->active=pt(v,0x2708);j->source=pt(ptr(in,0x20),0x84);
    j->roi=(hv_point){0,0};
    j->jitter_bits[0]=word(v,0xcf38);j->jitter_bits[1]=word(v,0xcf40);j->exposure_bits[0]=num(v,0xdcc4);
    extern int hv_enqueue_gpu(void*,void*,void*,hv_job*,void**) __attribute__((visibility("hidden")));
    textures[0]=ptr(in,8);textures[1]=depth_output(b,v,ptr(in,0x20),j);
    return textures[1] && hv_enqueue_gpu(b,v,in,&j->conversion,&textures[2])==HV_QUEUED;
}
int ht_eligible(void *v,void *in){return eligible(v,in);}
