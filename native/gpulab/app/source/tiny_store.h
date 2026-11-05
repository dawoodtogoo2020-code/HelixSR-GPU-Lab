#ifndef TINY_STORE_H
#define TINY_STORE_H
#include <stddef.h>
#define TS_MAX_RECORD 320u
typedef struct ts_store ts_store;
/* Existing absolute private0700 root;64lowercasehex settings namespace.
 * Logical keys: settings/recovery. Global recovery filename never varies.
 * Single caller; second instance must fail while root lock is held.
 * All disk IO remains untrusted to the consuming codec/activation policy. */
ts_store *ts_open(const char *root, const char *settings_namespace);
/* Best effort bounded cleanup; any checked close/unlink error returns -1.
 * Frees the instance, which must never be used again. No live record deletion. */
int ts_close(ts_store *);
/* read:0missing,1present,-1error; size0 on nonpresent/error; no truncation. */
int ts_read(void *, const char *, unsigned char *, size_t, size_t *);
/* stage:actual write count; accepts1..320bytes. No live record mutation. */
size_t ts_stage(void *, const char *, const unsigned char *, size_t);
/* Exact sequence per key:stage,sync_stage,commit,sync_directory.
 * File fsync+checked close, same-directory rename, then directory fsync.
 * Any operation error leaves durability unknown and prevents later commits. */
int ts_sync_stage(void *, const char *);
int ts_commit(void *, const char *);
int ts_sync_directory(void *, const char *);
#endif
