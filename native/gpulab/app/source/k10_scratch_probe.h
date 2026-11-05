/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef K10_SCRATCH_PROBE_H
#define K10_SCRATCH_PROBE_H
#include "encoder_probe.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Isolated captured first k10 launch, full-ring scratch, never live chaining.
 * Fixture shader has a positive-address scratch-prefix literal correction;
 * model body, arguments, bindings and reference output are unchanged.
 * load/save names are relative to the chosen lane-a/k10-native-fixture root.
 * save callback is mandatory. Active FP16 comparison is result.b4.
 * Any result.retained requires the caller to park and preserve all remaining
 * GPU ownership. This function never releases allocations on submission,
 * suspend or completion ambiguity. Link model_scratch.c and encoder helpers. */
int k10_scratch_probe_run_ops(FILE *,const struct encoder_ops *,struct encoder_result *);
#ifdef __cplusplus
}
#endif
#endif
