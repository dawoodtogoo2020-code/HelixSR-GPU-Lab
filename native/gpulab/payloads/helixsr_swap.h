/* HelixSR on/off for an installed game, done by moving files, never by copying them.
 *
 * A package is a fixed set of game files (path relative to the game folder) with two known versions each: the game's
 * original and the HelixSR one. Every instance is identified by SHA-256; an executable may read back in an installed
 * form that differs from its uploaded SELF, so each version lists up to three accepted hashes. Each instance exists
 * exactly once, in one of four places:
 *     G  game folder / rel                 (what the game runs)
 *     N  game folder / rel + ".hlx-new"    (only between two renames)
 *     PO package / original / base         (the original while HelixSR is on)
 *     PH package / helix / base            (the HelixSR file while it is off)
 * hs_apply() moves the target version to G and the other to its package slot with rename() (same filesystem, atomic per
 * step). It keeps no journal: any interrupted state is recognised from where each instance is and finished. It changes
 * nothing when any instance is unrecognised, missing, duplicated or misplaced, and checks every file before moving any. */
#ifndef HELIXSR_SWAP_H
#define HELIXSR_SWAP_H
#include <stdio.h>

typedef struct {
    const char *rel,*base;
    const char *original[3],*helix[3];   /* accepted SHA-256; unused entries are NULL */
} hs_file;
/* folder=1: the HelixSR build is the game's whole backport folder and "off" means no backport folder at all, so the game
 * runs only its installed files. On: game folder = the build. Off: the build sits at package/stored/game. Only the
 * files' HelixSR hashes are used (to recognise the build); one rename of the folder switches.
 * folder=2: listed overlay files move independently; validated dependencies stay in the game root. */
typedef struct {
    const char *title,*version,*package,*description;
    unsigned count;
    const hs_file *files;
    int folder; /* 0: paired files, 1: whole folder, 2: overlay files */
} hs_package;
enum hs_target { HS_ORIGINAL=0, HS_HELIX=1 };
typedef struct {
    int ok;                  /* 1: the target layout holds now */
    unsigned moves;          /* renames performed */
    char state[24];          /* original | helix | unrecognized | package_missing | package_damaged | mixed */
    char reason[200];        /* short plain-English detail for the GPU Lab screen and the log */
} hs_result;

/* Called before every rename when set; nonzero refuses that move (the switch then fails and is rolled back or finished
 * by the next run). The launch payload sets it to "no game process is running". */
extern int (*hs_gate)(void);
/* The compiled-in package for this title and version, or NULL. */
const hs_package *hs_find(const char *title,const char *version);
/* Brings the game to the target. cache_path (may be NULL) keeps SHA-256 per (device, inode, size, mtime) so an unchanged
 * file is hashed once; a rename keeps all four, so moved files stay cached. log may be NULL. */
int hs_apply(const hs_package *,const char *game_root,const char *package_root,enum hs_target,
             const char *cache_path,hs_result *,FILE *log);
/* Read-only: which version the game folder holds (state only; nothing moves). */
int hs_status(const hs_package *,const char *game_root,const char *package_root,const char *cache_path,hs_result *,FILE *log);
#endif
