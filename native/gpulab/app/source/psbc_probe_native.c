/* SPDX-License-Identifier: GPL-3.0-or-later
 * Direct-memory ABI from PS5_Vulkan/.deps/native/.../ps5platform/kernel.h.
 * Allocation follows the working RADV PS5 radv_ps5_memory_alloc window32 path:
 * type12, protection0x33, non-fixed null address, then validate high32 window.
 * No guessed batch-map structure, MAP_FIXED overwrite, or heap GPU pointers. */
#include "psbc_probe.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
extern int32_t sceKernelAllocateDirectMemory(int64_t,int64_t,size_t,size_t,int,int64_t *);
extern int64_t sceKernelGetDirectMemorySize(void);
extern int32_t sceKernelReleaseDirectMemory(int64_t,size_t);
extern int32_t sceKernelMapDirectMemory(void **,size_t,int,int,int64_t,size_t);
extern int32_t sceKernelMunmap(void *,size_t);
extern int32_t sceAgcInit(uint32_t);
struct submit_description { void *words; uint32_t count; uint8_t flag,padding[3]; };
_Static_assert(sizeof(struct submit_description)==16,"native submission ABI");
extern int32_t sceAgcDriverSubmitDcb(struct submit_description *);
extern int32_t sceAgcSuspendPoint(void);
static int allocation(void *ctx,size_t bytes,uint32_t hi,int command,struct psbc_probe_allocation *a)
{
    (void)hi;(void)command;
    FILE *f=ctx; int64_t physical=-1; void *address=0;
    int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),bytes,65536,12,&physical);
    fprintf(f,"PSBC allocate bytes=%zu rc=%d physical=%" PRIx64 "\n",bytes,rc,(uint64_t)physical);fflush(f);
    if(rc)return rc;
    rc=sceKernelMapDirectMemory(&address,bytes,0x33,0,physical,65536);
    fprintf(f,"PSBC map rc=%d address=%p\n",rc,address);fflush(f);
    if(rc||!address){
        if(!rc)rc=-11;
        int released=sceKernelReleaseDirectMemory(physical,bytes);
        fprintf(f,"PSBC failed-map-release rc=%d\n",released);fflush(f);
        /* Report an unreleased physical allocation to the core for cleanup. */
        if(released){a->ptr=(void *)1;a->bytes=bytes;a->cookie[0]=(uintptr_t)physical;a->cookie[1]=0;}
        return rc;
    }
    a->ptr=address;a->bytes=bytes;a->cookie[0]=(uintptr_t)physical;a->cookie[1]=1;
    return 0;
}
static int release(void *ctx,struct psbc_probe_allocation *a)
{
    FILE *f=ctx;
    if(a->cookie[1]){
        int rc=sceKernelMunmap(a->ptr,a->bytes);
        fprintf(f,"PSBC unmap address=%p rc=%d\n",a->ptr,rc);fflush(f);
        if(rc)return rc;
        a->cookie[1]=0;
    }
    int rc=sceKernelReleaseDirectMemory((int64_t)a->cookie[0],a->bytes);
    fprintf(f,"PSBC release physical=%" PRIxPTR " rc=%d\n",a->cookie[0],rc);fflush(f);
    if(!rc)memset(a,0,sizeof(*a));
    return rc;
}
static int submit(void *ctx,const uint32_t *words,uint32_t count)
{
    struct submit_description d={(void *)words,count,0,{0,0,0}};
    fprintf((FILE *)ctx,"PSBC submit address=%p dwords=%u\n",(void *)words,count);fflush(ctx);
    int rc=sceAgcDriverSubmitDcb(&d);
    fprintf((FILE *)ctx,"PSBC submit-return rc=%d\n",rc);fflush(ctx);
    return rc;
}
static int suspend_point(void *ctx)
{
    int rc=sceAgcSuspendPoint();
    fprintf((FILE *)ctx,"PSBC suspend-point rc=%d\n",rc);fflush(ctx);return rc;
}
static uint64_t ns(void *ctx)
{ (void)ctx;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec; }
static void pause_us(void *ctx,unsigned us) { (void)ctx;usleep(us); }
int psbc_probe_run(FILE *f)
{
    if(!f)return -1;
    fprintf(f,"PSBC begin scope=existing-PSBC-LDS-control only model_inference=0\n");fflush(f);
    int init=sceAgcInit(8);
    fprintf(f,"PSBC agc-init version=8 rc=%d\n",init);fflush(f);
    if(init)return -10;
    struct psbc_probe_platform p={f,allocation,release,submit,suspend_point,ns,pause_us};
    struct psbc_probe_result r;
    int rc=psbc_probe_run_platform(&p,&r);
    fprintf(f,"PSBC result rc=%d stage=%d submit=%d suspend=%d completion=%u mismatches=%u input_errors=%u guard_errors=%u retained=%u cleanup=%u elapsed_ns=%" PRIu64 "\n",rc,r.stage,r.submit_rc,r.suspend_rc,r.completion_observed,r.mismatches,r.input_errors,r.guard_errors,r.resources_retained,r.cleanup_complete,r.elapsed_ns);
    fprintf(f,"PSBC addresses shader=%" PRIx64 " table=%" PRIx64 " data=%" PRIx64 " command=%" PRIx64 " fence=%" PRIx64 "\n",r.shader_address,r.table_address,r.data_address,r.command_address,r.fence_observed);
    fprintf(f,"PSBC values first=%08x expected=%08x last=%08x expected=%08x\n",r.output_first,r.expected_first,r.output_last,r.expected_last);fflush(f);
    /* The caller keeps this process alive on any retained result. Allocations
     * intentionally remain mapped after ambiguous submission/timeout. */
    return rc;
}
