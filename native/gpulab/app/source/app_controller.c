#include "app_controller.h"
#include <string.h>

static ac_result ready(const app_controller *c)
{
    if (!c || c->initialized != 1) return AC_INVALID;
    return c->closed ? AC_CLOSED : AC_OK;
}
static ac_result clock_step(app_controller *c, uint64_t now)
{
    ac_result r=ready(c);
    if (r!=AC_OK) return r;
    if (c->time_known && now<c->last_now) return AC_TIME_REVERSED;
    c->time_known=1;c->last_now=now;return AC_OK;
}
static ap_refusal runtime_ready(app_controller *c,uint64_t now)
{
    ac_result r=clock_step(c,now);
    if (r==AC_TIME_REVERSED) return AP_TIME_REVERSED;
    if (r!=AC_OK || !c->current) return AP_NOT_INITIALIZED;
    return AP_OK;
}
static int equal(const ap_identity *a,const ap_identity *b)
{
    return ap_identity_valid(a)==1 && ap_identity_valid(b)==1 &&
        strcmp(a->device,b->device)==0 && strcmp(a->title,b->title)==0 &&
        strcmp(a->build,b->build)==0 && strcmp(a->executable_hash,b->executable_hash)==0;
}
static int blocked(const as_view *v)
{
    return !v->runtime_valid || v->runtime.durable_block || v->runtime.recovery_required ||
        v->runtime.policy.restoration_outstanding || v->runtime.policy.action!=AP_ACTION_NONE ||
        v->runtime.policy.observed==AP_ACTIVE || v->runtime.policy.observed==AP_PENDING_ATTACH ||
        v->runtime.policy.observed==AP_RESTORE_REQUIRED || v->runtime.policy.observed==AP_FAULT;
}
/* A missing qualification is normal only for a known, safely idle session.
 * This permits status/navigation; it never authorizes a runtime ON request.
 * Every other non-OK tick remains an error, even when observed is inactive. */
static int tick_usable(const as_view *v,ap_refusal tick)
{
    if (tick==AP_OK) return 1;
    return tick==AP_MISSING_EVIDENCE && v->lifecycle==AS_OPEN &&
        v->runtime_valid==1 && !blocked(v) && v->runtime.policy.observed==AP_INACTIVE;
}
ac_result ac_init(app_controller *c,const ac_host *h)
{
    if (!c || c->initialized || !h || !h->root || !h->root[0] ||
        !h->admit || !h->acquire || !h->retire) return AC_INVALID;
    c->host=*h;c->initialized=1;return AC_OK;
}
ac_result ac_select(app_controller *c,const ap_identity *id,uint64_t now)
{
    as_view v;as_session *next;ap_identity target;ac_result r=clock_step(c,now);
    if (r!=AC_OK) return r;
    if (!id || ap_identity_valid(id)!=1) return AC_INVALID;
    target=*id;
    if (c->host.admit(c->host.context,c->host.root,&target)!=1) return AC_DENIED;
    if (c->current) {
        c->runtime_result=as_tick(c->current,now);
        if (!as_query(c->current,&v) || blocked(&v)) return AC_BLOCKED;
        if (v.lifecycle!=AS_OPEN || !tick_usable(&v,c->runtime_result)) return AC_SESSION_ERROR;
        if (equal(&v.identity,&target)) return AC_OK;
        c->session_result=as_close(c->current,now);
        if (c->session_result!=AS_OK) return AC_SESSION_ERROR;
        c->host.retire(c->host.context,c->current);c->current=NULL;
    }
    next=c->host.acquire(c->host.context);
    if (!next) return AC_SESSION_ERROR;
    c->current=next;
    if (next->view.lifecycle!=AS_NEW) return AC_SESSION_ERROR;
    c->session_result=as_open(next,c->host.root,&target,c->host.admit,c->host.context,now);
    if (c->session_result!=AS_OK || !as_query(next,&v)) return AC_SESSION_ERROR;
    if (!equal(&v.identity,&target) || v.lifecycle!=AS_OPEN) return AC_SESSION_ERROR;
    if (blocked(&v)) return AC_BLOCKED;
    return AC_OK;
}
as_result ac_save(app_controller *c,ap_desired desired,unsigned quality,uint64_t rev)
{
    ac_result r=ready(c);
    if (r==AC_CLOSED) return AS_CLOSED;
    if (r!=AC_OK || !c->current) return AS_INVALID;
    c->session_result=as_save_preference(c->current,desired,quality,rev);
    return c->session_result;
}
ap_refusal ac_enable(app_controller *c,uint64_t now)
{
    as_view v;ap_refusal r=runtime_ready(c,now);
    if (r!=AP_OK) return r;
    c->runtime_result=as_tick(c->current,now);
    if (c->runtime_result==AP_TIME_REVERSED) return c->runtime_result;
    if (!as_query(c->current,&v) || !v.runtime_valid) return AP_RUNTIME_FAULT;
    if (v.lifecycle!=AS_OPEN) return AP_NOT_INITIALIZED;
    if (v.runtime.durable_block || v.runtime.recovery_required || v.runtime.policy.restoration_outstanding ||
        v.runtime.policy.action==AP_ACTION_RESTORE_ORIGINAL || v.runtime.policy.observed==AP_RESTORE_REQUIRED)
        return AP_RESTORATION_OUTSTANDING;
    if (v.storage_fault || v.runtime.policy.observed==AP_FAULT) return AP_RUNTIME_FAULT;
    if (c->runtime_result!=AP_OK) return c->runtime_result;
    c->runtime_result=as_request_on(c->current,now);return c->runtime_result;
}
ap_refusal ac_disable(app_controller *c,uint64_t now)
{
    ap_refusal r=runtime_ready(c,now);
    if (r!=AP_OK) return r;
    c->runtime_result=as_disable(c->current,now);return c->runtime_result;
}
ac_result ac_poll(app_controller *c,uint64_t now,const dc_entry *label,app_status_view *out,asv_result *vr)
{
    as_view v;ac_result r;
    if (out) (void)asv_build(NULL,NULL,out);
    if (vr) *vr=ASV_INVALID;
    if (!out || !vr) return AC_INVALID;
    r=clock_step(c,now);if (r!=AC_OK) return r;
    if (!c->current) return AC_NO_SESSION;
    c->runtime_result=as_tick(c->current,now);
    if (!as_query(c->current,&v)) return AC_SESSION_ERROR;
    *vr=asv_build(&v,label,out);
    return v.lifecycle==AS_OPEN && v.runtime_valid==1 &&
        v.runtime.policy.observed!=AP_FAULT && tick_usable(&v,c->runtime_result) &&
        *vr==ASV_OK ? AC_OK : AC_SESSION_ERROR;
}
ac_result ac_close(app_controller *c,uint64_t now)
{
    ac_result r=ready(c);
    if (r==AC_CLOSED) return AC_OK;
    if (r!=AC_OK) return r;
    r=clock_step(c,now);if (r!=AC_OK) return r;
    if (c->current) {
        c->session_result=as_close(c->current,now);
        if (c->session_result!=AS_OK) return AC_SESSION_ERROR;
        c->host.retire(c->host.context,c->current);c->current=NULL;
    }
    c->closed=1;return AC_OK;
}
