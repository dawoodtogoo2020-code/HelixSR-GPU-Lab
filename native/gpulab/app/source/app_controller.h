#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H
#include "app_status.h"

/* Serialized trusted host, no renderer/input/executor. UI supplies only exact
 * identity and preference intents. Trusted callbacks own stable storage and
 * provisioning/admission; they must never recycle or reset live sessions.
 * admit must verify exact identity plus trusted provisioned root and global
 * recovery/rollback anchors. A UI flag, label or synthetic fixture cannot
 * implement production authority. Its approval is rechecked by as_open.
 * Before first init, use fresh APP_CONTROLLER_INITIALIZER zero storage.
 * Controller must remain at a stable address and must not be copied/reset.
 * acquire returns fresh zero initialized AS_NEW storage or NULL. retire only
 * receives terminal AS_DONE sessions after an AS_OK close. Root/context must
 * outlive the controller. No qualification or acknowledgement API exists. */
typedef as_session *(*ac_acquire_fn)(void *);
typedef void (*ac_retire_fn)(void *, as_session *);
typedef struct {
    const char *root;
    as_admit_fn admit;
    void *context;
    ac_acquire_fn acquire;
    ac_retire_fn retire;
} ac_host;
typedef enum { AC_OK=0, AC_INVALID, AC_DENIED, AC_BLOCKED, AC_SESSION_ERROR,
               AC_TIME_REVERSED, AC_CLOSED, AC_NO_SESSION } ac_result;
typedef struct {
    ac_host host;
    as_session *current;
    uint64_t last_now;
    int initialized, closed, time_known;
    as_result session_result;
    ap_refusal runtime_result;
} app_controller;
#define APP_CONTROLLER_INITIALIZER {0}
ac_result ac_init(app_controller *, const ac_host *);
/* Check admission before touching current, including same-identity reselection.
 * Same-identity selection refreshes and checks status without closing/reopening.
 * Refresh old session then block switching on unknown runtime, durable block,
 * recovery required, restoration outstanding or any pending action. Close old
 * first and retain it unless AS_OK; allocate/open new only after accepted close.
 * Failed/blocked new open remains current for explicit close/recovery and view.
 * Selection never requests ON, including when saved preference is ON. */
ac_result ac_select(app_controller *, const ap_identity *, uint64_t now);
as_result ac_save(app_controller *, ap_desired, unsigned quality, uint64_t revision);
ap_refusal ac_enable(app_controller *, uint64_t now);
/* Independent of save result; always forward valid OFF even during recovery. */
ap_refusal ac_disable(app_controller *, uint64_t now);
/* Tick then build pure display; label mismatch is reported by asv_build.
 * AP_MISSING_EVIDENCE is informational for known inactive sessions with no
 * recovery, durable block or action. Its actual refusal remains in the view.
 * It permits polling/navigation only; enable still refuses unqualified ON.
 * All other non-OK ticks remain errors. Unknown/fault runtime is not success. */
ac_result ac_poll(app_controller *, uint64_t now, const dc_entry *, app_status_view *, asv_result *);
/* Explicit shutdown requests OFF/close even when recovery blocks selection.
 * Preserve current and controller usability until AS_OK closure. Idempotent. */
ac_result ac_close(app_controller *, uint64_t now);
#endif
