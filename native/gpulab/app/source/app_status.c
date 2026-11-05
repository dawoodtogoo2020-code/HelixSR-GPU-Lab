#include "app_status.h"
#include <string.h>

#define TEXT_BAD ((size_t)-1)

static int in_range(long long v, long long lo, long long hi)
{
    return v >= lo && v <= hi;
}

static int is_bool(int v)
{
    return v == 0 || v == 1;
}

/* Length of a NUL-terminated printable ASCII string within cap bytes,
 * or TEXT_BAD. Reads stop at the first NUL or invalid byte. */
static size_t text_len(const char *s, size_t cap)
{
    size_t i;
    for (i = 0; i < cap; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == 0)
            return i;
        if (ch < 32 || ch > 126)
            return TEXT_BAD;
    }
    return TEXT_BAD;
}

static void set_text(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

#define SET_TEXT(field, s) set_text((field), sizeof(field), (s))

static void set_defaults(app_status_view *v)
{
    memset(v, 0, sizeof *v);
    SET_TEXT(v->title, "Unknown title");
    SET_TEXT(v->saved_text, "Unknown");
    SET_TEXT(v->runtime_text, "Unknown");
    SET_TEXT(v->recovery_text, "Unknown");
    SET_TEXT(v->refusal_text, "Unknown");
}

static int identity_equal(const ap_identity *a, const ap_identity *b)
{
    return strcmp(a->device, b->device) == 0 &&
           strcmp(a->title, b->title) == 0 &&
           strcmp(a->build, b->build) == 0 &&
           strcmp(a->executable_hash, b->executable_hash) == 0;
}

static const char *observed_text(ap_observed o)
{
    switch (o) {
    case AP_INACTIVE: return "Reported inactive";
    case AP_PENDING_ATTACH: return "Attach pending";
    case AP_ACTIVE: return "Reported active";
    case AP_RESTORE_REQUIRED: return "Restore required";
    case AP_FAULT: return "Runtime fault";
    }
    return "Unknown";
}

static int preference_ok(const as_view *view)
{
    const ps_profile *p = &view->preference;
    if (ap_identity_valid(&p->identity) != 1)
        return 0;
    if (!identity_equal(&p->identity, &view->identity))
        return 0;
    if (!in_range((long long)p->desired, (long long)AP_OFF, (long long)AP_ON))
        return 0;
    return p->quality <= 4u;
}

static int runtime_ok(const cs_status *r)
{
    const ap_status *s = &r->policy;
    if (!in_range((long long)s->observed, (long long)AP_INACTIVE, (long long)AP_FAULT))
        return 0;
    if (!in_range((long long)s->desired, (long long)AP_OFF, (long long)AP_ON))
        return 0;
    if (!in_range((long long)s->action, (long long)AP_ACTION_NONE,
                  (long long)AP_ACTION_RESTORE_ORIGINAL))
        return 0;
    if (!in_range((long long)s->refusal, (long long)AP_OK,
                  (long long)AP_GENERATION_EXHAUSTED))
        return 0;
    return is_bool(s->restoration_outstanding) && is_bool(r->durable_block) &&
           is_bool(r->recovery_required) && is_bool(r->backend_available);
}

static int recovery_condition(const cs_status *r)
{
    return r->durable_block == 1 || r->recovery_required == 1 ||
           r->policy.restoration_outstanding == 1 ||
           r->policy.observed == AP_RESTORE_REQUIRED;
}

static int label_ok(const dc_entry *label, size_t *len)
{
    size_t n;
    if (dc_identity_valid(&label->identity) != 1)
        return 0;
    n = text_len(label->label, sizeof label->label);
    if (n == TEXT_BAD || n < 1 || n > 95)
        return 0;
    *len = n;
    return 1;
}

static int label_matches(const dc_entry *label, const ap_identity *id)
{
    return strcmp(label->identity.device, id->device) == 0 &&
           strcmp(label->identity.title, id->title) == 0 &&
           strcmp(label->identity.build, id->build) == 0 &&
           strcmp(label->identity.digest, id->executable_hash) == 0;
}

static asv_result build(const as_view *view, const dc_entry *label,
                        app_status_view *c)
{
    as_lifecycle lc;
    size_t label_len = 0;

    if (!view)
        return ASV_INVALID;
    if (!in_range((long long)view->lifecycle, (long long)AS_NEW, (long long)AS_DONE))
        return ASV_INVALID;
    if (!in_range((long long)view->last_result, (long long)AS_OK, (long long)AS_CLOSED))
        return ASV_INVALID;
    if (!in_range((long long)view->store_result, (long long)PS_OK, (long long)PS_CORRUPT))
        return ASV_INVALID;
    if (!is_bool(view->preference_valid) || !is_bool(view->runtime_valid) ||
        !is_bool(view->storage_fault))
        return ASV_INVALID;

    lc = view->lifecycle;
    c->storage_fault = view->storage_fault;

    if (lc == AS_NEW || lc == AS_FAILED) {
        if (view->preference_valid || view->runtime_valid || label)
            return ASV_INVALID;
        SET_TEXT(c->runtime_text, lc == AS_NEW ? "Not opened" : "Session failed");
        return ASV_OK;
    }

    /* AS_OPEN or AS_DONE */
    if (ap_identity_valid(&view->identity) == 1) {
        c->identity_known = 1;
        c->identity = view->identity;
    } else if (lc == AS_OPEN || view->preference_valid) {
        return ASV_INVALID;
    }

    if (view->preference_valid) {
        const ps_profile *p = &view->preference;
        if (!preference_ok(view))
            return ASV_INVALID;
        c->preference_known = 1;
        c->saved_desired = p->desired;
        c->saved_quality = p->quality;
        c->saved_revision = p->revision;
        if (lc == AS_OPEN)
            SET_TEXT(c->saved_text, p->desired == AP_ON ? "Saved on" : "Saved off");
        else
            SET_TEXT(c->saved_text,
                     p->desired == AP_ON ? "Last saved on" : "Last saved off");
    }

    if (lc == AS_DONE)
        SET_TEXT(c->runtime_text, "Session closed");

    if (view->runtime_valid) {
        const cs_status *r = &view->runtime;
        int recovery;
        if (!runtime_ok(r))
            return ASV_INVALID;
        recovery = recovery_condition(r);
        if (lc == AS_OPEN) {
            const char *name = ap_refusal_name(r->policy.refusal);
            size_t n;
            if (!name)
                return ASV_INVALID;
            n = text_len(name, sizeof c->refusal_text);
            if (n == TEXT_BAD || n < 1)
                return ASV_INVALID;
            memcpy(c->refusal_text, name, n);
            c->refusal_text[n] = '\0';
            c->runtime_known = 1;
            c->reported_observed = r->policy.observed;
            c->reported_action = r->policy.action;
            c->reported_refusal = r->policy.refusal;
            SET_TEXT(c->runtime_text, observed_text(r->policy.observed));
            SET_TEXT(c->recovery_text,
                     recovery ? "Recovery required" : "No recovery recorded");
        } else {
            SET_TEXT(c->recovery_text,
                     recovery ? "Recovery recorded" : "Recovery unknown");
        }
        c->durable_block = r->durable_block;
        c->recovery_required = r->recovery_required;
    }

    if (label) {
        if (!c->identity_known || !label_ok(label, &label_len))
            return ASV_INVALID;
        if (label_len >= sizeof c->title)
            return ASV_INVALID;
        if (!label_matches(label, &c->identity))
            return ASV_LABEL_MISMATCH;
        memcpy(c->title, label->label, label_len);
        c->title[label_len] = '\0';
    }

    return ASV_OK;
}

asv_result asv_build(const as_view *view, const dc_entry *label,
                     app_status_view *out)
{
    app_status_view candidate;
    asv_result rc;

    if (!out)
        return ASV_INVALID;
    set_defaults(out);
    set_defaults(&candidate);
    rc = build(view, label, &candidate);
    if (rc == ASV_OK)
        *out = candidate;
    return rc;
}
