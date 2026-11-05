#ifndef HELIX_GPU_TIMESTAMP_PROBE_H
#define HELIX_GPU_TIMESTAMP_PROBE_H
#include "encoder_probe.h"
#define GPU_TIMESTAMP_SAMPLES 3u
struct gpu_timestamp_sample {uint64_t ticks,cpu_submit_ns,cpu_complete_ns;};
struct gpu_timestamp_result {
 int rc,submit_rc,suspend_rc;
 unsigned samples,retained,cleanup,guard_errors,immutable_errors,late_writes;
 struct gpu_timestamp_sample sample[GPU_TIMESTAMP_SAMPLES];
 double apparent_hz[GPU_TIMESTAMP_SAMPLES-1];
};
/* Exact source timestamp packet. Returns8 or0 for invalid destination/capacity.
 * This packet alone provides no drain/completion or ZPASS prerequisite.
 * Caller owns destination until confirmed GPU completion, cache eviction and
 * validation. No assumed timestamp period, no high-bit masking. */
size_t gpu_timestamp_packet(uint32_t*,size_t,uint64_t);
/* Probe chain includes source ZPASS_DONE prerequisite, then timestamp,
 * RELEASE_MEM cache drain + WAIT_REG_MEM64 + separate final completion marker.
 * Returns39 DWORDs, or0. Destinations must be separate8B-aligned owned ranges;
 * ZPASS needs at least0x200B reserved (probe spaces each scratch by4KiB). */
size_t gpu_timestamp_probe_commands(uint32_t*,size_t,uint64_t timestamp,
 uint64_t zpass,uint64_t barrier,uint64_t fence,uint32_t token);
/* Three CPU-separated samples, then cache-evicted stability checks. Uses only
 * allocate/release/submit/suspend/ns/sleep callbacks, no files or shaders.
 * Nonzero retained requires caller to park this process and keep allocations.
 * API success means counter/ordering/plausibility checks passed, not an exact
 * ns-per-tick qualification or measured model execution time. */
int gpu_timestamp_probe_run_ops(FILE*,const struct encoder_ops*,struct gpu_timestamp_result*);
#endif
