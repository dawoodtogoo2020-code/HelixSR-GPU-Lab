/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MODEL_SCRATCH_H
#define MODEL_SCRATCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed ABI of native-full-compact/k10, not an arbitrary spilled shader. */
#define MODEL_SCRATCH_K10_CODE_SHA256 "8571cad87d597836ff75fdfc71270ec79b020152d61b9e860adb8a58e1238252"
#define MODEL_SCRATCH_K10_CODE_BYTES UINT32_C(871744)
struct model_scratch_topology {
    uint32_t max_good_cu_per_sa, max_sa_per_se, num_se;
};
struct model_scratch_plan {
    uint64_t allocation_bytes;
    uint32_t waves, bytes_per_wave, wavesize_units, base_alignment;
    uint32_t tmpring_register, tmpring_value, tmpring_words[3];
    uint32_t rsrc1_register, rsrc1_value, rsrc2_register, rsrc2_value;
    uint32_t user_data_first, user_data_count, user_data[2];
    uint32_t descriptor_user_slot, wave_offset_sgpr, wave_size, bound;
};
/* Facts must come from the target GPU, never guessed from its product name.
 * Return 0 or negative; a nonnull output is cleared on failure. No I/O/GPU work.
 * Requirements computes Mesa's full topology bound without a GPU address.
 * Build also checks a page-aligned low-48-bit allocation and binds its DIRECT
 * address with SWIZZLE_ENABLE bit31 in user s1. Capacity excludes caller-owned
 * guards. 4096 alignment
 * is this helper's conservative allocation policy, not a claimed ISA minimum.
 * Caller verifies the code hash above, preserves descriptor pointer in user2,
 * programs TMPRING before dispatch, and owns visibility/completion/retention.
 */
int model_scratch_requirements(const struct model_scratch_topology *, struct model_scratch_plan *);
int model_scratch_build(const struct model_scratch_topology *, uint64_t gpu_base,
                        uint64_t capacity, struct model_scratch_plan *);
/* Explicit maximum representable GFX10 ring: 4095 records, 25,159,680 bytes.
 * Uses no invented target topology. This is a caller-selected conservative
 * ring capacity, not a topology query or hardware qualification. */
int model_scratch_full_ring_requirements(struct model_scratch_plan *);
int model_scratch_full_ring_build(uint64_t gpu_base, uint64_t capacity,
                                  struct model_scratch_plan *);
#ifdef __cplusplus
}
#endif
#endif
