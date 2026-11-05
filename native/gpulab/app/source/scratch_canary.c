/* SPDX-License-Identifier: GPL-3.0-or-later
 * Minimal three-stage assembled compute scratch diagnostic, no model kernel. */
#include "scratch_canary.h"
#include "model_scratch.h"
#include "encoder_sha256.h"
#include <inttypes.h>
#include <string.h>
enum {SC_ARENA=4194304,SC_COMMAND=131072,SC_CAP=128,SC_OUTPUT=4096,
 SC_SCRATCH_TOTAL=25296896,SC_SCRATCH_OFFSET=65536,SC_SCRATCH_PAYLOAD=25159680};
#define SC_TOKEN_BASE UINT32_C(0x53430000)
struct canary_spec {unsigned stage,bytes;const char*hash;};
struct sc_region {size_t offset,bytes;};
static const struct canary_spec specs[3] = {
 {1,132,"bac5aa3e54b4415d5657a1f6f5d3c4c55fb032e0deb8b636ed84da2368393a70"},
 {2,160,"d65ece96f66b21b975ff3ce48c275725c119847d8250c7ab401fb2ab09d12286"},
 {3,160,"4236b3355958ee42a322416ec60eecddf055dc5abed1687918d8cb70842dfd5f"}
};
static int sc_digest(const void *data,size_t n,const char *expected){
 char actual[65];encoder_sha256_hex(data,n,actual);return !strcmp(actual,expected);
}
static int sc_load(const struct encoder_ops*p,FILE*f,const char*name,void*to,size_t n,const char*hash){
 int rc=p->load(p->ctx,name,to,n),good=!rc&&sc_digest(to,n,hash);
 fprintf(f,"SCRATCH_CANARY file=%s bytes=%zu load_rc=%d sha256_ok=%d\n",name,n,rc,good);fflush(f);return good?0:-1;
}
static struct sc_region sc_reserve(size_t*c,size_t n,size_t alignment){
 *c=(*c+64+alignment-1)&~(alignment-1);struct sc_region r={*c,n};*c+=n+64;return r;
}
static int sc_inside(size_t at,const struct sc_region*r,size_t n){
 for(size_t i=0;i<n;i++)if(at>=r[i].offset&&at-r[i].offset<r[i].bytes)return 1;
 return 0;
}
static void sc_flush(const void*base,size_t n){
 uintptr_t p=(uintptr_t)base&~(uintptr_t)63,end=(uintptr_t)base+n;
 for(;p<end;p+=64)__asm__ volatile("clflush (%0)"::"r"(p):"memory");
 __asm__ volatile("mfence":::"memory");
}
static void sc_descriptor(uint32_t*d,const void*base,size_t n){
 uint64_t a=(uintptr_t)base;d[0]=(uint32_t)a;d[1]=(uint32_t)(a>>32);d[2]=(uint32_t)n;d[3]=0x31016fac;
}
static void sc_sh(uint32_t*p,size_t*n,uint32_t offset,const uint32_t*v,size_t count){
 p[(*n)++]=0xc0007600|((uint32_t)count<<16);p[(*n)++]=offset;memcpy(p+*n,v,count*4);*n+=count;
}
static size_t sc_commands(uint32_t*out,uint64_t code,uint64_t table,uint64_t fence,const struct model_scratch_plan*s,unsigned stage){
 if(!out||!s||!s->bound||!code||!table||!fence||(code&255)||(table&15)||(fence&7)||(code>>48)||(table>>32)!=2||(fence>>48))return 0;
 uint32_t p[SC_CAP],zero[4]={0};size_t n=0;
 uint32_t threads[]={128,1,1},pgm[]={(uint32_t)(code>>8),(uint32_t)(code>>40)};
 uint32_t resources[]={s->rsrc1_value,s->rsrc2_value},user[]={s->user_data[0],s->user_data[1],(uint32_t)table},dest[]={UINT32_MAX,UINT32_MAX};
 sc_sh(p,&n,0x204,zero,3);sc_sh(p,&n,0x207,threads,3);sc_sh(p,&n,0x20c,pgm,2);
 sc_sh(p,&n,0x212,resources,2);sc_sh(p,&n,0x215,zero,1);sc_sh(p,&n,0x228,zero,1);
 sc_sh(p,&n,0x240,user,3);sc_sh(p,&n,0x216,dest,2);sc_sh(p,&n,0x219,zero,2);sc_sh(p,&n,0x224,zero,4);
 sc_sh(p,&n,0x218,&s->tmpring_value,1);
 p[n++]=0xc0031500;p[n++]=1;p[n++]=1;p[n++]=1;p[n++]=0x8041;
 p[n++]=0xc0004600;p[n++]=0x407;
 sc_sh(p,&n,0x218,zero,1);
 const uint32_t release[]={0xc0064900,0x0030c514,0x20000000,(uint32_t)fence,(uint32_t)(fence>>32),SC_TOKEN_BASE|stage,0,0};
 memcpy(p+n,release,sizeof(release));n+=8;if(n>SC_CAP)return 0;memcpy(out,p,n*4);return n;
}
static void sc_validate(FILE*f,unsigned stage,const uint32_t*output,uint64_t base,const struct model_scratch_plan*p,struct encoder_result*r){
 uint32_t expected[1024],waves[4]={0};
 for(unsigned i=0;i<128;i++){
  const uint32_t*row=output+i*8;uint32_t offset=row[3];
  if((uint64_t)offset+128>p->allocation_bytes)r->overflow_errors++;
  if((i&31)==0)waves[i>>5]=offset;else if(offset!=waves[i>>5])r->overflow_errors++;
  uint64_t low=(uint64_t)(uint32_t)base+offset;
  uint32_t ex[8]={0x53433000u|stage,p->user_data[0],p->user_data[1],offset,(uint32_t)low,
   stage==3?(uint32_t)((base+offset)>>32):p->user_data[1]+0x7fff0000u+(uint32_t)(low>>32),i,stage==1?0x2468ace0u:(0x13579bdfu^i)};
  memcpy(expected+i*8,ex,sizeof(ex));
 }
 const unsigned char*a=(const void*)output,*b=(const void*)expected;
 for(unsigned i=0;i<SC_OUTPUT;i++)r->b3.byte_mismatches+=a[i]!=b[i];
 for(unsigned i=0;i<4;i++){
  const uint32_t*row=output+i*32*8;
  fprintf(f,"SCRATCH_CANARY wave=%u incoming_lo=0x%08x incoming_hi=0x%08x offset=0x%08x flat_lo=0x%08x flat_hi=0x%08x sample=0x%08x\n",i,row[1],row[2],row[3],row[4],row[5],row[7]);
 }
}
int scratch_canary_run_ops(FILE*f,const struct encoder_ops*p,int stage,struct encoder_result*r){
 if(!r)return -1;
 memset(r,0,sizeof(*r));r->rc=-1;
 if(!f||!p||stage<1||stage>3||!p->allocate||!p->release||!p->load||!p->save||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
 const struct canary_spec*spec=specs+stage-1;uint64_t begin=p->ns(p->ctx),submitted=0;
 struct encoder_allocation arena={0},scratch={0},command={0};
 fprintf(f,"SCRATCH_CANARY begin stage=%d register_readback=1 scratch_memory=%d model_kernel=0 fullring4095=1 stride6144=1\n",stage,stage>=2);fflush(f);
 r->stage=1;r->rc=p->allocate(p->ctx,SC_ARENA,&arena);if(r->rc)goto cleanup;
 if(!arena.ptr||arena.bytes<SC_ARENA||((uintptr_t)arena.ptr&65535)||((uintptr_t)arena.ptr>>32)!=2||(((uintptr_t)arena.ptr+SC_ARENA-1)>>32)!=2){r->rc=-3;goto cleanup;}
 r->rc=p->allocate(p->ctx,SC_SCRATCH_TOTAL,&scratch);if(r->rc)goto cleanup;
 if(!scratch.ptr||scratch.bytes<SC_SCRATCH_TOTAL||((uintptr_t)scratch.ptr&65535)||(uintptr_t)scratch.ptr>(UINT64_C(1)<<48)-SC_SCRATCH_TOTAL){r->rc=-4;goto cleanup;}
 r->rc=p->allocate(p->ctx,SC_COMMAND,&command);if(r->rc)goto cleanup;
 if(!command.ptr||command.bytes<SC_COMMAND||((uintptr_t)command.ptr&65535)||(uintptr_t)command.ptr>(UINT64_C(1)<<48)-SC_COMMAND){r->rc=-4;goto cleanup;}
 unsigned char*a=arena.ptr,*s=scratch.ptr,*c=command.ptr;
 memset(a,0xa5,SC_ARENA);memset(s,0xa5,SC_SCRATCH_TOTAL);memset(c,0,SC_COMMAND);memset(s+SC_SCRATCH_OFFSET,0,SC_SCRATCH_PAYLOAD);
 struct model_scratch_plan plan;uint64_t payload=(uintptr_t)s+SC_SCRATCH_OFFSET;
 if(model_scratch_full_ring_build(payload,SC_SCRATCH_PAYLOAD,&plan)||plan.allocation_bytes!=SC_SCRATCH_PAYLOAD||
 plan.user_data[0]!=(uint32_t)payload||plan.user_data[1]!=((uint32_t)(payload>>32)|0x80000000u)){
  r->rc=-5;goto cleanup;
 }
 size_t cursor=0;struct sc_region regions[3];regions[0]=sc_reserve(&cursor,spec->bytes,256);regions[1]=sc_reserve(&cursor,16,16);regions[2]=sc_reserve(&cursor,SC_OUTPUT,256);
 if(cursor>SC_ARENA){r->rc=-5;goto cleanup;}
 void*code=a+regions[0].offset;uint32_t*table=(void*)(a+regions[1].offset),*output=(void*)(a+regions[2].offset);memset(output,0xcd,SC_OUTPUT);
 char code_name[32],result_name[32];snprintf(code_name,sizeof(code_name),"stage%d.code.bin",stage);snprintf(result_name,sizeof(result_name),"stage%d-result.bin",stage);
 r->stage=2;if(sc_load(p,f,code_name,code,spec->bytes,spec->hash)){r->rc=-6;goto cleanup;}
 sc_descriptor(table,output,SC_OUTPUT);uint32_t saved_table[4];memcpy(saved_table,table,sizeof(saved_table));
 volatile uint64_t*fence=(void*)(c+0x1100);*fence=0;
 size_t count=sc_commands((void*)c,(uintptr_t)code,(uintptr_t)table,(uintptr_t)fence,&plan,(unsigned)stage);
 if(!count){r->rc=-7;goto cleanup;}uint32_t saved_commands[SC_CAP];memcpy(saved_commands,c,count*4);
 fprintf(f,"SCRATCH_CANARY prepared stage=%d code=%p code_bytes=%u table=%p output=%p scratch=%p user_hi=0x%x tmpring=0x%x dwords=%zu\n",stage,code,spec->bytes,(void*)table,(void*)output,(void*)(uintptr_t)payload,plan.user_data[1],plan.tmpring_value,count);fflush(f);
 sc_flush(a,SC_ARENA);sc_flush(s,SC_SCRATCH_TOTAL);sc_flush(c,SC_COMMAND);r->stage=3;submitted=p->ns(p->ctx);r->setup_ns=submitted-begin;r->retained=1;
 r->submit_rc=p->submit(p->ctx,(void*)c,(uint32_t)count);if(r->submit_rc){r->rc=-8;goto done;}
 r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-9;goto done;}
 for(;;){sc_flush((const void*)fence,8);r->fence=__atomic_load_n(fence,__ATOMIC_ACQUIRE);if(r->fence==(SC_TOKEN_BASE|(unsigned)stage))break;
  if(p->ns(p->ctx)-submitted>UINT64_C(10000000000)){r->rc=-10;goto done;}p->sleep_us(p->ctx,1000);
 }
 r->completion=1;r->retained=0;r->stage=4;r->submit_to_fence_ns=p->ns(p->ctx)-submitted;
 sc_flush(a,SC_ARENA);sc_flush(s,SC_SCRATCH_TOTAL);sc_flush(c,SC_COMMAND);
 sc_validate(f,(unsigned)stage,output,payload,&plan,r);
 r->immutable_errors+=!sc_digest(code,spec->bytes,spec->hash);r->immutable_errors+=memcmp(table,saved_table,sizeof(saved_table))!=0;r->immutable_errors+=memcmp(c,saved_commands,count*4)!=0;
 if(stage==1)for(size_t i=0;i<SC_SCRATCH_PAYLOAD;i++)if(s[SC_SCRATCH_OFFSET+i]!=0){r->immutable_errors++;break;}
 for(size_t i=0;i<SC_ARENA;i++)if(!sc_inside(i,regions,3))r->guard_errors+=a[i]!=0xa5;
 for(size_t i=0;i<SC_SCRATCH_OFFSET;i++)r->guard_errors+=s[i]!=0xa5;
 for(size_t i=SC_SCRATCH_OFFSET+SC_SCRATCH_PAYLOAD;i<SC_SCRATCH_TOTAL;i++)r->guard_errors+=s[i]!=0xa5;
 for(size_t i=count*4;i<SC_COMMAND;i++)if(i<0x1100||i>=0x1108)r->guard_errors+=c[i]!=0;
 r->debug_save_errors+=p->save(p->ctx,result_name,output,SC_OUTPUT)!=0;
 r->rc=(r->b3.byte_mismatches||r->overflow_errors||r->guard_errors||r->immutable_errors||r->debug_save_errors)?-11:0;
cleanup:
 if(command.ptr&&p->release(p->ctx,&command)){r->retained=1;r->rc=-12;goto done;}
 if(scratch.ptr&&p->release(p->ctx,&scratch)){r->retained=1;r->rc=-12;goto done;}
 if(arena.ptr&&p->release(p->ctx,&arena)){r->retained=1;r->rc=-12;goto done;}
 r->cleanup=1;
done:
 r->total_ns=p->ns(p->ctx)-begin;
 fprintf(f,"SCRATCH_CANARY result stage=%d rc=%d phase=%d completion=%u submit=%d suspend=%d retained=%u cleanup=%u guards=%u immutable=%u protocol=%u mismatched_bytes=%zu save_errors=%u setup_cpu_ns=%"PRIu64" submit_to_fence_ns=%"PRIu64" total_cpu_ns=%"PRIu64"\n",stage,r->rc,r->stage,r->completion,r->submit_rc,r->suspend_rc,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->overflow_errors,r->b3.byte_mismatches,r->debug_save_errors,r->setup_ns,r->submit_to_fence_ns,r->total_ns);fflush(f);return r->rc;
}
