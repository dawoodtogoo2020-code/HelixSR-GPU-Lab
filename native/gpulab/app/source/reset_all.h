#ifndef HELIX_RESET_ALL_H
#define HELIX_RESET_ALL_H
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit user action only. Caller closes every active AS/frontend store lock
 * first, and reinitializes/reloads afterward, including after partial failure.
 * Existing private absolute roots must end /settings and /render-settings.
 * Enumerates strict identity record filenames only, including catalog orphans.
 * Never accesses recovery data, game files or saves, and never deletes records.
 * At most256 canonical identities and4096 directory entries per root.
 * reset counts identities whose OFF and Game default/Native saves both succeed.
 * failed counts malformed/unreadable records, enumeration failures and failed
 * identity resets (a partial identity can contribute more than one issue).
 * Return0 only if every operation succeeded; otherwise-1 with partial counts.
 * Storage adapter success is not physical durability proof. Single controller. */
int helix_reset_all(const char *settings_root,const char *render_root,
                    unsigned *reset,unsigned *failed);
#ifdef __cplusplus
}
#endif
#endif
