#ifndef APP_FRONTEND_H
#define APP_FRONTEND_H
#include "app_controller.h"
typedef enum { AF_SELECT=0,AF_SAVE,AF_ENABLE,AF_DISABLE,AF_REFRESH,AF_CLOSE } af_event_kind;
typedef struct {
    af_event_kind kind;
    union {
        ap_identity identity;
        struct { ap_desired desired;unsigned quality;uint64_t revision; } save;
    } value;
} af_event;
typedef enum { AF_OK=0,AF_INVALID,AF_BUSY,AF_CLOCK_ERROR,AF_TIME_REVERSED,
               AF_CLOSED,AF_COMMAND_REFUSED,AF_STATUS_ERROR } af_result;
typedef struct {
    af_result result;
    af_event_kind event;
    ac_result controller_result,refresh_result;
    as_result save_result;
    ap_refusal runtime_result;
    asv_result display_result;
    app_status_view view;
    int view_available,closed;
} af_outcome;
typedef int (*af_clock_fn)(void *,uint64_t *);
typedef void (*af_publish_fn)(void *,const app_status_view *,const af_outcome *);
typedef struct { af_clock_fn clock;af_publish_fn publish;void *context; } af_host;
typedef struct {
    app_controller *controller;
    af_host host;
    uint64_t last_now;
    int initialized,busy,closed,time_known;
} app_frontend;
#define APP_FRONTEND_INITIALIZER {0}
/* Pure portable event orchestration, no input device, renderer or executor.
 * Use fresh zero storage at stable addresses. Never copy/reset live frontend
 * or controller. Controller must already have successful ac_init and remains
 * exclusively serialized through this frontend. Controller, host/context and
 * controller dependencies outlive frontend and all callbacks. No controller
 * ownership/admission/factory/qualification or acknowledgement is supplied.
 * Init performs no callbacks or controller operations and never enables ON. */
af_result af_init(app_frontend *,app_controller *,const af_host *);
/* Exactly one event per call, no loop. Input/output/frontend must not overlap.
 * Clock must return exactly1 with monotonic time. Bad input is rejected before
 * any host/controller call. Callbacks are synchronous, borrowed read-only
 * arguments valid only until return; copy bounded data if retaining it.
 * Reentry returns AF_BUSY; callbacks must not mutate frontend/controller.
 * Mutating events are followed by one same-time status poll and publication,
 * even on refusal, with command result separate from status-poll result.
 * A successful close makes frontend terminal with NO further poll/publication
 * or callback, including later calls. Failed close remains usable.
 * Unknown title label is deliberate: presentation data grants no authority.
 * Results are request/status evidence only, never activation or render proof. */
af_result af_step(app_frontend *,const af_event *,af_outcome *);
#endif
