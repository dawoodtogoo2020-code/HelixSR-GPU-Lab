/* SPDX-License-Identifier: GPL-3.0-or-later
 * Same raw ABI and type12/prot0x33 allocation path as the hardware-passed LDS
 * control. This wrapper runs only from an explicit encoder-probe startup flag. */
#include "encoder_probe.h"
#include "encoder_next.h"
#include "model_fixture.h"
#include "enc2_saturation_fixture.h"
#include "model_large_fixture.h"
#include "k10_scratch_probe.h"
#include "scratch_canary.h"
#include "gpu_timestamp_probe.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/statvfs.h>
#define INPUT_ROOT "/data/helixsr-speed/encoder"
extern int32_t sceKernelAllocateDirectMemory(int64_t,int64_t,size_t,size_t,int,int64_t *);
extern int64_t sceKernelGetDirectMemorySize(void);
extern int32_t sceKernelReleaseDirectMemory(int64_t,size_t);
extern int32_t sceKernelMapDirectMemory(void **,size_t,int,int,int64_t,size_t);
extern int32_t sceKernelMunmap(void *,size_t);
extern int32_t sceAgcInit(uint32_t);
struct encoder_submit {void *words;uint32_t count;uint8_t flag,padding[3];};
_Static_assert(sizeof(struct encoder_submit)==16,"AGC submission ABI");
extern int32_t sceAgcDriverSubmitDcb(struct encoder_submit *);
extern int32_t sceAgcSuspendPoint(void);
static int allocate(void *ctx,size_t bytes,struct encoder_allocation *a)
{
    FILE *f=ctx;int64_t physical=-1;void *address=0;
    int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),bytes,65536,12,&physical);
    fprintf(f,"ENCODER allocate bytes=%zu physical=%" PRIx64 " rc=%d\n",bytes,(uint64_t)physical,rc);fflush(f);
    if(rc)return rc;
    rc=sceKernelMapDirectMemory(&address,bytes,0x33,0,physical,65536);
    fprintf(f,"ENCODER map address=%p rc=%d\n",address,rc);fflush(f);
    if(rc||!address){
        if(!rc)rc=-30;
        int released=sceKernelReleaseDirectMemory(physical,bytes);
        fprintf(f,"ENCODER failed-map-release rc=%d\n",released);fflush(f);
        if(released){a->ptr=(void *)1;a->bytes=bytes;a->cookie[0]=(uintptr_t)physical;a->cookie[1]=0;}
        return rc;
    }
    a->ptr=address;a->bytes=bytes;a->cookie[0]=(uintptr_t)physical;a->cookie[1]=1;return 0;
}
static int release(void *ctx,struct encoder_allocation *a)
{
    FILE *f=ctx;
    if(a->cookie[1]){
        int rc=sceKernelMunmap(a->ptr,a->bytes);fprintf(f,"ENCODER unmap address=%p rc=%d\n",a->ptr,rc);fflush(f);
        if(rc)return rc;
        a->cookie[1]=0;
    }
    int rc=sceKernelReleaseDirectMemory((int64_t)a->cookie[0],a->bytes);
    fprintf(f,"ENCODER release physical=%" PRIxPTR " rc=%d\n",a->cookie[0],rc);fflush(f);
    if(!rc)memset(a,0,sizeof(*a));return rc;
}
static int load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact(INPUT_ROOT,name,data,n);if(rc)fprintf((FILE*)ctx,"ENCODER load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact(INPUT_ROOT,name,data,n);}
static int submit(void *ctx,const uint32_t *words,uint32_t count)
{
    struct encoder_submit d={(void *)words,count,0,{0,0,0}};
    fprintf((FILE *)ctx,"ENCODER submit address=%p dwords=%u\n",(void *)words,count);fflush(ctx);
    int rc=sceAgcDriverSubmitDcb(&d);fprintf((FILE *)ctx,"ENCODER submit-return rc=%d\n",rc);fflush(ctx);return rc;
}
static int suspend_point(void *ctx)
{int rc=sceAgcSuspendPoint();fprintf((FILE *)ctx,"ENCODER suspend-point rc=%d\n",rc);fflush(ctx);return rc;}
static uint64_t ns(void *ctx)
{(void)ctx;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec;}
static void pause_us(void *ctx,unsigned us){(void)ctx;usleep(us);}
int encoder_probe_run(FILE *f)
{
    if(!f)return -1;
    fprintf(f,"ENCODER begin scope=first-encoder repaired-CUDA-reference full_model=0\n");fflush(f);
    int rc=sceAgcInit(8);fprintf(f,"ENCODER agc-init version=8 rc=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,load,submit,suspend_point,ns,pause_us,save};
    struct encoder_result result;
    return encoder_probe_run_ops(f,&p,&result);
    /* Caller parks this process on nonzero return, especially retained=1. */
}
/* These explicit test launches use immutable, separate fixture directories. */
static const char *next_root;
static int next_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact(next_root,name,data,n);if(rc)fprintf((FILE*)ctx,"ENCODER_NEXT load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int next_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact(next_root,name,data,n);}
int encoder_next_probe_run(FILE *f,int stage)
{
    if(!f||(stage!=1&&stage!=2))return -1;
    next_root=stage==1?"/data/helixsr-speed/enc1":"/data/helixsr-speed/enc2";
    fprintf(f,"ENCODER_NEXT begin stage=%d repaired_CUDA_reference=1 live_chain=0 full_model=0\n",stage);fflush(f);
    int rc=sceAgcInit(8);fprintf(f,"ENCODER_NEXT agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,next_load,submit,suspend_point,ns,pause_us,next_save};
    struct encoder_result result;
    return encoder_next_run_ops(f,&p,stage,&result);
}

static int model_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact("/data/helixsr-speed/model-fixture",name,data,n);if(rc)fprintf((FILE*)ctx,"MODEL load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
int gpu_timestamp_native_run(FILE *f)
{
    if(!f)return -1;
    int rc=sceAgcInit(8);fprintf(f,"GPU_TIMESTAMP agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,NULL,submit,suspend_point,ns,pause_us,NULL};
    struct gpu_timestamp_result result;
    return gpu_timestamp_probe_run_ops(f,&p,&result);
}
static int model_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact("/data/helixsr-speed/model-fixture",name,data,n);}
int model_sequence_probe_run(FILE *f,int mode)
{
    if(!f)return -1;
    if(mode<0||mode>4)return -32;
    if(mode==4){
        struct statvfs space;
        int checked=statvfs("/data/helixsr-speed/model-fixture",&space);
        uint64_t unit=checked?0:(space.f_frsize?space.f_frsize:space.f_bsize);
        uint64_t available=!unit||space.f_bavail>UINT64_MAX/unit?0:space.f_bavail*unit;
        fprintf(f,"MODEL input_capture_space rc=%d available_bytes=%"PRIu64" required_bytes=209715200\n",checked,available);fflush(f);
        if(checked||available<UINT64_C(209715200))return -33;
    }
    int prefix=mode==1;
    struct ms_fixture fixture=helix_model_fixture;
    struct ms_frame frame=fixture.frames[0];
    if(prefix){
        unsigned count=0;
        while(count<frame.launch_count && !fixture.shaders[fixture.launches[count].shader].scratch_bytes_per_wave)++count;
        if(!count)return -32;
        frame.launch_count=count;frame.checkpoints=NULL;frame.checkpoint_count=0;
        fixture.name="compact32-prefix-before-scratch";
        fixture.frames=&frame;fixture.frame_count=1;fixture.launch_count=count;
    }
    fprintf(f,"MODEL begin prefix=%d launches=%u frames=%u repaired_CUDA_reference=1 synthetic_fixture=1 real_game=0\n",prefix,fixture.launch_count,fixture.frame_count);fflush(f);
    int rc=sceAgcInit(8);fprintf(f,"MODEL agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,model_load,submit,suspend_point,ns,pause_us,model_save};
    struct ms_result result;
    if(mode==4)return model_sequence_input_collect_run_ops(f,&p,&fixture,&result);
    if(mode==3)return model_sequence_timed_collect_run_ops(f,&p,&fixture,&result);
    return mode==2?model_sequence_collect_run_ops(f,&p,&fixture,&result):model_sequence_run_ops(f,&p,&fixture,&result);
}

static int large_model_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact("/data/helixsr-speed/model-large-fixture",name,data,n);if(rc)fprintf((FILE*)ctx,"MODEL_LARGE load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int large_model_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact("/data/helixsr-speed/model-large-fixture",name,data,n);}
int model_large_native_run(FILE *f,int mode)
{
    if(!f||mode<1||mode>5)return -1;
    if(mode==4){
        struct statvfs space;
        int checked=statvfs("/data/helixsr-speed/model-large-fixture",&space);
        uint64_t unit=checked?0:(space.f_frsize?space.f_frsize:space.f_bsize);
        uint64_t available=!unit||space.f_bavail>UINT64_MAX/unit?0:space.f_bavail*unit;
        fprintf(f,"MODEL_LARGE input_capture_space rc=%d available_bytes=%"PRIu64" required_bytes=268435456\n",checked,available);fflush(f);
        if(checked||available<UINT64_C(268435456))return -33;
    }
    const struct ms_fixture *fixture=&large_helix_model_fixture;
    fprintf(f,"MODEL_LARGE begin fixture=%s mode=%d synthetic=1 real_game=0 repaired_CUDA_reference=1\n",fixture->name,mode);fflush(f);
    int rc=sceAgcInit(8);fprintf(f,"MODEL_LARGE agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,large_model_load,submit,suspend_point,ns,pause_us,large_model_save};
    struct ms_result result;
    if(mode==5)return model_sequence_frame_timed_collect_run_ops(f,&p,fixture,&result);
    if(mode==4)return model_sequence_input_range_run_ops(f,&p,fixture,&result,19,20);
    if(mode==3)return model_sequence_timed_collect_run_ops(f,&p,fixture,&result);
    return mode==2?model_sequence_collect_run_ops(f,&p,fixture,&result):model_sequence_run_ops(f,&p,fixture,&result);
}

/* Independent-copy throughput diagnostic. The production model fixture and
 * settings are unchanged. Each explicit startup flag runs one admitted case. */
static char saturation_output_root[128];
static int saturation_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact("/data/helixsr-speed/enc2-saturation",name,data,n);if(rc)fprintf((FILE*)ctx,"ENC2_SATURATION load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int saturation_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact(saturation_output_root,name,data,n);}
int enc2_saturation_native_run(FILE *f,int copies)
{
    if(!f)return -1;
    const struct ms_fixture *fixture=NULL;const char *label=NULL;
    switch(copies){
    case 0:fixture=&sat_baseline_fixture;label="baseline";break;
    case 1:fixture=&sat_n1_fixture;label="n1";break;
    case 8:fixture=&sat_n8_fixture;label="n8";break;
    case 32:fixture=&sat_n32_fixture;label="n32";break;
    default:return -32;
    }
    snprintf(saturation_output_root,sizeof saturation_output_root,"/data/helixsr-speed/enc2-saturation/%s",label);
    fprintf(f,"ENC2_SATURATION begin case=%s copies=%d synthetic_independent_copies=1 real_resolution=0 real_game=0\n",label,copies?copies:1);fflush(f);
    int rc=sceAgcInit(8);fprintf(f,"ENC2_SATURATION agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,saturation_load,submit,suspend_point,ns,pause_us,saturation_save};
    struct ms_result result;
    return model_sequence_timed_collect_run_ops(f,&p,fixture,&result);
}

static int k10_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact("/data/helixsr-speed/k10",name,data,n);if(rc)fprintf((FILE*)ctx,"K10 load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int k10_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact("/data/helixsr-speed/k10",name,data,n);}
int k10_scratch_native_run(FILE *f)
{
    if(!f)return -1;
    int rc=sceAgcInit(8);fprintf(f,"K10 agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,k10_load,submit,suspend_point,ns,pause_us,k10_save};
    struct encoder_result result;
    return k10_scratch_probe_run_ops(f,&p,&result);
}

static int canary_load(void *ctx,const char *name,void *data,size_t n)
{errno=0;int rc=encoder_load_exact("/data/helixsr-speed/scratch-canary",name,data,n);if(rc)fprintf((FILE*)ctx,"CANARY load_failure file=%s code=%d errno=%d\n",name,rc,errno);return rc;}
static int canary_save(void *ctx,const char *name,const void *data,size_t n)
{(void)ctx;return encoder_save_exact("/data/helixsr-speed/scratch-canary",name,data,n);}
int scratch_canary_native_run(FILE *f,int stage)
{
    if(!f)return -1;
    int rc=sceAgcInit(8);fprintf(f,"CANARY agc_init=%d\n",rc);fflush(f);
    if(rc)return -31;
    struct encoder_ops p={f,allocate,release,canary_load,submit,suspend_point,ns,pause_us,canary_save};
    struct encoder_result result;
    return scratch_canary_run_ops(f,&p,stage,&result);
}
