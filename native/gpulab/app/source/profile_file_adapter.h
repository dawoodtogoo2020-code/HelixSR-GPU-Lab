#ifndef PROFILE_FILE_ADAPTER_H
#define PROFILE_FILE_ADAPTER_H
#include "profile_store.h"
#include "tiny_store.h"

/* Caller-owned, single-controller context. Start with PFA_INITIALIZER.
 * Keep this object alive while any copied ps_io/cs_service references it.
 * Each context admits only one open, including a failed open; never reset it.
 * After close, old borrowed callbacks fail rather than following a new title.
 * Settings use a versioned canonical digest of the full device/title/build/
 * executable identity. The recovery journal always uses one global file.
 * The root must be pre-provisioned trusted storage. This does not authenticate
 * first provisioning or detect rollback/loss of every recovery anchor.
 * Native-platform filesystem/SDK behavior still needs separate qualification. */
typedef struct {
    ts_store *store;
    ps_io io;
    unsigned lifecycle;
    int faulted;
    ps_result close_result;
} pfa_store;
#define PFA_INITIALIZER {0}

ps_result pfa_open(pfa_store *, const char *root, const ap_identity *);
const ps_io *pfa_get_io(pfa_store *);
ps_result pfa_close(pfa_store *);
#endif
