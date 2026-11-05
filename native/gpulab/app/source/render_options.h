#ifndef RENDER_OPTIONS_H
#define RENDER_OPTIONS_H
#include <stdint.h>
#include "activation_policy.h"
#ifdef __cplusplus
extern "C" {
#endif
enum ro_preset { RO_ULTRA_PERFORMANCE=0, RO_PERFORMANCE, RO_BALANCED,
                 RO_QUALITY, RO_NATIVE, RO_CUSTOM };
enum ro_backend { RO_HELIXSR=0, RO_HELIXSR_DLSS37, RO_FSR411_EXACT,
                  RO_FSR411_LOSSY, RO_PSSR=4, RO_GAME_DEFAULT=5 };
typedef struct ro_options {
    uint32_t input_width,input_height,output_width,output_height,preset,backend;
    uint64_t revision;
} ro_options;
enum ro_result { RO_OK=0, RO_DEFAULT=1, RO_INVALID=-1, RO_CONFLICT=-2,
                 RO_IO_ERROR=-3, RO_CORRUPT=-4 };
/* Configuration selections do not establish backend/model availability. */
void ro_defaults(ro_options *);
/* Scale both axes to nearest multiple of2; midpoint chooses even quotient.
 * Ratios:1/3,1/2,58/100,2/3,1. Custom preserves existing input dimensions.
 * Failure leaves the options unchanged. */
int ro_apply_preset(ro_options *,unsigned);
int ro_valid(const ro_options *);
const char *ro_preset_name(unsigned);
const char *ro_backend_name(unsigned);
/* Existing absolute private root must end in /render-settings. Never pass the
 * ON/OFF settings root. Identity-scoped301-byte record, versioned and sealed.
 * load:0persisted,1missing/default,negativeerror. Error leaves safe defaults.
 * save:0durable adapter success and revision updated; negativeerror leaves the
 * caller's options unchanged. Expected revision is checked under store lock.
 * Single caller/controller. Native adapter durability is a separate proof. */
int ro_load(const char *,const ap_identity *,ro_options *);
int ro_save(const char *,const ap_identity *,ro_options *,uint64_t expected_revision);
#ifdef __cplusplus
}
#endif
#endif
