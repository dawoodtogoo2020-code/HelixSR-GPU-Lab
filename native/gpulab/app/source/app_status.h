#ifndef APP_STATUS_H
#define APP_STATUS_H
#include "app_session.h"
#include "display_catalog.h"

/* Pure presentation of a trusted serialized cached snapshot. No qualification,
 * device proof, freshness grant, executor or button permission is produced.
 * No caller should use this display value to authorize runtime actions.
 * Strings are bounded plain text, never HTML or terminal markup.
 */
typedef enum { ASV_OK=0, ASV_INVALID, ASV_LABEL_MISMATCH } asv_result;
typedef struct {
    int identity_known, preference_known, runtime_known;
    ap_identity identity;
    char title[DC_LABEL_BYTES];
    char saved_text[32], runtime_text[40], recovery_text[64], refusal_text[64];
    ap_desired saved_desired;
    unsigned saved_quality;
    uint64_t saved_revision;
    ap_observed reported_observed;
    ap_action reported_action;
    ap_refusal reported_refusal;
    int storage_fault, durable_block, recovery_required;
} app_status_view;

/* Input and output must not overlap. Null label means unknown display title.
 * Non-OK always leaves nonnull output in fixed safe Unknown state.
 * Valid metadata labels never admit identity or runtime availability.
 */
asv_result asv_build(const as_view *, const dc_entry *, app_status_view *);
#endif
