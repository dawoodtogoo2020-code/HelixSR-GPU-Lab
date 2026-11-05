#ifndef LOGICAL_INPUT_H
#define LOGICAL_INPUT_H
#include "app_frontend.h"

typedef enum {
    LI_SELECT=0, LI_SAVE, LI_ENABLE, LI_DISABLE, LI_REFRESH, LI_CLOSE
} li_kind;
typedef struct {
    li_kind kind;
    union {
        ap_identity identity;
        struct { ap_desired desired; unsigned quality; uint64_t revision; } save;
    } value;
} li_intent;
typedef enum { LI_RELEASE=0, LI_PRESS=1 } li_phase;
typedef struct {
    li_phase phase;
    unsigned count;
    const li_intent *intents;
} li_sample;
typedef enum { LI_INVALID=0, LI_EMITTED, LI_SUPPRESSED, LI_RELEASED } li_result;
typedef struct { unsigned held; } li_gate;
#define LI_GATE_INITIALIZER {0}

/* Pure conversion of a complete logical intent. No dispatch or authority.
 * SELECT uses the accepted bounded identity validator. SAVE accepts only
 * AP_OFF/AP_ON and quality 0..4; revision is an opaque uint64_t value.
 * AP_ON in SAVE is a preference, never an implicit ENABLE event.
 * All required pointers reference complete readable/writable objects, with
 * input/output/state storage disjoint. No pointers are retained. Output is
 * unchanged on rejection; accepted payloads are copied by value. */
li_result li_translate(const li_intent *, af_event *);

/* Single serialized caller owns zero-initialized state. Do not edit held.
 * PRESS requires exactly one complete intent. Zero or multiple intents reject
 * before dereferencing intents. RELEASE is canonical: count=0, intents=NULL.
 * Only a valid explicit RELEASE clears held, and never emits an event.
 * Every valid press is validated even while held. First press emits once;
 * subsequent valid presses, including a changed kind/payload, are suppressed
 * until release. Malformed/simultaneous input leaves state and output intact.
 * Bad input never implies DISABLE; DISABLE is an explicit single action and
 * obeys the same edge gate. Host safety actions need a separate trusted path.
 * No physical mappings, timing, repeat rate, callbacks or OS/device behavior
 * are specified. Caller decides whether/when to dispatch an emitted event. */
li_result li_step(li_gate *, const li_sample *, af_event *);
#endif
