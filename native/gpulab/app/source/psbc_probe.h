#ifndef PSBC_NATIVE_PROBE_H
#define PSBC_NATIVE_PROBE_H
#include <stdint.h>
#include <stddef.h>
 #include <stdio.h>
/* Map/release callbacks own platform-specific direct-memory handles. A failed
 * alloc must leave no allocation behind. command=1 identifies command storage.
 * address_hi=2 requests a table-capable address, other values mean any address.
 * free returns zero only after unmap and physical release both succeed. */
struct psbc_probe_allocation { void *ptr; size_t bytes; uintptr_t cookie[4]; };
struct psbc_probe_platform {
    void *context;
    int (*allocate)(void *,size_t,uint32_t,int,struct psbc_probe_allocation *);
    int (*release)(void *,struct psbc_probe_allocation *);
    int (*submit)(void *,const uint32_t *,uint32_t);
    int (*suspend_point)(void *);
    uint64_t (*monotonic_ns)(void *);
    void (*sleep_us)(void *,unsigned);
};
struct psbc_probe_result {
    int stage, rc, submit_rc, suspend_rc;
    unsigned mismatches, input_errors, guard_errors, resources_retained;
    unsigned completion_observed, cleanup_complete;
    uint32_t output_first, expected_first, output_last, expected_last;
    uint64_t shader_address, table_address, data_address, command_address;
    uint64_t fence_observed, elapsed_ns;
};
/* Exactly one workgroup, 16 KiB LDS, 256 lanes. This executes the existing PSBC
 * compiler output, not a SPIR-V runtime compilation. Zero only on output/guard
 * verification AND successful cleanup. Caller must keep its process alive when
 * resources_retained is set, until device-specific retirement is proven. */
int psbc_probe_run_platform(const struct psbc_probe_platform *,struct psbc_probe_result *);
int psbc_probe_run(FILE *);
size_t psbc_probe_commands(uint32_t *,size_t,uint64_t,uint64_t,uint64_t);
#endif
