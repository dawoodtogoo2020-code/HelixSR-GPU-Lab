#ifndef APP_SESSION_H
#define APP_SESSION_H
#include "profile_file_adapter.h"
#include "control_service.h"

/* Portable orchestration contract only. This is not a native UI or executor.
 * Single trusted controller, caller-owned zero storage, never copy/reset a live
 * instance. Every open attempt consumes the instance, including failure.
 * Admission is a trusted platform callback, never a UI-supplied boolean. It
 * must verify exact identity, provisioned root and recovery/rollback anchors.
 * No implementation of that authority or model qualification is provided here.
 */
typedef int (*as_admit_fn)(void *, const char *, const ap_identity *);
typedef enum {
    AS_OK=0, AS_INVALID, AS_DENIED, AS_IOFAULT, AS_CONFLICT, AS_BUSY,
    AS_TIME_REVERSED, AS_CLOSED
} as_result;
typedef enum { AS_NEW=0, AS_FAILED, AS_OPEN, AS_DONE } as_lifecycle;
typedef struct {
    ap_identity identity;
    ps_profile preference;
    cs_status runtime;
    int preference_valid, runtime_valid, storage_fault;
    as_lifecycle lifecycle;
    ps_result store_result;
    as_result last_result;
} as_view;
typedef struct {
    pfa_store store;
    cs_service control;
    as_view view;
    int store_open, control_started;
} as_session;
#define AS_SESSION_INITIALIZER {0}

as_result as_open(as_session *, const char *root, const ap_identity *,
                  as_admit_fn, void *admission_context, uint64_t now);
/* Save preference only. No runtime request; quality range 0..4. A conflict
 * retains the old visible preference without turning it into an IO fault. */
as_result as_save_preference(as_session *, ap_desired, unsigned quality,
                             uint64_t expected_revision);
/* Explicit runtime intents; ON uses only the existing control service gate.
 * OFF still reaches a valid service despite a profile/store failure. Neither
 * a return code nor desired OFF proves actual restoration. No UI ACK API. */
ap_refusal as_request_on(as_session *, uint64_t now);
ap_refusal as_disable(as_session *, uint64_t now);
ap_refusal as_tick(as_session *, uint64_t now);
/* Copy bounded cached display data; no IO, trust or capability promotion.
 * Return 0 for null arguments or AS_NEW, otherwise fill the view and return1. */
int as_query(const as_session *, as_view *);
/* Request OFF, refuse while attach/active/restore is outstanding, then close
 * the borrowed IO owner once. Keep the session alive on AS_BUSY. Persisted
 * recovery remains global across later separately admitted fresh sessions.
 * Terminal close is idempotent; the object can never be reopened. */
as_result as_close(as_session *, uint64_t now);
#endif
