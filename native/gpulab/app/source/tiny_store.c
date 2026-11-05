#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#if !defined(_POSIX_C_SOURCE) && !defined(__FreeBSD__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "tiny_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TS_ROOT_MAX 4096u
#define TS_NAME_MAX 255u
#define TS_NS_LEN 64u
#define TS_STAGE_ATTEMPTS 32
#define TS_LOCK_NAME ".tiny-store.lock"
#define TS_RECOVERY_NAME "recovery.bin"
#define TS_STAGE_PREFIX ".tiny-stage."

enum { KEY_SETTINGS = 0, KEY_RECOVERY = 1, KEY_COUNT = 2 };
enum { PH_IDLE = 0, PH_STAGED, PH_SYNCED, PH_COMMITTED };

struct ts_key {
    int phase;
    int fd;          /* open stage fd while PH_STAGED, else -1 */
    int has_file;    /* stage name created by us may still exist */
    size_t len;
    dev_t dev;
    ino_t ino;
    char stage[64];
};

struct ts_store {
    int dirfd;
    int lockfd;
    int poisoned;
    uid_t euid;
    unsigned long counter;
    char settings_name[96];
    struct ts_key keys[KEY_COUNT];
};

static int key_index(const char *key)
{
    if (key == NULL)
        return -1;
    if (strcmp(key, "settings") == 0)
        return KEY_SETTINGS;
    if (strcmp(key, "recovery") == 0)
        return KEY_RECOVERY;
    return -1;
}

static const char *live_name(const ts_store *s, int idx)
{
    return idx == KEY_SETTINGS ? s->settings_name : TS_RECOVERY_NAME;
}

static int safe_file(const struct stat *st, uid_t euid)
{
    return S_ISREG(st->st_mode) && st->st_uid == euid && st->st_nlink <= 1 /* native stat reports zero for link count */ &&
           (st->st_mode & 07777) == 0600;
}

static int valid_namespace(const char *ns)
{
    size_t i;

    if (ns == NULL || strnlen(ns, TS_NS_LEN + 1) != TS_NS_LEN)
        return 0;
    for (i = 0; i < TS_NS_LEN; i++) {
        char c = ns[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
    }
    return 1;
}

/* Walk an existing absolute path component by component without following
 * links. Returns an O_DIRECTORY fd for the final component or -1. */
static void native_trace(const char *stage,int result) { FILE *f=fopen("/data/helixsr-speed/storage-debug.txt","a");if(f){fprintf(f,"%s result=%d errno=%d\n",stage,result,errno);fclose(f);} }
static int open_root(const char *root, uid_t euid)
{
    char buf[TS_ROOT_MAX + 1];
    size_t len;
    char *p;
    int fd;
    struct stat st;

    len = strnlen(root, TS_ROOT_MAX + 1);
    if (len < 2 || len > TS_ROOT_MAX || root[0] != '/' || root[len - 1] == '/')
        return -1;
    memcpy(buf, root, len + 1);

    fd = openat(AT_FDCWD, "/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    p = buf + 1;
    while (p != NULL) {
        char *slash = strchr(p, '/');
        int nfd;

        if (slash != NULL)
            *slash = '\0';
        if (*p == '\0' || strcmp(p, ".") == 0 || strcmp(p, "..") == 0 ||
            strlen(p) > TS_NAME_MAX) {
            (void)close(fd);
            return -1;
        }
        nfd = openat(fd, p, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC |
                                O_NONBLOCK | O_NOCTTY);
        if (close(fd) != 0) {
            if (nfd >= 0)
                (void)close(nfd);
            return -1;
        }
        if (nfd < 0)
            return -1;
        fd = nfd;
        p = slash != NULL ? slash + 1 : NULL;
    }

    if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != euid ||
        (st.st_mode & 07777) != 0700) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

static int open_lock(int dirfd, uid_t euid)
{
    struct stat st;
    int fd;

    fd = openat(dirfd, TS_LOCK_NAME,
                O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY,
                0600);
    if (fd < 0)
        return -1;
    int fs=fstat(fd,&st); {FILE*f=fopen("/data/helixsr-speed/storage-debug.txt","a");if(f){fprintf(f,"lock mode=%o uid=%u gid=%u links=%u size=%lld expecteduid=%u\n",st.st_mode,st.st_uid,st.st_gid,st.st_nlink,(long long)st.st_size,euid);fclose(f);}} native_trace("lock-fstat",fs); native_trace("lock-safe",safe_file(&st,euid));
    int lk=flock(fd,LOCK_EX|LOCK_NB); native_trace("lock-flock",lk);
    if (fs != 0 || !safe_file(&st,euid) || lk != 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

ts_store *ts_open(const char *root, const char *settings_namespace)
{
    ts_store *s;
    int i;

    if (root == NULL || !valid_namespace(settings_namespace))
        return NULL;

    s = calloc(1, sizeof *s);
    if (s == NULL)
        return NULL;
    s->dirfd = -1;
    s->lockfd = -1;
    /* Native titles report uid 0 through fstat/sceKernelStat, even for
     * their own files. Verified on Pro against payload lstat uid 1.
     * Keep all mode, no-follow, inode and lock checks in this native copy. */
    s->euid = 0;
    for (i = 0; i < KEY_COUNT; i++) {
        s->keys[i].phase = PH_IDLE;
        s->keys[i].fd = -1;
    }
    if (snprintf(s->settings_name, sizeof s->settings_name, "settings.%s.bin",
                 settings_namespace) != (int)(9 + TS_NS_LEN + 4))
        goto fail;

    s->dirfd = open_root(root, s->euid); native_trace("root-final",s->dirfd);
    if (s->dirfd < 0)
        goto fail;
    s->lockfd = open_lock(s->dirfd, s->euid); native_trace("lock-final",s->lockfd);
    if (s->lockfd < 0)
        goto fail;
    return s;

fail:
    if (s->lockfd >= 0)
        (void)close(s->lockfd);
    if (s->dirfd >= 0)
        (void)close(s->dirfd);
    free(s);
    return NULL;
}

int ts_close(ts_store *s)
{
    int rc;
    int i;

    if (s == NULL)
        return -1;
    rc = s->poisoned ? -1 : 0;

    for (i = 0; i < KEY_COUNT; i++) {
        struct ts_key *k = &s->keys[i];

        if (k->fd >= 0) {
            if (close(k->fd) != 0)
                rc = -1;
            k->fd = -1;
        }
        if (k->has_file) {
            struct stat st;

            if (fstatat(s->dirfd, k->stage, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (st.st_dev == k->dev && st.st_ino == k->ino && safe_file(&st, s->euid)) {
                    if (unlinkat(s->dirfd, k->stage, 0) != 0)
                        rc = -1;
                } else {
                    rc = -1;
                }
            } else if (errno != ENOENT) {
                rc = -1;
            }
            k->has_file = 0;
        }
        k->phase = PH_IDLE;
    }

    if (s->lockfd >= 0 && close(s->lockfd) != 0)
        rc = -1;
    if (s->dirfd >= 0 && close(s->dirfd) != 0)
        rc = -1;
    free(s);
    return rc;
}

int ts_read(void *h, const char *key, unsigned char *buf, size_t cap, size_t *out)
{
    ts_store *s = h;
    unsigned char tmp[TS_MAX_RECORD + 1];
    struct stat st, st2, st3;
    const char *name;
    size_t total = 0;
    int idx;
    int fd;

    if (out != NULL)
        *out = 0;
    idx = key_index(key);
    if (s == NULL || out == NULL || idx < 0 || cap > TS_MAX_RECORD ||
        (buf == NULL && cap != 0))
        return -1;
    name = live_name(s, idx);

    fd = openat(s->dirfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        if (errno == ENOENT)
            return 0;
        s->poisoned = 1;
        return -1;
    }
    if (fstat(fd, &st) != 0 || !safe_file(&st, s->euid) || st.st_size < 0 ||
        st.st_size > (off_t)TS_MAX_RECORD)
        goto bad;
    if ((size_t)st.st_size > cap) {
        /* Caller capacity too small: an error, never truncation. */
        if (close(fd) != 0)
            s->poisoned = 1;
        return -1;
    }

    for (;;) {
        ssize_t r = read(fd, tmp + total, sizeof tmp - total);
        if (r < 0)
            goto bad;
        if (r == 0)
            break;
        total += (size_t)r;
        if (total == sizeof tmp)
            break;
    }
    if (total != (size_t)st.st_size)
        goto bad;

    if (fstat(fd, &st2) != 0 || !safe_file(&st2, s->euid) || st2.st_dev != st.st_dev ||
        st2.st_ino != st.st_ino || st2.st_size != st.st_size)
        goto bad;
    if (fstatat(s->dirfd, name, &st3, AT_SYMLINK_NOFOLLOW) != 0 ||
        st3.st_dev != st.st_dev || st3.st_ino != st.st_ino)
        goto bad;

    if (close(fd) != 0) {
        s->poisoned = 1;
        return -1;
    }
    if (total != 0)
        memcpy(buf, tmp, total);
    *out = total;
    return 1;

bad:
    (void)close(fd);
    s->poisoned = 1;
    return -1;
}

size_t ts_stage(void *h, const char *key, const unsigned char *data, size_t n)
{
    ts_store *s = h;
    struct ts_key *k;
    struct stat st;
    ssize_t w;
    int idx;
    int fd = -1;
    int attempt;

    idx = key_index(key);
    if (s == NULL || idx < 0 || data == NULL || n == 0 || n > TS_MAX_RECORD)
        return 0;
    if (s->poisoned)
        return 0;
    k = &s->keys[idx];
    if (k->phase != PH_IDLE || k->has_file || k->fd >= 0) {
        s->poisoned = 1;
        return 0;
    }

    for (attempt = 0; attempt < TS_STAGE_ATTEMPTS; attempt++) {
        int len = snprintf(k->stage, sizeof k->stage, TS_STAGE_PREFIX "%ld.%lu",
                           (long)getpid(), s->counter++);
        if (len < 0 || (size_t)len >= sizeof k->stage) {
            s->poisoned = 1;
            return 0;
        }
        fd = openat(s->dirfd, k->stage,
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK |
                        O_NOCTTY,
                    0600);
        if (fd >= 0 || errno != EEXIST)
            break;
    }
    if (fd < 0) {
        s->poisoned = 1;
        return 0;
    }

    k->fd = fd;
    if (fstat(fd, &st) != 0) {
        /* Identity unknown: never unlink this name later. */
        s->poisoned = 1;
        return 0;
    }
    k->dev = st.st_dev;
    k->ino = st.st_ino;
    k->has_file = 1;
    if (!safe_file(&st, s->euid) || st.st_size != 0) {
        s->poisoned = 1;
        return 0;
    }

    w = write(fd, data, n);
    if (w < 0) {
        s->poisoned = 1;
        return 0;
    }
    if ((size_t)w != n) {
        s->poisoned = 1;
        return (size_t)w;
    }
    k->len = n;
    k->phase = PH_STAGED;
    return n;
}

int ts_sync_stage(void *h, const char *key)
{
    ts_store *s = h;
    struct ts_key *k;
    int idx;
    int fd;

    idx = key_index(key);
    if (s == NULL || idx < 0 || s->poisoned)
        return -1;
    k = &s->keys[idx];
    if (k->phase != PH_STAGED || k->fd < 0 || !k->has_file) {
        s->poisoned = 1;
        return -1;
    }

    if (fsync(k->fd) != 0) {
        s->poisoned = 1;
        return -1;
    }
    fd = k->fd;
    k->fd = -1; /* never retry close, whatever the result */
    if (close(fd) != 0) {
        s->poisoned = 1;
        return -1;
    }
    k->phase = PH_SYNCED;
    return 0;
}

int ts_commit(void *h, const char *key)
{
    ts_store *s = h;
    struct ts_key *k;
    struct stat st;
    const char *name;
    int idx;

    idx = key_index(key);
    if (s == NULL || idx < 0 || s->poisoned)
        return -1;
    k = &s->keys[idx];
    if (k->phase != PH_SYNCED || k->fd >= 0 || !k->has_file) {
        s->poisoned = 1;
        return -1;
    }
    name = live_name(s, idx);

    if (fstatat(s->dirfd, k->stage, &st, AT_SYMLINK_NOFOLLOW) != 0 ||
        st.st_dev != k->dev || st.st_ino != k->ino || !safe_file(&st, s->euid) ||
        st.st_size != (off_t)k->len) {
        s->poisoned = 1;
        return -1;
    }
    if (fstatat(s->dirfd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!safe_file(&st, s->euid)) {
            s->poisoned = 1;
            return -1;
        }
    } else if (errno != ENOENT) {
        s->poisoned = 1;
        return -1;
    }

    if (renameat(s->dirfd, k->stage, s->dirfd, name) != 0) {
        /* Outcome unknown; close-time cleanup rechecks identity. */
        s->poisoned = 1;
        return -1;
    }
    k->has_file = 0;
    k->phase = PH_COMMITTED;
    return 0;
}

int ts_sync_directory(void *h, const char *key)
{
    ts_store *s = h;
    struct ts_key *k;
    int idx;

    idx = key_index(key);
    if (s == NULL || idx < 0 || s->poisoned)
        return -1;
    k = &s->keys[idx];
    if (k->phase != PH_COMMITTED) {
        s->poisoned = 1;
        return -1;
    }

    if (fsync(s->dirfd) != 0) {
        s->poisoned = 1;
        return -1;
    }
    k->phase = PH_IDLE;
    k->len = 0;
    k->stage[0] = '\0';
    return 0;
}


