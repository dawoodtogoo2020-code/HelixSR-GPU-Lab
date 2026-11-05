#ifndef GAME_LIBRARY_H
#define GAME_LIBRARY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Installed games and upscaler findings for the GPU Lab library. The inventory
 * and launch payloads write the files; this only reads them. A finding is
 * evidence of what a game links or measured, not proof of what HelixSR does. */
#define GL_MAX_GAMES 64
typedef struct { char title[16],version[32],name[80]; } gl_game;
typedef struct {
    int present;
    char version[32],primary[24],engine[24],sha256[65];
    const char *basis;                 /* "Executable scan", "Runtime probe" or "Game profile" */
    int measured;                      /* 1: the sizes, motion and hook below come from an in-game measurement */
    int motion;                        /* 1: the game was seen producing motion vectors */
    unsigned output_width,output_height,input_width,input_height;
    char hook[16];                     /* "development" or "" (from a game profile) */
    char measured_basis[96];
} gl_finding;
typedef struct {
    int replace;                        /* 1: HelixSR replaces the game's upscaler */
    int scan_only;                      /* unmanaged title with only executable-scan evidence */
    int measured;                       /* 1: the dimensions below were measured in-game */
    unsigned output_width,output_height,input_width,input_height;
    char route[64],basis[96];
} gl_route;

/* installed.tsv: title, version, name per line. Returns the number read. */
unsigned gl_load_installed(const char *path,gl_game *out,unsigned capacity);
/* What is known about this exact version, most reliable first: a game profile (profiles/TITLE.txt under root, written
 * by the PC tools after an in-game measurement, so a new game needs no GPU Lab rebuild), a built-in in-game measurement,
 * then the executable scan (scan/TITLE.txt). A string scan only shows what is linked, so a measurement wins over it.
 * Returns 1 when out->present. */
int gl_find(const char *root,const char *title,const char *version,gl_finding *out);
void gl_recommend(const char *title,const char *version,const gl_finding *,gl_route *out);
/* A HelixSR package for this title and version is on the console: packages/TITLE-VERSION/READY under root, written
 * last by the PC package tool after it verified every file it uploaded ("HPKG1 ID", then a one-line description that
 * is copied to description when it is not NULL). */
int gl_package_ready(const char *root,const char *title,const char *version,char *description,size_t size);
/* The package id on READY's first line ("HPKG1 ID"), e.g. control-r18-20261029. Returns 1 when READY is valid. */
int gl_package_id(const char *root,const char *title,const char *version,char *id,size_t size);
/* The launch payload's last HelixSR switch for this title and version (install/TITLE.txt under root): state is
 * helix, original, unrecognized, package_missing, package_damaged or mixed. Returns 1 when present. */
int gl_install_state(const char *root,const char *title,const char *version,char state[24],char reason[200],int *ok);
/* Whether HelixSR can run in this game: "Ready" when its package is on the console (ready, from gl_package_ready),
 * else "In development" (built-in list or the game profile's hook) or "Not available yet". No file access. */
const char *gl_hook_status(const char *title,const gl_finding *finding,int ready,int *in_development);
#ifdef __cplusplus
}
#endif
#endif
