#include "activation_policy.h"
#include <stddef.h>
#include <string.h>

#define AP_MAGIC UINT32_C(0x41504331)

static int key_valid(const char *s, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0) return i != 0;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == ':' || c == '-')) return 0;
    }
    return 0;
}

int ap_identity_valid(const ap_identity *id)
{
    size_t i;
    if (!id || !key_valid(id->device, AP_KEY_SIZE) ||
        !key_valid(id->title, AP_KEY_SIZE) || !key_valid(id->build, AP_KEY_SIZE)) return 0;
    for (i = 0; i < 64; ++i) {
        char c = id->executable_hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return id->executable_hash[64] == 0;
}

static int same(const ap_identity *a, const ap_identity *b)
{
    /* Both have already passed bounded validation. */
    return strcmp(a->device, b->device) == 0 && strcmp(a->title, b->title) == 0 &&
           strcmp(a->build, b->build) == 0 &&
           memcmp(a->executable_hash, b->executable_hash, AP_HASH_SIZE) == 0;
}

static ap_refusal ready(const ap_policy *p)
{
    if (!p) return AP_INVALID_ARGUMENT;
    return p->initialized == AP_MAGIC ? AP_OK : AP_NOT_INITIALIZED;
}

static ap_refusal refuse(ap_policy *p, ap_refusal r)
{
    p->status.refusal = r;
    return r;
}

static void restore_required(ap_policy *p, int fault)
{
    if (p->status.observed == AP_PENDING_ATTACH || p->status.observed == AP_ACTIVE ||
        p->status.restoration_outstanding) {
        p->status.restoration_outstanding = 1;
        /* Preserve a restore failure until a successful restoration ACK. */
        if (fault || p->status.observed == AP_FAULT) p->status.observed = AP_FAULT;
        else p->status.observed = AP_RESTORE_REQUIRED;
        p->status.action = AP_ACTION_RESTORE_ORIGINAL;
    }
}

static ap_refusal time_valid(ap_policy *p, uint64_t now)
{
    if (now < p->last_now_ms) {
        restore_required(p, 0);
        return AP_TIME_REVERSED;
    }
    p->last_now_ms = now;
    return AP_OK;
}

static ap_refusal evidence_valid(const ap_policy *p, uint64_t now)
{
    const ap_evidence *e = &p->evidence;
    int flags[6];
    size_t i;
    if (!p->has_evidence) return AP_MISSING_EVIDENCE;
    if (!ap_identity_valid(&e->identity)) return AP_INVALID_IDENTITY;
    if (!same(&p->target, &e->identity)) return AP_IDENTITY_MISMATCH;
    flags[0] = e->backend; flags[1] = e->qualified_model; flags[2] = e->interoperability;
    flags[3] = e->exact_build_hook; flags[4] = e->temporal_contract; flags[5] = e->recovery_path;
    for (i = 0; i < 6; ++i) if (flags[i] != 0 && flags[i] != 1) return AP_INVALID_BOOLEAN;
    for (i = 0; i < 6; ++i) if (flags[i] == 0) return AP_CAPABILITY_MISSING;
    if (e->observed_ms > now) return AP_EVIDENCE_FUTURE;
    if (e->lifetime_ms == 0 || e->lifetime_ms > AP_MAX_EVIDENCE_AGE_MS) return AP_INVALID_EXPIRY;
    if (e->observed_ms > UINT64_MAX - e->lifetime_ms) return AP_TIME_OVERFLOW;
    /* Half-open validity interval: expired exactly at observed + lifetime. */
    if (now >= e->observed_ms + e->lifetime_ms) return AP_EVIDENCE_EXPIRED;
    return AP_OK;
}

ap_refusal ap_init(ap_policy *p, const ap_identity *target, uint64_t now)
{
    ap_identity copy;
    uint64_t generation;
    if (!p || !target) return AP_INVALID_ARGUMENT;
    if (p->initialized != 0 && p->initialized != AP_MAGIC) return AP_NOT_INITIALIZED;
    if (p->initialized == AP_MAGIC &&
        (p->status.observed != AP_INACTIVE || p->status.restoration_outstanding))
        return refuse(p, AP_ALREADY_LIVE);
    if (!ap_identity_valid(target)) return AP_INVALID_IDENTITY;
    if (p->initialized == AP_MAGIC && now < p->last_now_ms) return refuse(p, AP_TIME_REVERSED);
    copy = *target; /* Also works if target is the policy's own target. */
    generation = p->initialized == AP_MAGIC ? p->generation : 0;
    memset(p, 0, sizeof(*p));
    p->initialized = AP_MAGIC;
    p->target = copy;
    p->generation = generation;
    p->last_now_ms = now;
    return AP_OK;
}

ap_refusal ap_tick(ap_policy *p, uint64_t now)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    r = time_valid(p, now);
    if (r == AP_OK) r = evidence_valid(p, now);
    if (r != AP_OK) restore_required(p, 0);
    return refuse(p, r);
}

ap_refusal ap_update(ap_policy *p, const ap_evidence *e, uint64_t now)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    r = time_valid(p, now);
    if (r != AP_OK) return refuse(p, r);
    if (e) { p->evidence = *e; p->has_evidence = 1; }
    else p->has_evidence = 0;
    return ap_tick(p, now);
}

ap_refusal ap_invalidate(ap_policy *p)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    p->has_evidence = 0;
    restore_required(p, 0);
    return refuse(p, AP_MISSING_EVIDENCE);
}

ap_refusal ap_request(ap_policy *p, ap_desired desired, uint64_t now)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    if (desired != AP_OFF && desired != AP_ON) return refuse(p, AP_INVALID_ARGUMENT);
    /* Preserve the user's intent even when qualification refuses the action. */
    p->status.desired = desired;
    r = time_valid(p, now);
    if (desired == AP_OFF) {
        restore_required(p, 0);
        return refuse(p, r);
    }
    if (r != AP_OK) return refuse(p, r);
    if (p->status.restoration_outstanding) return refuse(p, AP_RESTORATION_OUTSTANDING);
    r = evidence_valid(p, now);
    if (r != AP_OK) { restore_required(p, 0); return refuse(p, r); }
    if (p->status.observed == AP_PENDING_ATTACH || p->status.observed == AP_ACTIVE)
        return refuse(p, AP_OK);
    if (p->generation == UINT64_MAX) return refuse(p, AP_GENERATION_EXHAUSTED);
    p->status.token = ++p->generation;
    p->status.observed = AP_PENDING_ATTACH;
    p->status.action = AP_ACTION_ATTACH;
    return refuse(p, AP_OK);
}

static ap_refusal ack_valid(ap_policy *p, const ap_identity *id, uint64_t token, ap_ack result)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    if (!id || (result != AP_ACK_FAILED && result != AP_ACK_SUCCESS && result != AP_ACK_UNCERTAIN))
        return AP_INVALID_ARGUMENT;
    if (!ap_identity_valid(id)) return AP_INVALID_IDENTITY;
    if (!same(&p->target, id)) return AP_IDENTITY_MISMATCH;
    if (token == 0 || token != p->status.token) return AP_STALE_TOKEN;
    return AP_OK;
}

ap_refusal ap_ack_attach(ap_policy *p, const ap_identity *id, uint64_t token, ap_ack result, uint64_t now)
{
    ap_refusal r = ack_valid(p, id, token, result);
    /* Rejected acknowledgements never change state, including time bookkeeping. */
    if (r != AP_OK) return r;
    if (p->status.observed != AP_PENDING_ATTACH || p->status.desired != AP_ON)
        return refuse(p, AP_UNEXPECTED_ACK);
    r = ap_tick(p, now);
    if (r != AP_OK) return r;
    if (result != AP_ACK_SUCCESS) {
        restore_required(p, 1);
        return refuse(p, AP_ATTACH_FAILED);
    }
    p->status.observed = AP_ACTIVE;
    p->status.action = AP_ACTION_NONE;
    return refuse(p, AP_OK);
}

ap_refusal ap_ack_restore(ap_policy *p, const ap_identity *id, uint64_t token, ap_ack result)
{
    ap_refusal r = ack_valid(p, id, token, result);
    if (r != AP_OK) return r;
    if (!p->status.restoration_outstanding) return refuse(p, AP_UNEXPECTED_ACK);
    if (result != AP_ACK_SUCCESS) {
        restore_required(p, 1);
        return refuse(p, AP_RESTORE_FAILED);
    }
    p->status.restoration_outstanding = 0;
    p->status.observed = AP_INACTIVE;
    p->status.action = AP_ACTION_NONE;
    return refuse(p, AP_OK);
}

ap_refusal ap_report_fault(ap_policy *p)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    restore_required(p, 1);
    return refuse(p, AP_RUNTIME_FAULT);
}

ap_refusal ap_query(const ap_policy *p, ap_status *s)
{
    ap_refusal r = ready(p);
    if (r != AP_OK) return r;
    if (!s) return AP_INVALID_ARGUMENT;
    *s = p->status;
    return AP_OK;
}

const char *ap_refusal_name(ap_refusal r)
{
    static const char *const names[] = {
        "ready", "invalid_argument", "not_initialized", "already_live",
        "invalid_identity", "identity_mismatch", "invalid_boolean",
        "missing_evidence", "capability_missing", "time_reversed",
        "evidence_future", "invalid_expiry", "time_overflow", "evidence_expired",
        "restoration_outstanding", "stale_token", "unexpected_ack", "attach_failed",
        "runtime_fault", "restore_failed", "generation_exhausted"
    };
    if ((unsigned)r >= sizeof(names) / sizeof(names[0])) return "unknown_refusal";
    return names[(unsigned)r];
}

const char *ap_action_name(ap_action a)
{
    switch (a) {
    case AP_ACTION_NONE: return "none";
    case AP_ACTION_ATTACH: return "attach_requested";
    case AP_ACTION_RESTORE_ORIGINAL: return "restore_original_pass";
    default: return "unknown_action";
    }
}
