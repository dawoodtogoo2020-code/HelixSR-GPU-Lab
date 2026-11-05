/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "model_scratch.h"
#include <string.h>
static int requirements(uint32_t waves, struct model_scratch_plan *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (waves < 32 || waves > 4095) return -1;
    struct model_scratch_plan p = {0};
    p.waves = waves;
    p.bytes_per_wave = 6144;
    p.wavesize_units = p.bytes_per_wave >> 10;
    p.allocation_bytes = (uint64_t)waves * p.bytes_per_wave;
    p.base_alignment = 4096;
    p.tmpring_register = 0x00b860;
    p.tmpring_value = p.waves | (p.wavesize_units << 12);
    /* Same SET_SH_REG packet convention as the proven encoder_commands. */
    p.tmpring_words[0] = 0xc0017600;
    p.tmpring_words[1] = (p.tmpring_register - 0x00b000) / 4;
    p.tmpring_words[2] = p.tmpring_value;
    p.rsrc1_register = 0x00b848; p.rsrc1_value = 0x402c001f;
    p.rsrc2_register = 0x00b84c; p.rsrc2_value = 0x00140187;
    p.user_data_first = 0; p.user_data_count = 2;
    p.descriptor_user_slot = 2; p.wave_offset_sgpr = 5; p.wave_size = 32;
    *out = p;
    return 0;
}
int model_scratch_requirements(const struct model_scratch_topology *t,
                               struct model_scratch_plan *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!t || !t->max_good_cu_per_sa || !t->max_sa_per_se || !t->num_se) return -1;
    /* Any individual positive factor >=128 exceeds the 12-bit WAVES field
     * after Mesa's factor32. This check also bounds the multiplication. */
    if (t->max_good_cu_per_sa > 127 || t->max_sa_per_se > 127 || t->num_se > 127) return -1;
    uint64_t waves = UINT64_C(32) * t->max_good_cu_per_sa * t->max_sa_per_se * t->num_se;
    if (waves > 4095) return -1;
    return requirements((uint32_t)waves, out);
}
static int bind_plan(struct model_scratch_plan p, uint64_t base, uint64_t capacity,
                     struct model_scratch_plan *out)
{
    const uint64_t limit = UINT64_C(1) << 48;
    if (!base || (base & (p.base_alignment - 1)) || base >= limit ||
        p.allocation_bytes > limit - base) return -2;
    if (capacity < p.allocation_bytes) return -3;
    p.user_data[0] = (uint32_t)base;
    /* RADV radv_emit_compute_scratch encodes SWIZZLE_ENABLE_GFX6 in bit31.
     * The shader prologue adds 0x7fff0000 to this encoded high word to
     * construct FLAT_SCR_HI; a plain address high word is invalid here. */
    p.user_data[1] = (uint32_t)(base >> 32) | UINT32_C(0x80000000);
    p.bound = 1;
    *out = p;
    return 0;
}
int model_scratch_build(const struct model_scratch_topology *t, uint64_t base,
                        uint64_t capacity, struct model_scratch_plan *out)
{
    struct model_scratch_plan p;
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (model_scratch_requirements(t, &p)) return -1;
    return bind_plan(p, base, capacity, out);
}
int model_scratch_full_ring_requirements(struct model_scratch_plan *out)
{ return requirements(4095, out); }
int model_scratch_full_ring_build(uint64_t base, uint64_t capacity,
                                  struct model_scratch_plan *out)
{
    struct model_scratch_plan p;
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (model_scratch_full_ring_requirements(&p)) return -1;
    return bind_plan(p, base, capacity, out);
}
