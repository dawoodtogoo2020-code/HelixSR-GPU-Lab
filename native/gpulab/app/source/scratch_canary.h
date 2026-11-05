/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SCRATCH_CANARY_H
#define SCRATCH_CANARY_H
#include "encoder_probe.h"
#ifdef __cplusplus
extern "C" {
#endif
/* stage1: incoming scratch args/wave offset/flat-register readback, no scratch
 * memory access. stage2: same readback plus integer scratch store/load.
 * stage3: stage2 with actual positive48-bit flat base instead of forcedffff
 * high16bits; separate53433003 record tag and stage3 load/save filenames.
 * load/save adapter root is lane-a/scratch-canary. Save is mandatory.
 * Result.b3 counts diagnostic output byte mismatches; overflow_errors counts
 * invalid/nonuniform wave offsets. Slot reuse across waves is allowed because
 * no barrier keeps them concurrently live. Output values are uint32, not FP16;
 * relative_l2 is unused. Captured stride alignment is not an admission rule.
 * Caller must park and preserve remaining GPU ownership if result.retained.
 */
int scratch_canary_run_ops(FILE *,const struct encoder_ops *,int stage,struct encoder_result *);
#ifdef __cplusplus
}
#endif
#endif
