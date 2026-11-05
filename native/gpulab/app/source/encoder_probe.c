/* SPDX-License-Identifier: GPL-3.0-or-later
 * Proven PSBC LDS-probe lifecycle, enlarged for the actual compiled encoder. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#if !defined(_POSIX_C_SOURCE) && !defined(__FreeBSD__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "encoder_probe.h"
#include "encoder_sha256.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
enum {ARENA_BYTES=4*1024*1024,COMMAND_BYTES=131072,CAPACITY=128,
 CODE_BYTES=519036,ARGS_BYTES=400,NET_BYTES=32768,WEIGHT_BYTES=1903872,
 B3_BYTES=32768,B4_BYTES=81920,B4_ACTIVE=20480,SCRATCH_BYTES=2056};
#define ENCODER_TOKEN UINT64_C(0x454e4333)
static const char code_hash[]="4d9f6a411b0345709a3456d46c1aa09534275a2284867c2024eb6966127a5b4c";
static const char args_hash[]="038011c374e2cd766171cde4fd4f6af5a9f3efb0256b1e178a270c7bc71e5ca3";
static const char net_hash[]="677d04fe967c2ab2bfbf79993a1c2f7da9ba54890d4ecd695f0e2e44936c5e7a";
static const char weight_hash[]="f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd";
static const char b3_hash[]="6746c3cc7789c5f82164350de24852530f0b9683c94a735e0a274bf49a7e69fa";
static const char b4_hash[]="39c87779d3c21f3b187e071dd9520a9f7df496c2b8e7af5a251ff3d3e8ec1f6a";
static void flush(const void *address,size_t bytes)
{
    uintptr_t p=(uintptr_t)address&~(uintptr_t)63,end=(uintptr_t)address+bytes;
    for(;p<end;p+=64)__asm__ volatile("clflush (%0)"::"r"(p):"memory");
    __asm__ volatile("mfence":::"memory");
}
static void sh(uint32_t *p,size_t *n,uint32_t offset,const uint32_t *v,size_t count)
{ p[(*n)++]=0xc0007600|((uint32_t)count<<16);p[(*n)++]=offset;memcpy(p+*n,v,count*4);*n+=count; }
size_t encoder_commands(uint32_t *out,size_t cap,uint64_t code,uint64_t table,uint64_t fence)
{
    if(!out||cap<CAPACITY||!code||!table||!fence||(code&255)||(table&15)||
       (fence&7)||(code>>48)||(table>>32)!=2||(fence>>48))return 0;
    uint32_t p[CAPACITY],zero[4]={0};size_t n=0;
    uint32_t threads[]={256,1,1},pgm[]={(uint32_t)(code>>8),(uint32_t)(code>>40)};
    uint32_t resources[]={0x402c0013,0x00200386},user[]={0,0,(uint32_t)table};
    uint32_t dest[]={UINT32_MAX,UINT32_MAX};
    sh(p,&n,0x204,zero,3);sh(p,&n,0x207,threads,3);sh(p,&n,0x20c,pgm,2);
    sh(p,&n,0x212,resources,2);sh(p,&n,0x215,zero,1);sh(p,&n,0x228,zero,1);
    sh(p,&n,0x240,user,3);sh(p,&n,0x216,dest,2);sh(p,&n,0x219,zero,2);sh(p,&n,0x224,zero,4);
    p[n++]=0xc0031500;p[n++]=1;p[n++]=2;p[n++]=1;p[n++]=0x8041;
    p[n++]=0xc0004600;p[n++]=0x407;
    const uint32_t release[]={0xc0064900,0x0030c514,0x20000000,(uint32_t)fence,
        (uint32_t)(fence>>32),(uint32_t)ENCODER_TOKEN,0,0};
    memcpy(p+n,release,sizeof(release));n+=8;if(n>CAPACITY)return 0;
    memcpy(out,p,n*4);return n;
}
static int digest_ok(const void *data,size_t bytes,const char *expected)
{ char actual[65];encoder_sha256_hex(data,bytes,actual);return !strcmp(actual,expected); }
static int load(const struct encoder_ops *p,FILE *f,const char *name,void *to,size_t n,const char *hash)
{
    int rc=p->load(p->ctx,name,to,n);
    int good=!rc&&digest_ok(to,n,hash);
    fprintf(f,"ENCODER file=%s bytes=%zu load_rc=%d sha256_ok=%d\n",name,n,rc,good);fflush(f);
    return good?0:-20;
}
struct region {size_t offset,bytes;};
static struct region reserve(size_t *cursor,size_t bytes,size_t alignment)
{
    *cursor=(*cursor+64+alignment-1)&~(alignment-1);
    struct region r={*cursor,bytes};*cursor+=bytes+64;return r;
}
static void descriptor(uint32_t *d,const void *base,size_t bytes)
{ uint64_t a=(uintptr_t)base;d[0]=(uint32_t)a;d[1]=(uint32_t)(a>>32);d[2]=(uint32_t)bytes;d[3]=0x31016fac; }
static double e5m3(unsigned char v)
{
    unsigned bits=(unsigned)v<<7,exp=(bits>>10)&31,mant=bits&1023;
    if(exp==31)return INFINITY;
    if(!exp)return ldexp((double)mant,-24);
    return ldexp((double)(1024+mant),(int)exp-25);
}
static struct encoder_comparison compare(const unsigned char *a,const unsigned char *b,size_t n)
{
    struct encoder_comparison r={0};double error=0,reference=0;
    for(size_t i=0;i<n;i++){
        r.byte_mismatches+=a[i]!=b[i];double x=e5m3(a[i]),y=e5m3(b[i]);
        if(!__builtin_isfinite(x)||!__builtin_isfinite(y)){r.nonfinite++;continue;}
        double d=x-y;error+=d*d;reference+=y*y;if(fabs(d)>r.max_abs)r.max_abs=fabs(d);
    }
    r.relative_l2=r.nonfinite?INFINITY:reference>0?sqrt(error/reference):error>0?INFINITY:0;
    return r;
}
static int inside(size_t at,const struct region *r,size_t n)
{for(size_t i=0;i<n;i++)if(at>=r[i].offset&&at-r[i].offset<r[i].bytes)return 1;return 0;}
int encoder_probe_run_ops(FILE *f,const struct encoder_ops *p,struct encoder_result *r)
{
    if(!r)return -1;
    memset(r,0,sizeof(*r));r->rc=-1;
    if(!f||!p||!p->allocate||!p->release||!p->load||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
    uint64_t begin=p->ns(p->ctx),submitted=0;
    struct encoder_allocation arena={0},command={0};
    unsigned char *ref3=malloc(B3_BYTES),*ref4=malloc(B4_BYTES);
    if(!ref3||!ref4){r->rc=-2;goto cleanup;}
    r->stage=1;r->rc=p->allocate(p->ctx,ARENA_BYTES,&arena);if(r->rc)goto cleanup;
    if(!arena.ptr||arena.bytes<ARENA_BYTES||((uintptr_t)arena.ptr&65535)||
       ((uintptr_t)arena.ptr>>32)!=2||(((uintptr_t)arena.ptr+ARENA_BYTES-1)>>32)!=2){r->rc=-3;goto cleanup;}
    r->rc=p->allocate(p->ctx,COMMAND_BYTES,&command);if(r->rc)goto cleanup;
    if(!command.ptr||command.bytes<COMMAND_BYTES||((uintptr_t)command.ptr&65535)||
       (uintptr_t)command.ptr>(UINT64_C(1)<<48)-COMMAND_BYTES){r->rc=-4;goto cleanup;}
    unsigned char *a=arena.ptr,*c=command.ptr;memset(a,0xa5,ARENA_BYTES);memset(c,0,COMMAND_BYTES);
    size_t cursor=0;struct region regions[8];
    regions[0]=reserve(&cursor,CODE_BYTES,256);regions[1]=reserve(&cursor,160,16);
    regions[2]=reserve(&cursor,ARGS_BYTES,256);regions[3]=reserve(&cursor,NET_BYTES,256);
    regions[4]=reserve(&cursor,WEIGHT_BYTES,256);regions[5]=reserve(&cursor,B3_BYTES,256);
    regions[6]=reserve(&cursor,B4_BYTES,256);regions[7]=reserve(&cursor,SCRATCH_BYTES,256);
    if(cursor>ARENA_BYTES){r->rc=-5;goto cleanup;}
    void *code=a+regions[0].offset,*args=a+regions[2].offset,*net=a+regions[3].offset,*weights=a+regions[4].offset;
    unsigned char *b3=a+regions[5].offset,*b4=a+regions[6].offset,*scratch=a+regions[7].offset;
    uint32_t *table=(void *)(a+regions[1].offset);
    r->stage=2;
    if(load(p,f,"enc_3.code.bin",code,CODE_BYTES,code_hash)||load(p,f,"args.bin",args,ARGS_BYTES,args_hash)||
       load(p,f,"input-net-in.bin",net,NET_BYTES,net_hash)||load(p,f,"weights.bin",weights,WEIGHT_BYTES,weight_hash)||
       load(p,f,"expected-b3.bin",ref3,B3_BYTES,b3_hash)||load(p,f,"expected-b4.bin",ref4,B4_BYTES,b4_hash)){
        r->rc=-6;goto cleanup;
    }
    /* All CBV bytes remain unchanged, including opaque pointer tokens/grid. */
    memset(b3,0,B3_BYTES);memset(b4,0,B4_BYTES);memset(scratch,0,SCRATCH_BYTES);
    descriptor(table,args,ARGS_BYTES);descriptor(table+4,net,NET_BYTES);
    descriptor(table+8,weights,WEIGHT_BYTES);descriptor(table+12,(unsigned char *)weights+13312,1890560);
    descriptor(table+16,(unsigned char *)weights+9216,1894656);descriptor(table+20,(unsigned char *)weights+13568,1890304);
    descriptor(table+24,b3,B3_BYTES);descriptor(table+28,b4,B4_BYTES);
    descriptor(table+32,b4+16384,65536);descriptor(table+36,scratch,SCRATCH_BYTES);
    uint32_t saved_table[40];memcpy(saved_table,table,sizeof(saved_table));
    volatile uint64_t *fence=(void *)(c+0x1100);*fence=0;
    size_t count=encoder_commands((void *)c,CAPACITY,(uintptr_t)code,(uintptr_t)table,(uintptr_t)fence);
    if(!count){r->rc=-7;goto cleanup;}
    fprintf(f,"ENCODER prepared arena=%p code=%p code_bytes=%u table=%p b3=%p b4=%p scratch=%p dwords=%zu\n",arena.ptr,code,CODE_BYTES,(void *)table,(void *)b3,(void *)b4,(void *)scratch,count);fflush(f);
    flush(a,ARENA_BYTES);flush(c,COMMAND_BYTES);r->stage=3;
    submitted=p->ns(p->ctx);r->setup_ns=submitted-begin;r->retained=1;
    r->submit_rc=p->submit(p->ctx,(void *)c,(uint32_t)count);
    if(r->submit_rc){r->rc=-8;goto done;}
    r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-9;goto done;}
    uint64_t deadline=submitted+UINT64_C(10000000000);
    for(;;){
        flush((const void *)fence,8);r->fence=__atomic_load_n(fence,__ATOMIC_ACQUIRE);
        if(r->fence==ENCODER_TOKEN)break;
        if(p->ns(p->ctx)>deadline){r->rc=-10;goto done;}
        p->sleep_us(p->ctx,1000);
    }
    r->submit_to_fence_ns=p->ns(p->ctx)-submitted;r->completion=1;r->retained=0;r->stage=4;
    flush(a,ARENA_BYTES);
    r->b3=compare(b3,ref3,B3_BYTES);r->b4=compare(b4,ref4,B4_ACTIVE);
    memcpy(&r->diagnostic0,scratch+1024,4);memcpy(&r->diagnostic1,scratch+2052,4);
    r->overflow_errors=(r->diagnostic0!=0)+(r->diagnostic1!=0);
    r->immutable_errors+=!digest_ok(code,CODE_BYTES,code_hash);r->immutable_errors+=!digest_ok(args,ARGS_BYTES,args_hash);
    r->immutable_errors+=!digest_ok(net,NET_BYTES,net_hash);r->immutable_errors+=!digest_ok(weights,WEIGHT_BYTES,weight_hash);
    r->immutable_errors+=memcmp(table,saved_table,sizeof(saved_table))!=0;
    for(size_t i=0;i<ARENA_BYTES;i++)if(!inside(i,regions,8))r->guard_errors+=a[i]!=0xa5;
    if(p->save){
        r->debug_save_errors+=p->save(p->ctx,"result-b3.bin",b3,B3_BYTES)!=0;
        r->debug_save_errors+=p->save(p->ctx,"result-b4.bin",b4,B4_BYTES)!=0;
        r->debug_save_errors+=p->save(p->ctx,"result-scratch.bin",scratch,SCRATCH_BYTES)!=0;
    }
    r->rc=(r->b3.nonfinite||r->b4.nonfinite||!(r->b3.relative_l2<1e-3)||!(r->b4.relative_l2<1e-3)||
           r->overflow_errors||r->immutable_errors||r->guard_errors||r->debug_save_errors)?-11:0;
cleanup:
    if(command.ptr&&p->release(p->ctx,&command)){r->retained=1;r->rc=-12;goto done;}
    if(arena.ptr&&p->release(p->ctx,&arena)){r->retained=1;r->rc=-12;goto done;}
    r->cleanup=1;
done:
    free(ref3);free(ref4);r->total_ns=p->ns(p->ctx)-begin;
    fprintf(f,"ENCODER result rc=%d stage=%d completion=%u submit=%d suspend=%d retained=%u cleanup=%u guards=%u immutable=%u overflow=%u diag0=%u diag1=%u debug_save_errors=%u\n",r->rc,r->stage,r->completion,r->submit_rc,r->suspend_rc,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->overflow_errors,r->diagnostic0,r->diagnostic1,r->debug_save_errors);
    fprintf(f,"ENCODER B3 active=%u byte_mismatches=%zu nonfinite=%zu relative_l2=%.12g max_abs=%.12g\n",B3_BYTES,r->b3.byte_mismatches,r->b3.nonfinite,r->b3.relative_l2,r->b3.max_abs);
    fprintf(f,"ENCODER B4 active=%u byte_mismatches=%zu nonfinite=%zu relative_l2=%.12g max_abs=%.12g\n",B4_ACTIVE,r->b4.byte_mismatches,r->b4.nonfinite,r->b4.relative_l2,r->b4.max_abs);
    fprintf(f,"ENCODER timing_cpu_ns setup=%" PRIu64 " submit_to_fence_including_suspend_poll=%" PRIu64 " total_including_validation_cleanup=%" PRIu64 " pure_gpu_timing=0\n",r->setup_ns,r->submit_to_fence_ns,r->total_ns);fflush(f);
    return r->rc;
}
int encoder_load_exact(const char *root,const char *name,void *data,size_t n)
{
    char path[768];int count=snprintf(path,sizeof(path),"%s/%s",root,name);
    if(count<0||(size_t)count>=sizeof(path)){errno=ENAMETOOLONG;return -1;}
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -21;
    int rc=0,saved_errno=0;struct stat st;
    if(fstat(fd,&st)){rc=-26;goto finish_load;}
    if(!S_ISREG(st.st_mode)){errno=EINVAL;rc=-27;goto finish_load;}
    if(st.st_size<0||(uint64_t)st.st_size!=(uint64_t)n){errno=EINVAL;rc=-28;goto finish_load;}
    size_t used=0;
    while(used<n){
        size_t chunk=n-used;if(chunk>65536)chunk=65536;
        ssize_t got=read(fd,(unsigned char *)data+used,chunk);
        if(got<0){if(errno==EINTR)continue;rc=-24;goto finish_load;}
        if(!got){errno=EIO;rc=-22;goto finish_load;}
        used+=(size_t)got;
    }
    for(;;){
        unsigned char extra;ssize_t got=read(fd,&extra,1);
        if(got<0){if(errno==EINTR)continue;rc=-24;}
        else if(got){errno=EINVAL;rc=-23;}
        break;
    }
finish_load:
    if(rc)saved_errno=errno;
    /* Do not retry close(EINTR): descriptor state is platform-dependent and a
     * retry could close a reused descriptor. Any close error rejects the load. */
    if(close(fd)&&!rc){rc=-25;saved_errno=errno;}
    if(rc)errno=saved_errno;
    return rc;
}
int encoder_save_exact(const char *root,const char *name,const void *data,size_t n)
{
    char path[768];int count=snprintf(path,sizeof(path),"%s/%s",root,name);
    if(count<0||(size_t)count>=sizeof(path)){errno=ENAMETOOLONG;return -1;}
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -21;
    int rc=0,saved_errno=0;struct stat st;
    if(fstat(fd,&st)){rc=-26;goto finish_save;}
    if(!S_ISREG(st.st_mode)){errno=EINVAL;rc=-27;goto finish_save;}
    size_t used=0;
    while(used<n){
        size_t chunk=n-used;if(chunk>65536)chunk=65536;
        ssize_t wrote=write(fd,(const unsigned char *)data+used,chunk);
        if(wrote<0){if(errno==EINTR)continue;rc=-24;goto finish_save;}
        if(!wrote){errno=EIO;rc=-22;goto finish_save;}
        used+=(size_t)wrote;
    }
    if(fstat(fd,&st)){rc=-26;goto finish_save;}
    if(st.st_size<0||(uint64_t)st.st_size!=(uint64_t)n){errno=EIO;rc=-28;}
finish_save:
    if(rc)saved_errno=errno;
    if(close(fd)&&!rc){rc=-25;saved_errno=errno;}
    if(rc)errno=saved_errno;
    return rc;
}
