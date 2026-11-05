#ifndef PROFILE_NAMESPACE_H
#define PROFILE_NAMESPACE_H
#include "activation_policy.h"
/* SHA256("HelixSR-settings-v1" || canonical257byte identity). This is a
 * filename namespace, not authentication, a recovery anchor or permission. */
int pfa_settings_namespace(const ap_identity *,char out[65]);
#endif
