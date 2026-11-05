#ifndef CONTROL_SERVICE_H
#define CONTROL_SERVICE_H
#include "recovery_journal.h"
typedef struct { ap_status policy; int durable_block, recovery_required, backend_available; uint64_t incarnation,recovery_incarnation,recovery_token; ap_identity recovery_identity; } cs_status;
typedef struct {ap_policy policy;ps_io io;rj_record journal;int durable_block,initialized;} cs_service;
#define CS_INITIALIZER {0}
/* First init only on fresh zero storage. Do not reinit/copy a live controller.
 * Profile desired state never automatically becomes a runtime ON request.
 * No executor/endpoint exists; production cannot qualify or attach anything. */
ps_result cs_init(cs_service *,const ps_io *,const ap_identity *,uint64_t now);
ap_refusal cs_request(cs_service *,ap_desired,uint64_t now);
ap_refusal cs_tick(cs_service *,uint64_t now);
int cs_query(const cs_service *,cs_status *);
/* Trusted future executor boundary, NEVER connect these to untrusted UI ACKs.
 * Successful restore means independently confirmed original pass + completion.
 * ACK results are assertions by caller, not physical proof supplied here. */
ap_refusal cs_ack_attach(cs_service *,const ap_identity *,uint64_t incarnation,uint64_t token,ap_ack,uint64_t now);
ps_result cs_ack_restore(cs_service *,const ap_identity *,uint64_t incarnation,uint64_t token,ap_ack);
#ifdef HELIX_CONTROL_TEST
/* Not declared or linked into production. Synthetic CPU fixtures only. */
ap_refusal cs_test_qualify(cs_service *,const ap_evidence *,uint64_t now);
#endif
#endif
