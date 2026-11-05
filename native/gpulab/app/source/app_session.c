#include <string.h>

#include "app_session.h"

#define AS_QUALITY_MAX 4u

static int desired_valid(ap_desired d)
{
    return d == AP_OFF || d == AP_ON;
}

static void safe_preference(as_session *s)
{
    memset(&s->view.preference, 0, sizeof s->view.preference);
    s->view.preference.identity = s->view.identity;
    s->view.preference.desired = AP_OFF;
    s->view.preference_valid = 0;
}

/* Refresh the cached runtime snapshot; never present stale data as valid. */
static int refresh_runtime(as_session *s)
{
    cs_status st;

    memset(&st, 0, sizeof st);
    if (cs_query(&s->control, &st)) {
        s->view.runtime = st;
        s->view.runtime_valid = 1;
        return 1;
    }
    s->view.runtime_valid = 0;
    s->view.storage_fault = 1;
    return 0;
}

static as_result open_fail(as_session *s, as_result r)
{
    s->view.lifecycle = AS_FAILED;
    s->view.last_result = r;
    return r;
}

static int loaded_acceptable(ps_result r, const ps_profile *p,
                             const ap_identity *id)
{
    if (r != PS_OK && r != PS_DEFAULT)
        return 0;
    if (!ps_identity_equal(&p->identity, id))
        return 0;
    if (!desired_valid(p->desired))
        return 0;
    if ((unsigned long long)p->quality > AS_QUALITY_MAX)
        return 0;
    if (r == PS_DEFAULT && (p->desired != AP_OFF || p->revision != 0))
        return 0;
    return 1;
}

as_result as_open(as_session *s, const char *root, const ap_identity *identity,
                  as_admit_fn admit, void *admission_context, uint64_t now)
{
    const ps_io *io;
    ps_profile loaded;
    ps_result r;
    as_result result = AS_OK;

    if (s == NULL)
        return AS_INVALID;
    if (s->view.lifecycle != AS_NEW)
        return AS_INVALID;

    /* Every attempt consumes the instance before any validation. */
    s->view.lifecycle = AS_FAILED;

    if (root == NULL || root[0] == '\0' || identity == NULL || admit == NULL)
        return open_fail(s, AS_INVALID);
    if (!ap_identity_valid(identity))
        return open_fail(s, AS_DENIED);
    s->view.identity = *identity;
    if (admit(admission_context, root, identity) != 1)
        return open_fail(s, AS_DENIED);

    r = pfa_open(&s->store, root, identity);
    s->view.store_result = r;
    if (r != PS_OK) {
        s->view.storage_fault = 1;
        safe_preference(s);
        return open_fail(s, AS_IOFAULT);
    }
    s->store_open = 1;

    io = pfa_get_io(&s->store);
    if (io == NULL) {
        s->view.storage_fault = 1;
        s->view.store_result = PS_IO_ERROR;
        safe_preference(s);
        return open_fail(s, AS_IOFAULT);
    }

    memset(&loaded, 0, sizeof loaded);
    r = ps_load(io, identity, &loaded);
    if (loaded_acceptable(r, &loaded, identity)) {
        s->view.preference = loaded;
        s->view.preference_valid = 1;
        s->view.store_result = r;
    } else {
        /* Never expose partial bytes from a failed load. */
        safe_preference(s);
        s->view.storage_fault = 1;
        s->view.store_result =
            (r == PS_OK || r == PS_DEFAULT) ? PS_CORRUPT : r;
        result = AS_IOFAULT;
    }

    /* Boot control even after a profile fault so OFF and recovery stay
     * reachable and visible. */
    s->control_started = 1;
    r = cs_init(&s->control, io, identity, now);
    if (r != PS_OK) {
        if (!s->view.storage_fault)
            s->view.store_result = r;
        s->view.storage_fault = 1;
        result = AS_IOFAULT;
    }
    if (!refresh_runtime(s))
        result = AS_IOFAULT;

    s->view.lifecycle = AS_OPEN;
    s->view.last_result = result;
    return result;
}

as_result as_save_preference(as_session *s, ap_desired desired,
                             unsigned quality, uint64_t expected_revision)
{
    const ps_io *io;
    ps_profile candidate;
    ps_profile saved;
    ps_result r;
    as_result result;

    if (s == NULL)
        return AS_INVALID;
    if (s->view.lifecycle == AS_DONE)
        return AS_CLOSED;
    if (s->view.lifecycle != AS_OPEN)
        return AS_INVALID;

    if (s->view.storage_fault || !s->view.preference_valid) {
        result = AS_IOFAULT;
    } else if (!desired_valid(desired) || quality > AS_QUALITY_MAX) {
        result = AS_INVALID;
    } else if ((uint64_t)s->view.preference.revision != expected_revision) {
        result = AS_CONFLICT;
    } else {
        io = pfa_get_io(&s->store);
        if (io == NULL) {
            s->view.storage_fault = 1;
            s->view.store_result = PS_IO_ERROR;
            result = AS_IOFAULT;
        } else {
            candidate = s->view.preference;
            candidate.desired = desired;
            candidate.quality = quality;
            memset(&saved, 0, sizeof saved);
            r = ps_save(io, &candidate, expected_revision, &saved);
            s->view.store_result = r;
            if (r == PS_OK) {
                s->view.preference = saved;
                s->view.preference_valid = 1;
                result = AS_OK;
            } else if (r == PS_CONFLICT) {
                result = AS_CONFLICT;
            } else {
                s->view.storage_fault = 1;
                result = AS_IOFAULT;
            }
        }
    }
    s->view.last_result = result;
    return result;
}

ap_refusal as_request_on(as_session *s, uint64_t now)
{
    ap_refusal r;

    if (s == NULL)
        return AP_INVALID_ARGUMENT;
    if (s->view.lifecycle != AS_OPEN || !s->control_started)
        return AP_NOT_INITIALIZED;
    if (!s->view.runtime_valid || s->view.storage_fault)
        return AP_RUNTIME_FAULT;
    r = cs_request(&s->control, AP_ON, now);
    refresh_runtime(s);
    return r;
}

ap_refusal as_disable(as_session *s, uint64_t now)
{
    ap_refusal r;

    if (s == NULL)
        return AP_INVALID_ARGUMENT;
    if (s->view.lifecycle != AS_OPEN || !s->control_started)
        return AP_NOT_INITIALIZED;
    r = cs_request(&s->control, AP_OFF, now);
    refresh_runtime(s);
    return r;
}

ap_refusal as_tick(as_session *s, uint64_t now)
{
    ap_refusal r;

    if (s == NULL)
        return AP_INVALID_ARGUMENT;
    if (s->view.lifecycle != AS_OPEN || !s->control_started)
        return AP_NOT_INITIALIZED;
    r = cs_tick(&s->control, now);
    refresh_runtime(s);
    return r;
}

int as_query(const as_session *s, as_view *out)
{
    if (s == NULL || out == NULL)
        return 0;
    if (s->view.lifecycle == AS_NEW)
        return 0;
    *out = s->view;
    return 1;
}

static as_result finish_close(as_session *s)
{
    ps_result r;
    as_result result = AS_OK;

    if (s->store_open) {
        r = pfa_close(&s->store);
        s->store_open = 0;
        if (r != PS_OK) {
            s->view.store_result = r;
            s->view.storage_fault = 1;
            result = AS_IOFAULT;
        }
    }
    s->view.lifecycle = AS_DONE;
    s->view.last_result = result;
    return result;
}

as_result as_close(as_session *s, uint64_t now)
{
    ap_refusal r;
    const cs_status *st;

    if (s == NULL || s->view.lifecycle == AS_NEW)
        return AS_INVALID;
    if (s->view.lifecycle == AS_DONE)
        return s->view.last_result;

    if (s->control_started) {
        r = cs_request(&s->control, AP_OFF, now);
        if (!refresh_runtime(s)) {
            s->view.last_result = AS_BUSY;
            return AS_BUSY;
        }
        if (r == AP_TIME_REVERSED) {
            s->view.last_result = AS_TIME_REVERSED;
            return AS_TIME_REVERSED;
        }
        st = &s->view.runtime;
        if (st->policy.observed == AP_ACTIVE ||
            st->policy.observed == AP_PENDING_ATTACH ||
            st->policy.restoration_outstanding ||
            (int)st->policy.action != 0) {
            s->view.last_result = AS_BUSY;
            return AS_BUSY;
        }
    }
    return finish_close(s);
}
