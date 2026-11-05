#ifndef ENCODER_PROBE_H
#define ENCODER_PROBE_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
struct encoder_allocation { void *ptr;size_t bytes;uintptr_t cookie[4]; };
struct encoder_ops {
    void *ctx;
    int (*allocate)(void *,size_t,struct encoder_allocation *);
    int (*release)(void *,struct encoder_allocation *);
    int (*load)(void *,const char *,void *,size_t);
    int (*submit)(void *,const uint32_t *,uint32_t);
    int (*suspend_point)(void *);
    uint64_t (*ns)(void *);
    void (*sleep_us)(void *,unsigned);
    int (*save)(void *,const char *,const void *,size_t);
};
struct encoder_comparison {size_t byte_mismatches,nonfinite;double relative_l2,max_abs;};
struct encoder_result {
    int rc,stage,submit_rc,suspend_rc;
    unsigned completion,retained,cleanup,guard_errors,immutable_errors,overflow_errors;
    unsigned debug_save_errors;
    uint32_t diagnostic0,diagnostic1;
    uint64_t setup_ns,submit_to_fence_ns,total_ns,fence;
    struct encoder_comparison b3,b4;
};
int encoder_probe_run(FILE *);
int encoder_probe_run_ops(FILE *,const struct encoder_ops *,struct encoder_result *);
int encoder_load_exact(const char *,const char *,void *,size_t);
int encoder_save_exact(const char *,const char *,const void *,size_t);
size_t encoder_commands(uint32_t *,size_t,uint64_t,uint64_t,uint64_t);
#endif
