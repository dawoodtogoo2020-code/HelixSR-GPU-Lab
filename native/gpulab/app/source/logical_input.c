#include "logical_input.h"
#include <string.h>

li_result li_translate(const li_intent *intent, af_event *out)
{
    af_event event;
    if (!intent || !out) return LI_INVALID;
    /* Validate completely before constructing or committing output. */
    switch (intent->kind) {
    case LI_SELECT:
        if (ap_identity_valid(&intent->value.identity)!=1) return LI_INVALID;
        break;
    case LI_SAVE:
        if ((intent->value.save.desired!=AP_OFF && intent->value.save.desired!=AP_ON)
            || intent->value.save.quality>4u) return LI_INVALID;
        break;
    case LI_ENABLE: case LI_DISABLE: case LI_REFRESH: case LI_CLOSE: break;
    default: return LI_INVALID;
    }
    memset(&event,0,sizeof event);
    switch (intent->kind) {
    case LI_SELECT: event.kind=AF_SELECT; event.value.identity=intent->value.identity; break;
    case LI_SAVE:
        event.kind=AF_SAVE;
        event.value.save.desired=intent->value.save.desired;
        event.value.save.quality=intent->value.save.quality;
        event.value.save.revision=intent->value.save.revision;
        break;
    case LI_ENABLE: event.kind=AF_ENABLE; break;
    case LI_DISABLE: event.kind=AF_DISABLE; break;
    case LI_REFRESH: event.kind=AF_REFRESH; break;
    case LI_CLOSE: event.kind=AF_CLOSE; break;
    default: return LI_INVALID;
    }
    *out=event;
    return LI_EMITTED;
}

li_result li_step(li_gate *gate, const li_sample *sample, af_event *out)
{
    af_event event;
    if (!gate || !sample || !out || gate->held>1u) return LI_INVALID;
    switch (sample->phase) {
    case LI_RELEASE:
        if (sample->count!=0u || sample->intents!=NULL) return LI_INVALID;
        gate->held=0u;
        return LI_RELEASED;
    case LI_PRESS:
        if (sample->count!=1u || !sample->intents) return LI_INVALID;
        if (li_translate(sample->intents,&event)!=LI_EMITTED) return LI_INVALID;
        if (gate->held==1u) return LI_SUPPRESSED;
        *out=event;
        gate->held=1u;
        return LI_EMITTED;
    default: return LI_INVALID;
    }
}
