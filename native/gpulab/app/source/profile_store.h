#ifndef PROFILE_STORE_H
#define PROFILE_STORE_H
#include "activation_policy.h"
#include <stddef.h>
#define PS_MAX_RECORD 320u
/* SINGLE CONTROLLER, no concurrent writers. Exact platform adapter boundary:
 * read: 0 missing, 1 present, -1 error; reject oversize rather than truncate.
 * stage: create private regular temporary file in the destination directory,
 * write exactly len bytes (return actual count); no live file modification.
 * sync_stage: fsync the staged file and check close errors.
 * commit: atomic rename over destination in the SAME directory/filesystem.
 * sync_directory: fsync that directory before reporting durable success.
 * Adapter must use fixed caller-owned roots, reject links, bound reads, enforce
 * ownership/access, and serialize calls. Each key has independent staging.
 * Any error means durability UNKNOWN, including rename/dir-fsync failures.
 * These callbacks are a required platform boundary, NOT physical durability
 * proof. No PS5 file adapter or real action executor is supplied here. */
typedef struct {
 void *context;
 int (*read)(void *, const char *, unsigned char *, size_t, size_t *);
 size_t (*stage)(void *, const char *, const unsigned char *, size_t);
 int (*sync_stage)(void *, const char *);
 int (*commit)(void *, const char *);
 int (*sync_directory)(void *, const char *);
} ps_io;
typedef enum { PS_OK=0, PS_DEFAULT, PS_INVALID, PS_CONFLICT, PS_IO_ERROR, PS_CORRUPT } ps_result;
typedef struct { ap_identity identity; uint64_t revision; ap_desired desired; unsigned quality; } ps_profile;
int ps_io_valid(const ps_io *io);
int ps_identity_equal(const ap_identity *a,const ap_identity *b);
void ps_encode_identity(unsigned char *out,const ap_identity *id);
int ps_decode_identity(ap_identity *id,const unsigned char *in);
uint64_t ps_get64(const unsigned char *p);
void ps_put64(unsigned char *p,uint64_t n);
void ps_seal(unsigned char *p,size_t n);
int ps_sealed(const unsigned char *p,size_t n);
ps_result ps_atomic_write(const ps_io *io,const char *key,const unsigned char *data,size_t n);
/* Checksums detect accidental corruption only, never authenticate hostile data.
 * One fixed slot. Different exact identity reads OFF/revision0 and can replace
 * only with expected revision0. Caller can map separate IO namespaces per title.
 * Missing/corrupt/new identity always returns an OFF default, never old ON. */
ps_result ps_load(const ps_io *,const ap_identity *,ps_profile *);
ps_result ps_save(const ps_io *,const ps_profile *,uint64_t expected_revision,ps_profile *saved);
#endif
