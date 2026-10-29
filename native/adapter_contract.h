/* SPDX-License-Identifier: MIT */
#ifndef HELIX_ADAPTER_CONTRACT_H
#define HELIX_ADAPTER_CONTRACT_H
#include <stdint.h>
#define HC_KEY_SIZE 64u
#define HC_HASH_SIZE 65u
typedef struct { char device[HC_KEY_SIZE], title[HC_KEY_SIZE], build[HC_KEY_SIZE], executable_hash[HC_HASH_SIZE]; } hc_identity;

/* This gate validates caller-supplied metadata only; it neither attaches nor
 * submits GPU work. A refusal means use the original path only before submit
 * and after the caller has safely handled all in-flight resource lifetimes. */
typedef enum { HC_FMT_UNKNOWN=0, HC_FMT_RGBA16F, HC_FMT_RGBA32F, HC_FMT_RG16F,
    HC_FMT_RG32F, HC_FMT_R32F, HC_FMT_D32F, HC_FMT_R16F, HC_FMT_R8_UNORM } hc_format;
typedef enum { HC_MOTION_UNKNOWN=0, HC_MOTION_CURRENT_TO_PREVIOUS, HC_MOTION_PREVIOUS_TO_CURRENT } hc_motion_direction;
typedef enum { HC_SCALE_UNKNOWN=0, HC_SCALE_PIXELS, HC_SCALE_NORMALIZED } hc_motion_scale;
typedef enum { HC_MRES_UNKNOWN=0, HC_MRES_INPUT, HC_MRES_OUTPUT } hc_motion_resolution;
typedef enum { HC_JITTER_UNKNOWN=0, HC_JITTER_PIXELS, HC_JITTER_NORMALIZED } hc_jitter_units;
typedef enum { HC_DEPTH_UNKNOWN=0, HC_DEPTH_NORMAL, HC_DEPTH_INVERTED } hc_depth;
typedef enum { HC_DEPTH_ENCODING_UNKNOWN=0, HC_DEPTH_DEVICE_0_1, HC_DEPTH_LINEAR_VIEW } hc_depth_encoding;
typedef enum { HC_COLOR_UNKNOWN=0, HC_COLOR_LINEAR, HC_COLOR_PRE_EXPOSED } hc_color;
typedef enum { HC_EXPOSURE_UNKNOWN=0, HC_EXPOSURE_SCALAR, HC_EXPOSURE_TEXTURE } hc_exposure;
/* Reactive and composition masks use normalized 0..1 semantics; pixel values
 * remain caller-verified. Both masks are optional unless required by descriptor. */
enum { HC_COLOR_IN=0, HC_DEPTH_IN, HC_MOTION_IN, HC_EXPOSURE_IN, HC_OUTPUT, HC_REACTIVE_IN, HC_COMPOSITION_IN, HC_RESOURCE_COUNT };
enum { HC_MATH_FP16_ARITHMETIC=1u<<0, HC_MATH_INT_DOT4=1u<<1, HC_MATH_FP32_ARITHMETIC=1u<<2 };
#define HC_MATH_KNOWN (HC_MATH_FP16_ARITHMETIC|HC_MATH_INT_DOT4|HC_MATH_FP32_ARITHMETIC)
#define HC_FORMAT_BIT(i) (1u << (i))
#define HC_BIT(i) (1u << (i))
#define HC_MANDATORY (HC_BIT(HC_COLOR_IN)|HC_BIT(HC_DEPTH_IN)|HC_BIT(HC_MOTION_IN)|HC_BIT(HC_OUTPUT))
/* bytes=stride=0 means opaque GPU image; caller must prove allocation bounds.
 * Otherwise both must be supplied and describe a linear row layout. */
typedef struct { uintptr_t handle; uint32_t width,height; hc_format format; uint64_t bytes; uint32_t stride; } hc_resource;
typedef struct {
    hc_identity identity; char backend_hash[HC_HASH_SIZE], model_hash[HC_HASH_SIZE];
    int adapter_qualified, backend_qualified;
    uint32_t required_wave_size, required_math_mask;
    uint32_t min_input_width,min_input_height,max_input_width,max_input_height;
    uint32_t min_output_width,min_output_height,max_output_width,max_output_height;
    hc_motion_direction direction; hc_motion_scale scale; hc_motion_resolution motion_resolution;
    hc_jitter_units jitter_units; hc_depth depth; hc_depth_encoding depth_encoding; hc_color color; hc_exposure exposure;
    int motion_vectors_include_jitter; /* exactly 0 or 1 */
    uint32_t required_mask, allowed_formats[HC_RESOURCE_COUNT];
} hc_descriptor;
typedef struct {
    hc_identity identity; char backend_hash[HC_HASH_SIZE],model_hash[HC_HASH_SIZE];
    /* Game-frame IDs need not be consecutive. previous_frame_id identifies
     * the frame referenced by this frame's temporal inputs. */
    uint64_t frame_id, previous_frame_id, history_id; uint32_t input_width,input_height,output_width,output_height;
    float jitter_x,jitter_y,frame_time_ms,exposure_value;
    /* Caller must attest output allocation does not overlap any input. Distinct
     * opaque handles alone cannot prove non-aliasing. */
    int barriers_confirmed,lifetimes_confirmed,output_non_aliasing_confirmed; uint32_t present_mask;
    int capabilities_confirmed; uint32_t observed_wave_size, available_math_mask;
    hc_resource resources[HC_RESOURCE_COUNT];
} hc_frame;
typedef struct {
    int has_previous,work_in_flight; uint64_t frame_id,history_id;
    uint32_t observed_wave_size, available_math_mask, required_wave_size, required_math_mask;
    uint32_t input_width,input_height,output_width,output_height;
    hc_identity identity; char backend_hash[HC_HASH_SIZE],model_hash[HC_HASH_SIZE];
    hc_motion_direction direction; hc_motion_scale scale; hc_motion_resolution motion_resolution;
    hc_jitter_units jitter_units; hc_depth depth; hc_depth_encoding depth_encoding; hc_color color; hc_exposure exposure;
    int motion_vectors_include_jitter;
    uint32_t required_mask, present_mask;
    hc_resource resources[HC_RESOURCE_COUNT]; /* layout metadata only; handles ignored */
} hc_previous;
typedef enum { HC_ACCEPT=0, HC_INVALID, HC_UNQUALIFIED, HC_IDENTITY_MISMATCH,
    HC_UNSUPPORTED, HC_MISSING_RESOURCE, HC_ALIAS, HC_STALE, HC_UNSAFE, HC_DRAIN_REQUIRED } hc_result;
typedef struct { hc_result result; int needs_history_reset; } hc_decision;
hc_decision hc_check(const hc_descriptor *,const hc_frame *,const hc_previous *);
#endif
