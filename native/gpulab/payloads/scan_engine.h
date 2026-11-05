#ifndef SCAN_ENGINE_H
#define SCAN_ENGINE_H
#include <stddef.h>
#include <stdint.h>
/* Streaming upscaler fingerprint of a game executable: SHA-256 plus counts of
 * known upscaler and engine strings, each searched as ASCII and as UTF-16LE.
 * Read-only evidence; a string's presence shows the code is linked, not that
 * the game uses it at runtime. Portable C for the payload and host tests. */
enum se_tech { SE_FSR1=0, SE_FSR2, SE_FSR3, SE_FSR4, SE_PSSR, SE_TSR, SE_TAAU, SE_XESS, SE_DLSS, SE_CHECKERBOARD,
               SE_UNREAL, SE_NORTHLIGHT, SE_UNITY, SE_FROSTBITE, SE_TECH_COUNT };
typedef struct se_state {
    uint32_t h[8];
    uint64_t bytes;
    unsigned char block[64];
    size_t block_used;
    unsigned char tail[128];
    size_t tail_used;
    uint32_t hits[SE_TECH_COUNT];
} se_state;
void se_init(se_state *);
/* Any chunk sizes; matches spanning chunk boundaries are found. */
void se_update(se_state *,const unsigned char *data,size_t bytes);
void se_finish(se_state *,char sha256_hex[65]);
const char *se_tech_name(unsigned tech);
/* The upscaler a regular PS5 build most likely runs, or "Unknown". */
const char *se_primary(const se_state *);
const char *se_engine(const se_state *);
#endif
