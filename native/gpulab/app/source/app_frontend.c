#include "app_frontend.h"
#include <string.h>
static int valid_event(const af_event *e)
{
    if (!e || e->kind<AF_SELECT || e->kind>AF_CLOSE) return 0;
    if (e->kind==AF_SELECT) return ap_identity_valid(&e->value.identity)==1;
    if (e->kind==AF_SAVE) return (e->value.save.desired==AP_OFF || e->value.save.desired==AP_ON) && e->value.save.quality<=4;
    return 1;
}
static void defaults(af_outcome *o)
{
    memset(o,0,sizeof *o);o->result=AF_INVALID;
    o->controller_result=o->refresh_result=AC_INVALID;o->save_result=AS_INVALID;
    o->runtime_result=AP_INVALID_ARGUMENT;o->display_result=ASV_INVALID;
    (void)asv_build(NULL,NULL,&o->view);
}
af_result af_init(app_frontend *f,app_controller *controller,const af_host *host)
{
    if (!f || f->initialized || !controller || !host || !host->clock || !host->publish) return AF_INVALID;
    f->controller=controller;f->host=*host;f->initialized=1;return AF_OK;
}
af_result af_step(app_frontend *f,const af_event *e,af_outcome *o)
{
    uint64_t now;af_result result=AF_OK;af_event intent;
    if (!o) return AF_INVALID;
    defaults(o);
    if (!f || f->initialized!=1) return AF_INVALID;
    if (f->busy) return o->result=AF_BUSY;
    if (f->closed) {o->closed=1;return o->result=AF_CLOSED;}
    if (!valid_event(e)) return AF_INVALID;
    intent=*e;e=&intent;
    o->event=e->kind;f->busy=1;
    if (f->host.clock(f->host.context,&now)!=1) {result=AF_CLOCK_ERROR;goto finish;}
    if (f->time_known && now<f->last_now) {result=AF_TIME_REVERSED;goto finish;}
    f->time_known=1;f->last_now=now;
    switch (e->kind) {
    case AF_SELECT:
        o->controller_result=ac_select(f->controller,&e->value.identity,now);
        if (o->controller_result!=AC_OK) result=AF_COMMAND_REFUSED;
        break;
    case AF_SAVE:
        o->save_result=ac_save(f->controller,e->value.save.desired,e->value.save.quality,e->value.save.revision);
        if (o->save_result!=AS_OK) result=AF_COMMAND_REFUSED;
        break;
    case AF_ENABLE:
        o->runtime_result=ac_enable(f->controller,now);
        if (o->runtime_result!=AP_OK) result=AF_COMMAND_REFUSED;
        break;
    case AF_DISABLE:
        o->runtime_result=ac_disable(f->controller,now);
        if (o->runtime_result!=AP_OK) result=AF_COMMAND_REFUSED;
        break;
    case AF_REFRESH: break;
    case AF_CLOSE:
        o->controller_result=ac_close(f->controller,now);
        if (o->controller_result==AC_OK) {f->closed=1;o->closed=1;goto finish;}
        result=AF_COMMAND_REFUSED;break;
    }
    o->refresh_result=ac_poll(f->controller,now,NULL,&o->view,&o->display_result);
    o->view_available=o->display_result==ASV_OK;
    if (e->kind==AF_REFRESH) o->controller_result=o->refresh_result;
    if (result==AF_OK && o->refresh_result!=AC_OK) result=AF_STATUS_ERROR;
    o->result=result;
    f->host.publish(f->host.context,&o->view,o);
finish:
    o->result=result;f->busy=0;return result;
}
