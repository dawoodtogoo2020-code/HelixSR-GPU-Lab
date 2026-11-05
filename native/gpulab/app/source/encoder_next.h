#ifndef ENCODER_NEXT_H
#define ENCODER_NEXT_H
#include "encoder_probe.h"
/* stage1 selects enc_1; stage2 selects enc_2. Each invocation loads its own
 * immutable captured input. Sequential calls do not chain live GPU tensors.
 * result.stage retains encoder_probe's lifecycle phase. result.b3 describes
 * B6 (stage1) or B7 (stage2); result.b4 describes B5 or B4's active region.
 * Nonzero retained requires the caller to keep this process alive for recovery.
 * load/save callbacks select the stage fixture root; ../weights.bin is shared. */
int encoder_next_run_ops(FILE *,const struct encoder_ops *,int stage,struct encoder_result *);
#endif
