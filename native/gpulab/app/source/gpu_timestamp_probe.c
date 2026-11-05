/* SPDX-License-Identifier: GPL-3.0-or-later
 * Timestamp/ZPASS packets: PS5_Vulkan ps5vk_query.c and diagnostics.cpp,
 * copyright2026 Mihawk-99. Completion drain follows its R70/R90 path. */
#include "gpu_timestamp_probe.h"
#include <inttypes.h>
#include <string.h>
enum {BYTES=65536,WORDS=39};
static size_t ts(unsigned i){return 0x100+(size_t)i*64;}
static size_t barrier(unsigned i){return 0x200+(size_t)i*64;}
static size_t fence(unsigned i){return 0x300+(size_t)i*64;}
static size_t zpass(unsigned i){return 0x1000+(size_t)i*0x1000;}
static int address(uint64_t a,size_t bytes){return a&&!(a&7)&&a<(UINT64_C(1)<<48)&&bytes<=(UINT64_C(1)<<48)-a;}
size_t gpu_timestamp_packet(uint32_t*out,size_t cap,uint64_t a){if(!out||cap<8||!address(a,8))return 0;const uint32_t p[]={0xc0064900,0x0030c528,0x63000000,(uint32_t)a,(uint32_t)(a>>32),0,0,0};memcpy(out,p,sizeof(p));return 8;}
static void release_packet(uint32_t*p,uint64_t a,uint32_t token,uint32_t control){const uint32_t q[]={0xc0064900,control,0x20000000,(uint32_t)a,(uint32_t)(a>>32),token,0,0};memcpy(p,q,sizeof(q));}
size_t gpu_timestamp_probe_commands(uint32_t*p,size_t cap,uint64_t t,uint64_t z,uint64_t b,uint64_t f,uint32_t token){
 if(!p||cap<WORDS||!token||!address(t,8)||!address(z,0x200)||!address(b,8)||!address(f,8))return 0;
 uint64_t starts[]={t,z,b,f},sizes[]={8,0x200,8,8};for(unsigned i=0;i<4;i++)for(unsigned j=0;j<i;j++)if(starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return 0;
 p[0]=0xc0004600;p[1]=0x407; /* CS_PARTIAL_FLUSH, proven compute drain. */
 p[2]=0xc0024600;p[3]=0x115;p[4]=(uint32_t)z;p[5]=(uint32_t)(z>>32);
 if(gpu_timestamp_packet(p+6,cap-6,t)!=8)return 0;
 release_packet(p+14,b,token,0x0000c514);
 const uint32_t wait[]={0xc0079300,0x06000113,(uint32_t)b,(uint32_t)(b>>32),token,0,UINT32_MAX,0,0x40};memcpy(p+22,wait,sizeof(wait));
 release_packet(p+31,f,token,0x0030c514);return WORDS;
}
static void flush(const void*base,size_t n){uintptr_t a=(uintptr_t)base&~(uintptr_t)63,end=(uintptr_t)base+n;for(;a<end;a+=64)__asm__ volatile("clflush (%0)"::"r"(a):"memory");__asm__ volatile("mfence":::"memory");}
static uint64_t read64(const unsigned char*p){return __atomic_load_n((const uint64_t*)p,__ATOMIC_ACQUIRE);}
static int mutable(size_t at){for(unsigned i=0;i<GPU_TIMESTAMP_SAMPLES;i++)if((at>=ts(i)&&at<ts(i)+8)||(at>=barrier(i)&&at<barrier(i)+8)||(at>=fence(i)&&at<fence(i)+8)||(at>=zpass(i)&&at<zpass(i)+0x200))return 1;return 0;}
static int validate(FILE*log,const unsigned char*d,const unsigned char*c,const uint32_t*saved,unsigned through,struct gpu_timestamp_result*r){
 flush(d,BYTES);flush(c,BYTES);
 unsigned bad=0;for(size_t at=0;at<BYTES;at++)if(!mutable(at))bad+=d[at]!=0xa5;
 for(size_t at=WORDS*4;at<BYTES;at++)bad+=c[at]!=0;
 r->guard_errors+=bad;r->immutable_errors+=memcmp(c,saved,WORDS*4)!=0;
 for(unsigned i=0;i<=through;i++)if(r->sample[i].ticks&&read64(d+ts(i))!=r->sample[i].ticks){r->late_writes++;fprintf(log,"GPU_TIMESTAMP late_write sample=%u before=%"PRIu64" after=%"PRIu64"\n",i,r->sample[i].ticks,read64(d+ts(i)));}
 for(unsigned i=0;i<GPU_TIMESTAMP_SAMPLES;i++){if(i>through){if(read64(d+ts(i))||read64(d+barrier(i))||read64(d+fence(i)))r->late_writes++;}else if(read64(d+barrier(i))!=0x47540001u+i||read64(d+fence(i))!=0x47540001u+i)r->late_writes++;}
 return r->guard_errors||r->immutable_errors||r->late_writes?-1:0;
}
int gpu_timestamp_probe_run_ops(FILE*log,const struct encoder_ops*p,struct gpu_timestamp_result*r){
 if(!r)return -1;
 memset(r,0,sizeof(*r));r->rc=-1;
 if(!log||!p||!p->allocate||!p->release||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
 struct encoder_allocation data={0},command={0};unsigned char*d=NULL,*c=NULL;uint32_t saved[WORDS];
 fprintf(log,"GPU_TIMESTAMP begin samples=3 cpu_gap_us=100000 assumed_ns_per_tick=none diagnostic=1\n");
 if(p->allocate(p->ctx,BYTES,&data)||!data.ptr||data.bytes<BYTES||((uintptr_t)data.ptr&65535)||!address((uintptr_t)data.ptr,BYTES)){r->rc=-2;goto cleanup;}
 if(p->allocate(p->ctx,BYTES,&command)||!command.ptr||command.bytes<BYTES||((uintptr_t)command.ptr&65535)||!address((uintptr_t)command.ptr,BYTES)){r->rc=-2;goto cleanup;}
 if((uintptr_t)data.ptr<(uintptr_t)command.ptr+BYTES&&(uintptr_t)command.ptr<(uintptr_t)data.ptr+BYTES){r->rc=-2;goto cleanup;}
 d=data.ptr;c=command.ptr;memset(d,0xa5,BYTES);for(unsigned i=0;i<GPU_TIMESTAMP_SAMPLES;i++){memset(d+ts(i),0,8);memset(d+barrier(i),0,8);memset(d+fence(i),0,8);memset(d+zpass(i),0,0x200);}flush(d,BYTES);
 for(unsigned i=0;i<GPU_TIMESTAMP_SAMPLES;i++){
  memset(c,0,BYTES);uint32_t token=0x47540001u+i;
  if(gpu_timestamp_probe_commands((void*)c,BYTES/4,(uintptr_t)(d+ts(i)),(uintptr_t)(d+zpass(i)),(uintptr_t)(d+barrier(i)),(uintptr_t)(d+fence(i)),token)!=WORDS){r->rc=-3;goto cleanup;}
  memcpy(saved,c,sizeof(saved));flush(c,BYTES);r->sample[i].cpu_submit_ns=p->ns(p->ctx);if(!r->sample[i].cpu_submit_ns||(i&&r->sample[i].cpu_submit_ns<=r->sample[i-1].cpu_complete_ns)){r->rc=-4;goto cleanup;}
  r->retained=1;r->submit_rc=p->submit(p->ctx,(void*)c,WORDS);if(r->submit_rc){r->rc=-5;goto done;}
  r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-6;goto done;}
  for(;;){flush(d+fence(i),8);if(read64(d+fence(i))==token)break;uint64_t now=p->ns(p->ctx);if(!now||now<r->sample[i].cpu_submit_ns||now-r->sample[i].cpu_submit_ns>UINT64_C(2000000000)){r->rc=-7;goto done;}p->sleep_us(p->ctx,1000);}
  r->sample[i].cpu_complete_ns=p->ns(p->ctx);if(r->sample[i].cpu_complete_ns<r->sample[i].cpu_submit_ns||validate(log,d,c,saved,i,r)){r->rc=-8;goto done;}
  r->sample[i].ticks=read64(d+ts(i));r->samples=i+1;
  fprintf(log,"GPU_TIMESTAMP sample=%u raw_ticks=%"PRIu64" cpu_submit_ns=%"PRIu64" cpu_complete_ns=%"PRIu64" marker=%"PRIu64" barrier=%"PRIu64"\n",i,r->sample[i].ticks,r->sample[i].cpu_submit_ns,r->sample[i].cpu_complete_ns,read64(d+fence(i)),read64(d+barrier(i)));fflush(log);
  if(!r->sample[i].ticks||read64(d+barrier(i))!=token||(i&&r->sample[i].ticks<=r->sample[i-1].ticks)){r->rc=-9;goto done;}
  if(i){uint64_t delta=r->sample[i].ticks-r->sample[i-1].ticks;uint64_t dt=r->sample[i].cpu_complete_ns-r->sample[i-1].cpu_complete_ns;uint64_t lo=r->sample[i].cpu_submit_ns-r->sample[i-1].cpu_complete_ns,hi=r->sample[i].cpu_complete_ns-r->sample[i-1].cpu_submit_ns;if(!dt||!lo||!hi){r->rc=-9;goto done;}r->apparent_hz[i-1]=(double)delta*1e9/(double)dt;fprintf(log,"GPU_TIMESTAMP interval=%u raw_delta=%"PRIu64" cpu_completion_delta_ns=%"PRIu64" cpu_bracket_lo_ns=%"PRIu64" cpu_bracket_hi_ns=%"PRIu64" apparent_hz=%.12g bracket_hz_low=%.12g bracket_hz_high=%.12g exact_period_claim=0\n",i-1,delta,dt,lo,hi,r->apparent_hz[i-1],(double)delta*1e9/(double)hi,(double)delta*1e9/(double)lo);if(r->apparent_hz[i-1]<1e7||r->apparent_hz[i-1]>1e9){r->rc=-9;goto done;}}
  /* Keep all addresses live and untouched while checking for late writes.
   * This is also the100ms CPU separation before the next sample. */
  uint64_t quiet_start=p->ns(p->ctx);p->sleep_us(p->ctx,100000);uint64_t quiet_end=p->ns(p->ctx);
  fprintf(log,"GPU_TIMESTAMP stability sample=%u cpu_start_ns=%"PRIu64" cpu_end_ns=%"PRIu64"\n",i,quiet_start,quiet_end);
  if(!quiet_start||quiet_end<quiet_start||quiet_end-quiet_start<UINT64_C(90000000)||validate(log,d,c,saved,i,r)){r->rc=-10;goto done;}r->retained=0;
 }
 r->rc=0;
cleanup:
 if(command.ptr&&p->release(p->ctx,&command)){r->rc=-11;r->retained=1;goto done;}
 if(data.ptr&&p->release(p->ctx,&data)){r->rc=-11;r->retained=1;goto done;}r->cleanup=1;
done:
 fprintf(log,"GPU_TIMESTAMP result rc=%d samples=%u submit=%d suspend=%d retained=%u cleanup=%u guards=%u immutable=%u late_writes=%u exact_period_claim=0\n",r->rc,r->samples,r->submit_rc,r->suspend_rc,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->late_writes);fflush(log);return r->rc;
}
