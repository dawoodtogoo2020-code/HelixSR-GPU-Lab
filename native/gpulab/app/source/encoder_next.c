/* SPDX-License-Identifier: GPL-3.0-or-later
 * Isolated current enc_1/enc_2 fixtures using encoder_probe's proven lifecycle. */
#include "encoder_next.h"
#include "encoder_sha256.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
enum { NEXT_ARENA=4*1024*1024,NEXT_COMMAND=131072,NEXT_CAP=128,
       NEXT_ARGS=416,NEXT_WEIGHTS=1903872,NEXT_DESCRIPTORS=96 };
struct next_spec {
 unsigned stage,code_bytes,input_bytes,out0_bytes,out1_bytes;
 unsigned active0_bytes,active1_offset,active1_bytes,weight0,weight1;
 unsigned view1_offset,view1_bytes;uint32_t rsrc1,rsrc2;
 const char *code_hash,*args_hash,*input_hash,*weight_hash,*ref0_hash,*ref1_hash;
 const char *initial1_hash,*prefix1_hash,*suffix1_hash;
};
/* IMMUTABLE_SPEC_BEGIN */
static const struct next_spec specs[2] = {
    { 1, 845272, 65536, 16384, 4096, 16384, 0, 4096, 13824, 161280, 0, 4096, 0x402c0019u, 0x00180186u, "a28253fb494f274b222732807dbf7e0ac53cd7f3bf7ab753881f33c193f12f3c", "5d3437b252db8a2ad48a42cc947f602ff0b9180d087693aceab115ea78e1c9bd", "6090e6bf0888e55f20140e06d044f74af55c3f3972765942bcbab4b080a7fc7d", "f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd", "c9953234cf50057af62b410261b753e9139b850cd93c040816b5880d733fa4da", "d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0", "", "", "" },
    { 2, 844568, 4096, 4096, 81920, 4096, 16384, 1024, 161536, 456448, 16384, 65536, 0x402c001bu, 0x00180186u, "d7382c19cbddfd64412d50f0c5ed6f2a5386a6cc1806d21b9f6e16450a118f00", "47bd8f12887ed94090791f80acf13bf209e25f1d5ba48596c967db1495c5f3ad", "d061a4ab2dc67147b0a1e75f68a812f03e8511958573250796b987525f1c30f0", "f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd", "a7755302622fc3e39aa93d2de8cf288dc7308c4dfe8b396bfc32c0bee5ec7a2b", "39051b9e24f5f8d81d289f3dbca1696ce0c9e09a816102f0eb87e3b3a6e362b3", "39c87779d3c21f3b187e071dd9520a9f7df496c2b8e7af5a251ff3d3e8ec1f6a", "db5f5ea0a70839a8cda874bfb3d6f99892bfcbf2793b36ad60fd3167e47d8c87", "7a54d8c26017966538a8b881a82ddf77c071f30de7449cf49b46257d34c11f7d" }
};
/* IMMUTABLE_SPEC_END */
struct next_region {size_t offset,bytes;};
static uint64_t next_token(unsigned stage){return UINT64_C(0x454e4300)|stage;}
static int next_digest(const void *data,size_t n,const char *expected){
 char actual[65];encoder_sha256_hex(data,n,actual);return !strcmp(actual,expected);
}
static int next_load(const struct encoder_ops*p,FILE*f,const char*name,void*to,size_t n,const char*hash){
 int rc=p->load(p->ctx,name,to,n),good=!rc&&next_digest(to,n,hash);
 fprintf(f,"ENCODER_NEXT file=%s bytes=%zu load_rc=%d sha256_ok=%d\n",name,n,rc,good);fflush(f);return good?0:-1;
}
static struct next_region next_reserve(size_t*c,size_t n,size_t alignment){
 *c=(*c+64+alignment-1)&~(alignment-1);struct next_region r={*c,n};*c+=n+64;return r;
}
static int next_inside(size_t at,const struct next_region*r,size_t n){
 for(size_t i=0;i<n;i++)if(at>=r[i].offset&&at-r[i].offset<r[i].bytes)return 1;
 return 0;
}
static void next_flush(const void*base,size_t n){
 uintptr_t p=(uintptr_t)base&~(uintptr_t)63,end=(uintptr_t)base+n;
 for(;p<end;p+=64)__asm__ volatile("clflush (%0)"::"r"(p):"memory");
 __asm__ volatile("mfence":::"memory");
}
static void next_descriptor(uint32_t*d,const void*base,size_t n){
 uint64_t a=(uintptr_t)base;d[0]=(uint32_t)a;d[1]=(uint32_t)(a>>32);d[2]=(uint32_t)n;d[3]=0x31016fac;
}
static void next_sh(uint32_t*p,size_t*n,uint32_t offset,const uint32_t*v,size_t count){
 p[(*n)++]=0xc0007600|((uint32_t)count<<16);p[(*n)++]=offset;memcpy(p+*n,v,count*4);*n+=count;
}
static size_t next_commands(uint32_t*out,const struct next_spec*s,uint64_t code,uint64_t table,uint64_t fence){
 if(!out||!code||!table||!fence||(code&255)||(table&15)||(fence&7)||(code>>48)||(table>>32)!=2||(fence>>48))return 0;
 uint32_t p[NEXT_CAP],zero[4]={0};size_t n=0;
 uint32_t threads[]={128,1,1},pgm[]={(uint32_t)(code>>8),(uint32_t)(code>>40)};
 uint32_t resources[]={s->rsrc1,s->rsrc2},user[]={0,0,(uint32_t)table},dest[]={UINT32_MAX,UINT32_MAX};
 next_sh(p,&n,0x204,zero,3);next_sh(p,&n,0x207,threads,3);next_sh(p,&n,0x20c,pgm,2);
 next_sh(p,&n,0x212,resources,2);next_sh(p,&n,0x215,zero,1);next_sh(p,&n,0x228,zero,1);
 next_sh(p,&n,0x240,user,3);next_sh(p,&n,0x216,dest,2);next_sh(p,&n,0x219,zero,2);next_sh(p,&n,0x224,zero,4);
 p[n++]=0xc0031500;p[n++]=2;p[n++]=1;p[n++]=1;p[n++]=0x8041;
 p[n++]=0xc0004600;p[n++]=0x407;
 uint64_t token=next_token(s->stage);
 const uint32_t release[]={0xc0064900,0x0030c514,0x20000000,(uint32_t)fence,(uint32_t)(fence>>32),(uint32_t)token,0,0};
 memcpy(p+n,release,sizeof(release));n+=8;if(n>NEXT_CAP)return 0;memcpy(out,p,n*4);return n;
}
static double next_half(uint16_t bits){
 unsigned e=(bits>>10)&31,m=bits&1023;double v=e==31?INFINITY:e?ldexp((double)(1024+m),(int)e-25):ldexp((double)m,-24);
 return bits&32768?-v:v;
}
static struct encoder_comparison next_compare(const unsigned char*a,const unsigned char*b,size_t n){
 struct encoder_comparison r={0};double error=0,reference=0;
 for(size_t i=0;i<n;i++)r.byte_mismatches+=a[i]!=b[i];
 for(size_t i=0;i<n;i+=2){uint16_t ah,bh;memcpy(&ah,a+i,2);memcpy(&bh,b+i,2);double x=next_half(ah),y=next_half(bh);
  if(!__builtin_isfinite(x)||!__builtin_isfinite(y)){r.nonfinite++;continue;}
  double d=x-y;error+=d*d;reference+=y*y;if(fabs(d)>r.max_abs)r.max_abs=fabs(d);
 }
 r.relative_l2=r.nonfinite?INFINITY:reference>0?sqrt(error/reference):error>0?INFINITY:0;return r;
}
int encoder_next_run_ops(FILE*f,const struct encoder_ops*p,int stage,struct encoder_result*r){
 if(!r)return -1;
 memset(r,0,sizeof(*r));r->rc=-1;
 if(!f||!p||stage<1||stage>2||!p->allocate||!p->release||!p->load||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
 const struct next_spec*s=specs+(stage-1);uint64_t begin=p->ns(p->ctx),submitted=0;
 struct encoder_allocation arena={0},command={0};unsigned char*ref0=malloc(s->active0_bytes),*ref1=malloc(s->active1_bytes);
 fprintf(f,"ENCODER_NEXT begin kernel_stage=%d input_format=%s output_format=fp16 isolated_fixture=1 live_chaining=0 repaired_cuda_reference=1\n",stage,stage==1?"e5m3":"fp16");fflush(f);
 if(!ref0||!ref1){r->rc=-2;goto cleanup;}
 r->stage=1;r->rc=p->allocate(p->ctx,NEXT_ARENA,&arena);if(r->rc)goto cleanup;
 if(!arena.ptr||arena.bytes<NEXT_ARENA||((uintptr_t)arena.ptr&65535)||((uintptr_t)arena.ptr>>32)!=2||(((uintptr_t)arena.ptr+NEXT_ARENA-1)>>32)!=2){r->rc=-3;goto cleanup;}
 r->rc=p->allocate(p->ctx,NEXT_COMMAND,&command);if(r->rc)goto cleanup;
 if(!command.ptr||command.bytes<NEXT_COMMAND||((uintptr_t)command.ptr&65535)||(uintptr_t)command.ptr>(UINT64_C(1)<<48)-NEXT_COMMAND){r->rc=-4;goto cleanup;}
 unsigned char*a=arena.ptr,*c=command.ptr;memset(a,0xa5,NEXT_ARENA);memset(c,0,NEXT_COMMAND);
 size_t cursor=0;struct next_region regions[7];
 regions[0]=next_reserve(&cursor,s->code_bytes,256);regions[1]=next_reserve(&cursor,NEXT_DESCRIPTORS,16);
 regions[2]=next_reserve(&cursor,NEXT_ARGS,256);regions[3]=next_reserve(&cursor,s->input_bytes,256);
 regions[4]=next_reserve(&cursor,NEXT_WEIGHTS,256);regions[5]=next_reserve(&cursor,s->out0_bytes,256);regions[6]=next_reserve(&cursor,s->out1_bytes,256);
 if(cursor>NEXT_ARENA){r->rc=-5;goto cleanup;}
 void*code=a+regions[0].offset,*args=a+regions[2].offset,*input=a+regions[3].offset,*weights=a+regions[4].offset;
 unsigned char*out0=a+regions[5].offset,*out1=a+regions[6].offset;uint32_t*table=(void*)(a+regions[1].offset);
 r->stage=2;
 if(next_load(p,f,"shader.code.bin",code,s->code_bytes,s->code_hash)||next_load(p,f,"args.bin",args,NEXT_ARGS,s->args_hash)||
    next_load(p,f,"input.bin",input,s->input_bytes,s->input_hash)||next_load(p,f,"../weights.bin",weights,NEXT_WEIGHTS,s->weight_hash)||
    next_load(p,f,"expected-out0.bin",ref0,s->active0_bytes,s->ref0_hash)||next_load(p,f,"expected-out1.bin",ref1,s->active1_bytes,s->ref1_hash)){
  r->rc=-6;goto cleanup;
 }
 memset(out0,0,s->out0_bytes);
 if(stage==1)memset(out1,0,s->out1_bytes);
 else if(next_load(p,f,"initial-out1.bin",out1,s->out1_bytes,s->initial1_hash)){r->rc=-6;goto cleanup;}
 next_descriptor(table,args,NEXT_ARGS);next_descriptor(table+4,input,s->input_bytes);
 next_descriptor(table+8,(unsigned char*)weights+s->weight0,NEXT_WEIGHTS-s->weight0);
 next_descriptor(table+12,(unsigned char*)weights+s->weight1,NEXT_WEIGHTS-s->weight1);
 next_descriptor(table+16,out0,s->out0_bytes);next_descriptor(table+20,out1+s->view1_offset,s->view1_bytes);
 uint32_t saved_table[24];memcpy(saved_table,table,sizeof(saved_table));
 volatile uint64_t*fence=(void*)(c+0x1100);*fence=0;
 size_t count=next_commands((void*)c,s,(uintptr_t)code,(uintptr_t)table,(uintptr_t)fence);
 if(!count){r->rc=-7;goto cleanup;}uint32_t saved_commands[NEXT_CAP];memcpy(saved_commands,c,count*4);
 fprintf(f,"ENCODER_NEXT prepared stage=%d arena=%p code=%p code_bytes=%u table=%p out0=%p out1=%p dwords=%zu scratch=0\n",stage,arena.ptr,code,s->code_bytes,(void*)table,(void*)out0,(void*)out1,count);fflush(f);
 next_flush(a,NEXT_ARENA);next_flush(c,NEXT_COMMAND);r->stage=3;submitted=p->ns(p->ctx);r->setup_ns=submitted-begin;r->retained=1;
 r->submit_rc=p->submit(p->ctx,(void*)c,(uint32_t)count);if(r->submit_rc){r->rc=-8;goto done;}
 r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-9;goto done;}
 for(;;){next_flush((const void*)fence,8);r->fence=__atomic_load_n(fence,__ATOMIC_ACQUIRE);if(r->fence==next_token((unsigned)stage))break;
  if(p->ns(p->ctx)-submitted>UINT64_C(10000000000)){r->rc=-10;goto done;}p->sleep_us(p->ctx,1000);
 }
 r->completion=1;r->retained=0;r->stage=4;r->submit_to_fence_ns=p->ns(p->ctx)-submitted;next_flush(a,NEXT_ARENA);next_flush(c,NEXT_COMMAND);
 r->b3=next_compare(out0,ref0,s->active0_bytes);r->b4=next_compare(out1+s->active1_offset,ref1,s->active1_bytes);
 r->immutable_errors+=!next_digest(code,s->code_bytes,s->code_hash);r->immutable_errors+=!next_digest(args,NEXT_ARGS,s->args_hash);
 r->immutable_errors+=!next_digest(input,s->input_bytes,s->input_hash);r->immutable_errors+=!next_digest(weights,NEXT_WEIGHTS,s->weight_hash);
 r->immutable_errors+=memcmp(table,saved_table,sizeof(saved_table))!=0;r->immutable_errors+=memcmp(c,saved_commands,count*4)!=0;
 if(stage==2){r->immutable_errors+=!next_digest(out1,s->active1_offset,s->prefix1_hash);
  size_t end=s->active1_offset+s->active1_bytes;r->immutable_errors+=!next_digest(out1+end,s->out1_bytes-end,s->suffix1_hash);
 }
 for(size_t i=0;i<NEXT_ARENA;i++)if(!next_inside(i,regions,7))r->guard_errors+=a[i]!=0xa5;
 for(size_t i=count*4;i<NEXT_COMMAND;i++)if(i<0x1100||i>=0x1108)r->guard_errors+=c[i]!=0;
 if(p->save){r->debug_save_errors+=p->save(p->ctx,"result-out0.bin",out0,s->out0_bytes)!=0;r->debug_save_errors+=p->save(p->ctx,"result-out1.bin",out1,s->out1_bytes)!=0;}
 r->rc=(r->b3.byte_mismatches||r->b4.byte_mismatches||r->b3.nonfinite||r->b4.nonfinite||!(r->b3.relative_l2<1e-3)||!(r->b4.relative_l2<1e-3)||r->guard_errors||r->immutable_errors||r->debug_save_errors)?-11:0;
cleanup:
 if(command.ptr&&p->release(p->ctx,&command)){r->retained=1;r->rc=-12;goto done;}
 if(arena.ptr&&p->release(p->ctx,&arena)){r->retained=1;r->rc=-12;goto done;}
 r->cleanup=1;
done:
 free(ref0);free(ref1);r->total_ns=p->ns(p->ctx)-begin;
 fprintf(f,"ENCODER_NEXT result kernel_stage=%d rc=%d phase=%d completion=%u submit=%d suspend=%d retained=%u cleanup=%u guards=%u immutable=%u save_errors=%u\n",stage,r->rc,r->stage,r->completion,r->submit_rc,r->suspend_rc,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->debug_save_errors);
 fprintf(f,"ENCODER_NEXT out0 bytes=%u mismatches=%zu nonfinite=%zu relative_l2=%.12g out1 offset=%u bytes=%u mismatches=%zu nonfinite=%zu relative_l2=%.12g\n",s->active0_bytes,r->b3.byte_mismatches,r->b3.nonfinite,r->b3.relative_l2,s->active1_offset,s->active1_bytes,r->b4.byte_mismatches,r->b4.nonfinite,r->b4.relative_l2);
 fprintf(f,"ENCODER_NEXT timing_cpu_ns setup=%"PRIu64" submit_to_fence_including_suspend_poll=%"PRIu64" total=%"PRIu64" pure_gpu_timing=0\n",r->setup_ns,r->submit_to_fence_ns,r->total_ns);fflush(f);return r->rc;
}
