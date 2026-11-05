/* SPDX-License-Identifier: GPL-3.0-or-later
 * Isolated captured k10 with explicit full-ring scratch and retained lifecycle. */
#include "k10_scratch_probe.h"
#include "model_scratch.h"
#include "encoder_sha256.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
enum { K10_ARENA=4*1024*1024,K10_COMMAND=131072,K10_CAP=128,K10_CODE=871744,
 K10_ARGS=672,K10_B7=4096,K10_B4=81920,K10_WEIGHTS=133632,K10_DESCRIPTORS=128,
 K10_ACTIVE_OFFSET=20480,K10_ACTIVE=4096,K10_SCRATCH_PAYLOAD=25159680,
 K10_SCRATCH_OFFSET=65536,K10_SCRATCH_TOTAL=25296896 };
#define K10_TOKEN UINT32_C(0x4b313053)
struct k10_region {size_t offset,bytes;};
static const char code_hash[]="7434b89368210e33b9772cd8e5d6ab3534cd55e0d604f55f70f6dab5ef3a0006";
static const char args_hash[]="c630cb0083fd16621a867844a801cf42b376b5fcd546479545a66340e9d03e4e";
static const char b7_hash[]="a7755302622fc3e39aa93d2de8cf288dc7308c4dfe8b396bfc32c0bee5ec7a2b";
static const char b4_hash[]="16949f3d40b7fe429c226682eb66deacc9abba02affa191cfaa9455fd4f6532c";
static const char weight_hash[]="6b336979a20c4b0a80380ab2d65fcb7bd8149ad0418f05a0be2392c7501ef76e";
static const char expected_hash[]="5b5f539915686fb4efcc1356ea78c712cee82782512965b6b54bffe287aaab54";
static const char prefix_hash[]="b7d1b852dec3e691db6161c4d932a9c783f28bcf03875ceb5be9f3066f6a34a3";
static const char suffix_hash[]="4f81904a9b06c58572a0e5769b3b4ffb99e7bd4be88ee8c2b64a804f483d9dc6";
static int k10_digest(const void *data,size_t n,const char *expected){
 char actual[65];encoder_sha256_hex(data,n,actual);return !strcmp(actual,expected);
}
static int k10_load(const struct encoder_ops*p,FILE*f,const char*name,void*to,size_t n,const char*hash){
 int rc=p->load(p->ctx,name,to,n),good=!rc&&k10_digest(to,n,hash);
 fprintf(f,"K10_SCRATCH file=%s bytes=%zu load_rc=%d sha256_ok=%d\n",name,n,rc,good);fflush(f);return good?0:-1;
}
static struct k10_region k10_reserve(size_t*c,size_t n,size_t alignment){
 *c=(*c+64+alignment-1)&~(alignment-1);struct k10_region r={*c,n};*c+=n+64;return r;
}
static int k10_inside(size_t at,const struct k10_region*r,size_t n){
 for(size_t i=0;i<n;i++)if(at>=r[i].offset&&at-r[i].offset<r[i].bytes)return 1;
 return 0;
}
static void k10_flush(const void*base,size_t n){
 uintptr_t p=(uintptr_t)base&~(uintptr_t)63,end=(uintptr_t)base+n;
 for(;p<end;p+=64)__asm__ volatile("clflush (%0)"::"r"(p):"memory");
 __asm__ volatile("mfence":::"memory");
}
static void k10_descriptor(uint32_t*d,const void*base,size_t n){
 uint64_t a=(uintptr_t)base;d[0]=(uint32_t)a;d[1]=(uint32_t)(a>>32);d[2]=(uint32_t)n;d[3]=0x31016fac;
}
static void k10_sh(uint32_t*p,size_t*n,uint32_t offset,const uint32_t*v,size_t count){
 p[(*n)++]=0xc0007600|((uint32_t)count<<16);p[(*n)++]=offset;memcpy(p+*n,v,count*4);*n+=count;
}
static double k10_half(uint16_t bits){
 unsigned e=(bits>>10)&31,m=bits&1023;double v=e==31?INFINITY:e?ldexp((double)(1024+m),(int)e-25):ldexp((double)m,-24);
 return bits&32768?-v:v;
}
static struct encoder_comparison k10_compare(const unsigned char*a,const unsigned char*b,size_t n){
 struct encoder_comparison r={0};double error=0,reference=0;
 for(size_t i=0;i<n;i++)r.byte_mismatches+=a[i]!=b[i];
 for(size_t i=0;i<n;i+=2){uint16_t ah,bh;memcpy(&ah,a+i,2);memcpy(&bh,b+i,2);double x=k10_half(ah),y=k10_half(bh);
  if(!__builtin_isfinite(x)||!__builtin_isfinite(y)){r.nonfinite++;continue;}
  double d=x-y;error+=d*d;reference+=y*y;if(fabs(d)>r.max_abs)r.max_abs=fabs(d);
 }
 r.relative_l2=r.nonfinite?INFINITY:reference>0?sqrt(error/reference):error>0?INFINITY:0;return r;
}
static size_t k10_commands(uint32_t*out,uint64_t code,uint64_t table,uint64_t fence,const struct model_scratch_plan*s){
 if(!out||!s||!s->bound||!code||!table||!fence||(code&255)||(table&15)||(fence&7)||(code>>48)||(table>>32)!=2||(fence>>48))return 0;
 uint32_t p[K10_CAP],zero[4]={0};size_t n=0;
 uint32_t threads[]={128,1,1},pgm[]={(uint32_t)(code>>8),(uint32_t)(code>>40)};
 uint32_t resources[]={s->rsrc1_value,s->rsrc2_value},user[]={s->user_data[0],s->user_data[1],(uint32_t)table},dest[]={UINT32_MAX,UINT32_MAX};
 k10_sh(p,&n,0x204,zero,3);k10_sh(p,&n,0x207,threads,3);k10_sh(p,&n,0x20c,pgm,2);
 k10_sh(p,&n,0x212,resources,2);k10_sh(p,&n,0x215,zero,1);k10_sh(p,&n,0x228,zero,1);
 k10_sh(p,&n,0x240,user,3);k10_sh(p,&n,0x216,dest,2);k10_sh(p,&n,0x219,zero,2);k10_sh(p,&n,0x224,zero,4);
 k10_sh(p,&n,0x218,&s->tmpring_value,1);
 p[n++]=0xc0031500;p[n++]=1;p[n++]=1;p[n++]=1;p[n++]=0x8041;
 /* Drain compute before changing the live scratch stride, then clear it
  * before the final completion token permits any backing-memory release. */
 p[n++]=0xc0004600;p[n++]=0x407;
 k10_sh(p,&n,0x218,zero,1);
 const uint32_t release[]={0xc0064900,0x0030c514,0x20000000,(uint32_t)fence,(uint32_t)(fence>>32),K10_TOKEN,0,0};
 memcpy(p+n,release,sizeof(release));n+=8;if(n>K10_CAP)return 0;memcpy(out,p,n*4);return n;
}
int k10_scratch_probe_run_ops(FILE*f,const struct encoder_ops*p,struct encoder_result*r){
 if(!r)return -1;
 memset(r,0,sizeof(*r));r->rc=-1;
 if(!f||!p||!p->allocate||!p->release||!p->load||!p->save||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
 uint64_t begin=p->ns(p->ctx),submitted=0;
 struct encoder_allocation arena={0},scratch={0},command={0};unsigned char*reference=malloc(K10_ACTIVE);
 fprintf(f,"K10_SCRATCH begin isolated_fixture=1 launch=14 live_chaining=0 output_format=fp16 repaired_cuda_reference=1 full_ring=4095 scratch_payload=%u\n",K10_SCRATCH_PAYLOAD);fflush(f);
 if(!reference){r->rc=-2;goto cleanup;}
 r->stage=1;r->rc=p->allocate(p->ctx,K10_ARENA,&arena);if(r->rc)goto cleanup;
 if(!arena.ptr||arena.bytes<K10_ARENA||((uintptr_t)arena.ptr&65535)||((uintptr_t)arena.ptr>>32)!=2||(((uintptr_t)arena.ptr+K10_ARENA-1)>>32)!=2){r->rc=-3;goto cleanup;}
 r->rc=p->allocate(p->ctx,K10_SCRATCH_TOTAL,&scratch);if(r->rc)goto cleanup;
 if(!scratch.ptr||scratch.bytes<K10_SCRATCH_TOTAL||((uintptr_t)scratch.ptr&65535)||(uintptr_t)scratch.ptr>(UINT64_C(1)<<48)-K10_SCRATCH_TOTAL){r->rc=-4;goto cleanup;}
 r->rc=p->allocate(p->ctx,K10_COMMAND,&command);if(r->rc)goto cleanup;
 if(!command.ptr||command.bytes<K10_COMMAND||((uintptr_t)command.ptr&65535)||(uintptr_t)command.ptr>(UINT64_C(1)<<48)-K10_COMMAND){r->rc=-4;goto cleanup;}
 unsigned char*a=arena.ptr,*s=scratch.ptr,*c=command.ptr;
 memset(a,0xa5,K10_ARENA);memset(s,0xa5,K10_SCRATCH_TOTAL);memset(c,0,K10_COMMAND);
 memset(s+K10_SCRATCH_OFFSET,0,K10_SCRATCH_PAYLOAD);
 struct model_scratch_plan plan;
 if(model_scratch_full_ring_build((uintptr_t)s+K10_SCRATCH_OFFSET,K10_SCRATCH_PAYLOAD,&plan)||plan.allocation_bytes!=K10_SCRATCH_PAYLOAD){r->rc=-5;goto cleanup;}
 size_t cursor=0;struct k10_region regions[6];
 regions[0]=k10_reserve(&cursor,K10_CODE,256);regions[1]=k10_reserve(&cursor,K10_DESCRIPTORS,16);regions[2]=k10_reserve(&cursor,K10_ARGS,256);
 regions[3]=k10_reserve(&cursor,K10_B7,256);regions[4]=k10_reserve(&cursor,K10_WEIGHTS,256);regions[5]=k10_reserve(&cursor,K10_B4,256);
 if(cursor>K10_ARENA){r->rc=-5;goto cleanup;}
 void*code=a+regions[0].offset,*args=a+regions[2].offset,*b7=a+regions[3].offset,*weights=a+regions[4].offset;
 unsigned char*b4=a+regions[5].offset;uint32_t*table=(void*)(a+regions[1].offset);r->stage=2;
 if(k10_load(p,f,"shader.code.bin",code,K10_CODE,code_hash)||k10_load(p,f,"args.bin",args,K10_ARGS,args_hash)||
 k10_load(p,f,"input-b7.bin",b7,K10_B7,b7_hash)||k10_load(p,f,"initial-b4.bin",b4,K10_B4,b4_hash)||
 k10_load(p,f,"weights-slice.bin",weights,K10_WEIGHTS,weight_hash)||k10_load(p,f,"expected-active.bin",reference,K10_ACTIVE,expected_hash)){
  r->rc=-6;goto cleanup;
 }
 k10_descriptor(table,args,K10_ARGS);k10_descriptor(table+4,b4+16384,65536);k10_descriptor(table+8,b7,K10_B7);
 const unsigned offsets[]={0,32768,33024,65792};for(unsigned i=0;i<4;i++)k10_descriptor(table+12+i*4,(unsigned char*)weights+offsets[i],K10_WEIGHTS-offsets[i]);
 k10_descriptor(table+28,b4+K10_ACTIVE_OFFSET,K10_B4-K10_ACTIVE_OFFSET);
 uint32_t saved_table[32];memcpy(saved_table,table,sizeof(saved_table));
 volatile uint64_t*fence=(void*)(c+0x1100);*fence=0;
 size_t count=k10_commands((void*)c,(uintptr_t)code,(uintptr_t)table,(uintptr_t)fence,&plan);
 if(!count){r->rc=-7;goto cleanup;}uint32_t saved_commands[K10_CAP];memcpy(saved_commands,c,count*4);
 fprintf(f,"K10_SCRATCH prepared code=%p table=%p b4=%p scratch=%p payload=%u tmpring=0x%x dwords=%zu clear_ring_before_fence=1\n",code,(void*)table,(void*)b4,(void*)(s+K10_SCRATCH_OFFSET),K10_SCRATCH_PAYLOAD,plan.tmpring_value,count);fflush(f);
 k10_flush(a,K10_ARENA);k10_flush(s,K10_SCRATCH_TOTAL);k10_flush(c,K10_COMMAND);
 r->stage=3;submitted=p->ns(p->ctx);r->setup_ns=submitted-begin;r->retained=1;
 r->submit_rc=p->submit(p->ctx,(void*)c,(uint32_t)count);if(r->submit_rc){r->rc=-8;goto done;}
 r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-9;goto done;}
 for(;;){k10_flush((const void*)fence,8);r->fence=__atomic_load_n(fence,__ATOMIC_ACQUIRE);if(r->fence==K10_TOKEN)break;
  if(p->ns(p->ctx)-submitted>UINT64_C(10000000000)){r->rc=-10;goto done;}p->sleep_us(p->ctx,1000);
 }
 r->completion=1;r->retained=0;r->stage=4;r->submit_to_fence_ns=p->ns(p->ctx)-submitted;
 k10_flush(a,K10_ARENA);k10_flush(s,K10_SCRATCH_TOTAL);k10_flush(c,K10_COMMAND);
 r->b4=k10_compare(b4+K10_ACTIVE_OFFSET,reference,K10_ACTIVE);
 r->immutable_errors+=!k10_digest(code,K10_CODE,code_hash);r->immutable_errors+=!k10_digest(args,K10_ARGS,args_hash);
 r->immutable_errors+=!k10_digest(b7,K10_B7,b7_hash);r->immutable_errors+=!k10_digest(weights,K10_WEIGHTS,weight_hash);
 r->immutable_errors+=!k10_digest(b4,K10_ACTIVE_OFFSET,prefix_hash);r->immutable_errors+=!k10_digest(b4+K10_ACTIVE_OFFSET+K10_ACTIVE,K10_B4-K10_ACTIVE_OFFSET-K10_ACTIVE,suffix_hash);
 r->immutable_errors+=memcmp(table,saved_table,sizeof(saved_table))!=0;r->immutable_errors+=memcmp(c,saved_commands,count*4)!=0;
 for(size_t i=0;i<K10_ARENA;i++)if(!k10_inside(i,regions,6))r->guard_errors+=a[i]!=0xa5;
 for(size_t i=0;i<K10_SCRATCH_OFFSET;i++)r->guard_errors+=s[i]!=0xa5;
 for(size_t i=K10_SCRATCH_OFFSET+K10_SCRATCH_PAYLOAD;i<K10_SCRATCH_TOTAL;i++)r->guard_errors+=s[i]!=0xa5;
 for(size_t i=count*4;i<K10_COMMAND;i++)if(i<0x1100||i>=0x1108)r->guard_errors+=c[i]!=0;
 r->debug_save_errors+=p->save(p->ctx,"result-b4.bin",b4,K10_B4)!=0;
 r->debug_save_errors+=p->save(p->ctx,"result-b4-active.bin",b4+K10_ACTIVE_OFFSET,K10_ACTIVE)!=0;
 r->rc=(r->b4.byte_mismatches||r->b4.nonfinite||!(r->b4.relative_l2<1e-3)||r->guard_errors||r->immutable_errors||r->debug_save_errors)?-11:0;
cleanup:
 if(command.ptr&&p->release(p->ctx,&command)){r->retained=1;r->rc=-12;goto done;}
 if(scratch.ptr&&p->release(p->ctx,&scratch)){r->retained=1;r->rc=-12;goto done;}
 if(arena.ptr&&p->release(p->ctx,&arena)){r->retained=1;r->rc=-12;goto done;}
 r->cleanup=1;
done:
 free(reference);r->total_ns=p->ns(p->ctx)-begin;
 fprintf(f,"K10_SCRATCH result rc=%d phase=%d completion=%u submit=%d suspend=%d retained=%u cleanup=%u guards=%u immutable=%u save_errors=%u\n",r->rc,r->stage,r->completion,r->submit_rc,r->suspend_rc,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->debug_save_errors);
 fprintf(f,"K10_SCRATCH b4 offset=%u bytes=%u mismatches=%zu nonfinite=%zu relative_l2=%.12g setup_cpu_ns=%"PRIu64" submit_to_fence_ns=%"PRIu64" total_cpu_ns=%"PRIu64" pure_gpu_timing=0\n",K10_ACTIVE_OFFSET,K10_ACTIVE,r->b4.byte_mismatches,r->b4.nonfinite,r->b4.relative_l2,r->setup_ns,r->submit_to_fence_ns,r->total_ns);fflush(f);return r->rc;
}
