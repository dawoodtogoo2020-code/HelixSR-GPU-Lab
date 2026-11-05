#ifndef RECOVERY_JOURNAL_H
#define RECOVERY_JOURNAL_H
#include "profile_store.h"
typedef struct { uint64_t incarnation, last_token, attach_incarnation, token; int unresolved; ap_identity identity; } rj_record;
/* Global IO namespace MUST NOT change with selected title/profile. Never erase
 * journal on OFF, profile reset, title change or restart. Corruption blocks ON.
 * Missing ledger is trusted only for genuinely first provisioned storage; loss
 * or rollback of an existing ledger is outside this API's evidence. */
ps_result rj_boot(const ps_io *,rj_record *);
ps_result rj_intent(const ps_io *,rj_record *,const ap_identity *,uint64_t *token);
ps_result rj_restored(const ps_io *,rj_record *,const ap_identity *,uint64_t incarnation,uint64_t token,ap_ack);
#endif
