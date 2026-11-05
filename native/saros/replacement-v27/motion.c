/* SAROS PPSA07631 01.000.000 only. One stock conversion into a dedicated RDG texture. */
#include "velocity_pass.h"
typedef struct { void *shader,*section; } shader_ref;
typedef struct { hv8 bytes[16]; void *texture; } view_desc;
typedef struct { hv8 bytes[56]; } texture_desc;
#pragma GCC visibility push(hidden)
extern const hv8 hv_texture_vt[],hv_depth_name[],hv_velocity_name[],hv_output_name[],hv_param_vt[],hv_pass_vt[],hv_param_meta[],hv_shader_type[],hv_fence_vt[],hv_readback_vt[],hv_readback_name[];
extern void hv_init_type(void),hv_arena_grow(void *,int),hv_registry_grow(void *,int);
extern shader_ref hv_lookup(void *,const void *,int);
extern void *hv_renderer(void),*hv_texture(void *,texture_desc *,const void *,int);
extern void hv_access(void *,void *,int),*hv_srv(void *,void *,void *,const view_desc *),*hv_uav(void *,void *,void *,const view_desc *,const hv8 *);
extern void hv_add(void *,const void *,const shader_ref *,void *,hvptr);
extern void hv_make_name(hvptr *,const void *,int),hv_read_init(hv_readback *,hvptr),hv_read_destroy(hv_readback *);
extern int ht_motion_shape(void *);extern void ht_native_copy(void *,hv_readback *,void *,hv_point,hv_point);
extern hv8 *hv_map(hv_readback *,int *,int *);
extern void hv_unmap(hv_readback *);
#pragma GCC visibility pop

static hv8 *at(void *p,int n){return (hv8 *)p+n;}
static int *integer(void *p,int n){return (int *)at(p,n);}
static void **pointer(void *p,int n){return (void **)at(p,n);}
static void zero(void *p,unsigned n){__asm__ volatile("cld; rep stosb":"+D"(p),"+c"(n):"a"(0):"memory","cc");}
static int plausible(void *p){hvptr x=(hvptr)p;return x>=65536 && x<0x400000000000UL && !(x&7);}
static int publish(hv_job *j,int state){__atomic_store_n(&j->state,state,__ATOMIC_RELEASE);return state;}
static int eq(hv_point a,hv_point b){return a.x==b.x && a.y==b.y;}
static hv_point pt(void *p,int n){return *(hv_point *)at(p,n);}

static void *make_texture(void *builder,hv_point extent){
    texture_desc desc; zero(&desc,sizeof(desc));
    *(hvptr *)desc.bytes=0x20008; *integer(&desc,8)=1; *integer(&desc,0x18)=0x3f800000;
    *(hv_point *)at(&desc,0x24)=extent;
    *(hvptr *)at(&desc,0x2c)=0x0f00010100010001UL;desc.bytes[0x35]=0xff;
    return hv_texture(builder,&desc,hv_output_name,0);
}
/* Matched native FFX typed allocation at 0x673cf35, managed in builder's destructor array. */
static void *params(void *b){
    hvptr block=((hvptr)*pointer(b,0x70)+15)&~15UL;
    if(block+0x60>(hvptr)*pointer(b,0x78)){
        hv_arena_grow(at(b,0x70),0x70);block=((hvptr)*pointer(b,0x70)+15)&~15UL;
    }
    *pointer(b,0x70)=(void *)(block+0x60);
    hv8 *header=(hv8 *)block;zero(header,0x60);*pointer(header,0)=(void *)hv_param_vt;
    header[0x48]=1;
    int index=(*integer(b,0xa0))++;
    if(index+1>*integer(b,0xa4)) hv_registry_grow(at(b,0x98),index);
    ((void **)*pointer(b,0x98))[index]=header;
    return header+0x10;
}
static void *srv(void *b,void *t){
    view_desc v; zero(&v,sizeof(v));v.texture=t;v.bytes[2]=at(t,0x90)[0];
    if(at(t,0x92)[0]==1 || at(t,0x92)[0]==4) *(hv16 *)(v.bytes+6)=*(hv16 *)at(t,0x8e);
    return hv_srv(at(b,0x198),at(b,0x70),at(t,8),&v);
}
static void *uav(void *b,void *t){
    view_desc v;hv8 flags=0;zero(&v,sizeof(v));v.texture=t;v.bytes[0]=at(t,0x94)[0];
    return hv_uav(at(b,0x198),at(b,0x70),at(t,8),&v,&flags);
}

int hv_enqueue_gpu(void *b,void *view,void *inputs,hv_job *j,void **result){
    /* ht_step owns the only writer through its capture CAS. It constructs
     * conversion once, from its already admitted view and input. */
    if(!b||j->state!=HV_IDLE)return HV_INPUT_ERROR;
    j->state=HV_BUILDING;
    hv_point end=pt(view,0x2708);
    void *depth=*pointer(inputs,0x20),*velocity=*pointer(inputs,0x38);
    hv_init_type();shader_ref shader=hv_lookup(*pointer(view,0xde58),hv_shader_type,0);
    if(!plausible(shader.shader)||!plausible(shader.section)||*integer(shader.shader,0xd4)!=0)
        return publish(j,HV_SHADER_ERROR); /* One compute entry in cooked map 368. */
    j->source_extent=pt(depth,0x84);
    void *t=make_texture(b,end);
    if(!ht_motion_shape(t)||t==depth||t==velocity||!eq(pt(t,0x84),end))return publish(j,HV_TEXTURE_ERROR);
    void *p=params(b);hv_access(p,depth,0x10);
    void *d=srv(b,depth),*v=srv(b,velocity),*out=uav(b,t);
    if(!d||!v||!out)return publish(j,HV_VIEW_ERROR);
    *pointer(p,0x18)=d;*pointer(p,0x20)=v;*pointer(p,0x30)=*pointer(view,0x18);
    hv8 mode=at(hv_renderer(),0x41)[0];at(p,0x38)[0]=mode==3?2:mode;*pointer(p,0x40)=out;
    int count=*integer(b,0x170);hvptr groups=(hvptr)((j->source_extent.y+7)/8)<<32 | (unsigned)((j->source_extent.x+7)/8);
    hvptr shipping_event=0;hv_add(b,&shipping_event,&shader,p,groups);
    /* FlushAccessModeQueue in the native producer may insert a transition before our pass. */
    int last=*integer(b,0x170)-1;
    if(last<count)return publish(j,HV_PASS_ERROR);
    void *pass=((void **)*pointer(b,0x168))[last];
    if(!pass||*pointer(pass,0)!=hv_pass_vt||*pointer(pass,0x1b8)!=hv_param_meta||
       *pointer(pass,0x1c0)!=p)return publish(j,HV_PASS_ERROR);
    *result=t;return publish(j,HV_QUEUED);
}
