#ifndef ACTIVATION_POLICY_H
#define ACTIVATION_POLICY_H

#include <stdint.h>

#define AP_KEY_SIZE 64u
#define AP_HASH_SIZE 65u
#define AP_MAX_EVIDENCE_AGE_MS UINT64_C(30000)

typedef struct {
    char device[AP_KEY_SIZE];
    char title[AP_KEY_SIZE];
    char build[AP_KEY_SIZE];
    char executable_hash[AP_HASH_SIZE];
} ap_identity;

typedef struct {
    ap_identity identity;
    /* Strict booleans: each must be exactly 0 or 1. */
    int backend, qualified_model, interoperability;
    int exact_build_hook, temporal_contract, recovery_path;
    uint64_t observed_ms;
    uint64_t lifetime_ms;
} ap_evidence;

typedef enum { AP_OFF = 0, AP_ON = 1 } ap_desired;
typedef enum {
    AP_INACTIVE = 0, AP_PENDING_ATTACH, AP_ACTIVE,
    AP_RESTORE_REQUIRED, AP_FAULT
} ap_observed;
typedef enum { AP_ACK_FAILED = 0, AP_ACK_SUCCESS = 1, AP_ACK_UNCERTAIN = 2 } ap_ack;
typedef enum { AP_ACTION_NONE = 0, AP_ACTION_ATTACH, AP_ACTION_RESTORE_ORIGINAL } ap_action;
typedef enum {
    AP_OK = 0, AP_INVALID_ARGUMENT, AP_NOT_INITIALIZED, AP_ALREADY_LIVE,
    AP_INVALID_IDENTITY, AP_IDENTITY_MISMATCH, AP_INVALID_BOOLEAN,
    AP_MISSING_EVIDENCE, AP_CAPABILITY_MISSING, AP_TIME_REVERSED,
    AP_EVIDENCE_FUTURE, AP_INVALID_EXPIRY, AP_TIME_OVERFLOW,
    AP_EVIDENCE_EXPIRED, AP_RESTORATION_OUTSTANDING, AP_STALE_TOKEN,
    AP_UNEXPECTED_ACK, AP_ATTACH_FAILED, AP_RUNTIME_FAULT,
    AP_RESTORE_FAILED, AP_GENERATION_EXHAUSTED
} ap_refusal;

typedef struct {
    ap_desired desired;
    ap_observed observed;
    ap_action action;
    ap_refusal refusal;
    uint64_t token;
    int restoration_outstanding;
} ap_status;

/* Caller-owned, single-threaded storage. Do not edit fields directly.
 * BEFORE the first ap_init, zero-initialize with AP_POLICY_INITIALIZER.
 * Never zero/copy/reset a live policy. ap_init rejects live/uncertain instances.
 * ap_init may retarget an inactive instance; generation remains monotonic.
 * No API frees resources or proves a hook/device/model works. Caller performs
 * actions and acknowledges completion after its own synchronization.
 */
typedef struct {
    uint32_t initialized;
    ap_identity target;
    ap_evidence evidence;
    int has_evidence;
    uint64_t last_now_ms;
    uint64_t generation;
    ap_status status;
} ap_policy;
#define AP_POLICY_INITIALIZER {0}

/* Keys: nonempty ASCII [A-Za-z0-9._:-], bounded NUL termination.
 * Hash: exactly 64 lowercase hex characters and a terminating NUL.
 * All identity comparisons are exact, not prefixes; identities are copied.
 */
int ap_identity_valid(const ap_identity *identity);
ap_refusal ap_init(ap_policy *policy, const ap_identity *target, uint64_t now_ms);
ap_refusal ap_update(ap_policy *policy, const ap_evidence *evidence, uint64_t now_ms);
ap_refusal ap_tick(ap_policy *policy, uint64_t now_ms);
ap_refusal ap_invalidate(ap_policy *policy);
ap_refusal ap_request(ap_policy *policy, ap_desired desired, uint64_t now_ms);
ap_refusal ap_ack_attach(ap_policy *policy, const ap_identity *identity,
                         uint64_t token, ap_ack result, uint64_t now_ms);
ap_refusal ap_ack_restore(ap_policy *policy, const ap_identity *identity,
                          uint64_t token, ap_ack result);
ap_refusal ap_report_fault(ap_policy *policy);
ap_refusal ap_query(const ap_policy *policy, ap_status *status);
const char *ap_refusal_name(ap_refusal refusal);
const char *ap_action_name(ap_action action);
#endif
