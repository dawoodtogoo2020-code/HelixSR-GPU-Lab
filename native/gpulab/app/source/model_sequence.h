#ifndef MODEL_SEQUENCE_H
#define MODEL_SEQUENCE_H
#include "encoder_probe.h"
#include <stdbool.h>

/* Fixture ABI1. Names are relative to encoder_ops.load's fixture root.
 * sha256 is lowercase64hex for every file, including expected checkpoints.
 * Logical image bytes are tightly packed: width*height*DXGI bytes/pixel.
 * Runtime image rows are padded to256bytes; no expected file is uploaded.
 * All resources have an explicit initializer file, including initial zeros.
 * Resource contents persist across launches and frames. No implicit clears.
 */
#define MS_FIXTURE_ABI 1u
#define MS_ARGS_ROLE UINT32_MAX
/* The sole qualified scratch ABI, with the positive-address prefix correction.
 * Original compiler hash remains in model_scratch.h for provenance only. */
#define MS_SCRATCH_K10_CODE_SHA256 "7434b89368210e33b9772cd8e5d6ab3534cd55e0d604f55f70f6dab5ef3a0006"
#define MS_SCRATCH_K10_TAGS_CODE_SHA256 "9db02e6d0785f340579af5611d85ec6d188ea2077128c5e39434f46743afa2fb"
#define MS_SCRATCH_K10_TAGS_CODE_BYTES UINT32_C(825536)
#define MS_SCRATCH_K10_PACKED_CODE_BYTES UINT32_C(101232)
#define MS_SCRATCH_K10_PACKED_CODE_SHA256 "ce8d41af28a70cfebc2080584faf221fbdc08104eb98ecc014bdf48f7d7f08f5"
enum ms_resource_kind { MS_BUFFER=1,MS_IMAGE=2 };
enum ms_descriptor_kind { MS_DESC_BUFFER=1,MS_DESC_IMAGE=2,MS_DESC_SAMPLER=3 };
enum ms_resource_flags { MS_IMMUTABLE=1u };
enum ms_numeric_format { MS_EXACT=0,MS_F16=1,MS_F32=2,MS_E5M3=3 };
struct ms_file { const char *name;const char *sha256;uint64_t bytes; };
struct ms_resource {
 const char *name;struct ms_file initial;
 uint32_t kind,flags,width,height,dxgi_format;
 /* BUFFER: width/height/format0. IMAGE: DXGI10/34/41/54 only. */
};
/* Each logical role resolves to resource_even when parity0, resource_odd when
 * parity1. Set equal for a fixed role. Parity starts0 and flips ONLY after a
 * complete frame, including successful post-frame checkpoint comparisons.
 * Uploads/checkpoints/bindings all resolve using the same frame parity.
 */
struct ms_role { const char *name;uint32_t resource_even,resource_odd; };
struct ms_shader {
 const char *name;struct ms_file code;
 uint32_t rsrc1,rsrc2,block[3],user_slot;
 uint32_t scratch_bytes_per_wave;
 /* Referenced scratch requires an allowlisted corrected k10 variant and metadata:
  * 6144B/wave, RSRC1/2=402c001f/00140187, block128x1x1, user_slot2.
  * Other referenced shaders require both scratch bytes and SCRATCH_EN zero.
  * Unreferenced shader records are loaded/hashed but never dispatched. */
};
struct ms_binding {
 uint32_t kind,table_offset,role;
 uint64_t view_offset,view_bytes;
 uint32_t sampler_words[4];
 /* BUFFER16B, IMAGE32B, SAMPLER16B; offsets16B aligned/nonoverlapping.
  * MS_ARGS_ROLE allowed only for BUFFER, points to this launch's args.
  * BUFFER view uses logical byte extent; IMAGE view must cover full logical
  * image with offset0. Sampler uses only sampler_words, other fields zero.
  */
};
struct ms_checkpoint;
struct ms_launch {
 const char *name;uint32_t shader;struct ms_file args;
 uint32_t grid[3],table_bytes;
 const struct ms_binding *bindings;uint32_t binding_count;
 const struct ms_checkpoint *checkpoints;uint32_t checkpoint_count;
};
struct ms_upload {
 uint32_t role;uint64_t logical_offset;struct ms_file file;
 /* Only explicit external inputs. Full images only; buffers may be sliced. */
};
struct ms_checkpoint {
 const char *name;uint32_t role;uint64_t logical_offset;
 struct ms_file expected;
 uint32_t numeric_format;bool require_final_psnr;
 /* Comparison in logical/tightly-packed coordinates. MS_EXACT compares raw
  * bytes; numeric formats require all finite and relativeL2<1e-3. E5M3 is
  * unsigned byte decoded as exact FP16 bits(byte<<7). Optional final PSNR
  * requires>=45dB, peak=max(1,max(abs(reference))). Zero reference requires
  * zero numeric error. Byte differences remain diagnostic for tolerant pass.
  * Partial image checkpoints may span rows, excluding runtime row padding.
  */
};
struct ms_frame {
 const char *name;uint32_t first_launch,launch_count;
 const struct ms_upload *uploads;uint32_t upload_count;
 const struct ms_checkpoint *checkpoints;uint32_t checkpoint_count;
};
struct ms_fixture {
 uint32_t abi;const char *name;
 const struct ms_resource *resources;uint32_t resource_count;
 const struct ms_role *roles;uint32_t role_count;
 const struct ms_shader *shaders;uint32_t shader_count;
 const struct ms_launch *launches;uint32_t launch_count;
 const struct ms_frame *frames;uint32_t frame_count;
 uint64_t timeout_ns; /*0 selects10seconds per launch. Diagnostic only. */
};
struct ms_result {
 int rc,stage,submit_rc,suspend_rc;
 uint32_t frame,launch,completed_frames,completed_launches,parity;
 uint32_t retained,cleanup,guard_errors,immutable_errors,save_errors;
 uint32_t numerical_failures,final_psnr_checks;
 uint64_t byte_mismatches,first_mismatch,setup_ns,submit_to_fence_ns,total_ns;
 double max_relative_l2,min_final_psnr;
};
/* Nonzero retained requires caller to park/retain this process, including on
 * submit rejection where queue acceptance is ambiguous. No performance claim.
 * Scratch sequences request a third allocation:25296896B,64KiB-aligned,
 * low48-bit VA, with25159680B mutable payload at offset65536 and guards.
 * All GPU allocations remain owned on ambiguous completion; release failure
 * stops further cleanup. TMPRING is cleared after compute drain before fence.
 * No native wrapper or generated fixture is defined by this module.
 */
int model_sequence_run_ops(FILE *,const struct encoder_ops *,
                           const struct ms_fixture *,struct ms_result *);
/* Explicit diagnostic continuation, never a qualification pass on failures.
 * Continues finite numeric tolerance/PSNR failures with nonzero reference,
 * including remaining checkpoints in the same launch/frame; completed frames
 * advance parity. Any observed numerical failure still returns -12 at end.
 * Exact, zero-reference error, nonfinite, I/O, guard/immutable and GPU failures
 * stop immediately with the strict runner's cleanup/retention behavior.
 * Fixture ABI, thresholds, initializers and references are unchanged. */
int model_sequence_collect_run_ops(FILE *,const struct encoder_ops *,
                                   const struct ms_fixture *,struct ms_result *);
/* Separate diagnostic-collect variant with unique GPU start/end timestamp
 * slots per launch. Logs raw ticks only, including ordering/event overhead;
 * no exact timestamp period or resolution-performance claim. Adds one guarded
 * low48-bit mapping of align64KiB(launch_count*0x3000). Invalid/missing/late
 * timestamps retain all allocations. No added sleeps between launches.
 * All existing fixture/result structs, thresholds and modes are unchanged. */
int model_sequence_timed_collect_run_ops(FILE *,const struct encoder_ops *,
                                         const struct ms_fixture *,struct ms_result *);
/* Explicit standalone timing variant. Keeps each dispatch's command, table,
 * arguments, barrier, submit, suspend and fence checks. Flushes and audits
 * the entire arena, immutable code/resources and scratch guards once after
 * the final confirmed fence of each frame, before frame checkpoints and the
 * next frame's uploads. Rejects fixtures with launch checkpoints before any
 * GPU allocation; frame checkpoints (including final images) are unchanged.
 * A detected corruption can therefore be attributed only to its frame.
 * Retention on ambiguous submit/suspend/fence/timing states is unchanged. */
int model_sequence_frame_timed_collect_run_ops(FILE *,const struct encoder_ops *,
                                               const struct ms_fixture *,struct ms_result *);
/* Explicit diagnostic capture of all mutable physical resources before every
 * launch. HSRINP1 little-endian files contain a 64B header and 32B/resource
 * records followed by tightly packed logical bytes. Immutable weights and
 * launch arguments retain the existing hash checks. Capture I/O failure stops
 * before submission; no new GPU allocation or timing claim. */
int model_sequence_input_collect_run_ops(FILE *,const struct encoder_ops *,
                                         const struct ms_fixture *,struct ms_result *);
/* Same read-only HSRINP1 snapshots for an inclusive global launch range.
 * Strict original checks stop at the first failure. One CPU copy buffer is
 * reused; no extra GPU allocation, initializer, reset or shader modification. */
int model_sequence_input_range_run_ops(FILE *,const struct encoder_ops *,
                                      const struct ms_fixture *,struct ms_result *,
                                      uint32_t first,uint32_t last);
/* Exposed pure descriptor/packing helpers for fixture qualification tests. */
int model_sequence_descriptor(uint32_t out[8],const struct ms_resource *,
                              uint64_t gpu_base,uint64_t storage_bytes,
                              uint64_t row_pitch);
int model_sequence_pack_rows(void *,size_t,const void *,size_t,
                             uint32_t width,uint32_t height,uint32_t bpp);
int model_sequence_unpack_rows(void *,size_t,const void *,size_t,
                               uint32_t width,uint32_t height,uint32_t bpp);
#endif
