#ifndef FRAME_LOG_H
#define FRAME_LOG_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Frame-time logs for the GPU Lab frame-rate screen (HFPS1 text, one file per session). The writers are the HelixSR
 * frame logger payload (the game's own frame counter, sampled while it runs) and the PC converter for HelixSR timer-strip
 * recordings. This only reads and summarises; every number keeps the basis its writer declared.
 *
 *   HFPS1
 *   key=value lines: title version name source(engine|gpu-strip) source_detail started refresh_mhz
 *                    frame_time(measured|calibrated|nominal) missed(measured|estimated) helix(on|off|unknown)
 *                    package render output wall_ms counted_frames recorder_drops end
 *   frames
 *   FRAME_US [REPEATS [HELIX_US [FLAGS]]]      one line per game frame, in order
 *   end
 *
 * FRAME_US is the frame's duration in microseconds (0: unknown). REPEATS counts the extra refreshes the frame stayed on
 * screen, from a recording (only meaningful when missed=measured). HELIX_US is the HelixSR block's GPU time inside that
 * frame (0: none): part of the frame, never its frame time. FLAGS: FL_FLAG_*. */
#define FL_SLOTS 8u
#define FL_MAX_FRAMES 262144u
#define FL_BINS 60u
enum { FL_SOURCE_UNKNOWN=0, FL_SOURCE_ENGINE, FL_SOURCE_GPU_STRIP, FL_SOURCE_RENDER_TRACE };
enum { FL_TIME_MEASURED=0, FL_TIME_CALIBRATED, FL_TIME_NOMINAL, FL_TIME_CPU_CALIBRATED };
/* ESTIMATED: the frame time was split from a longer interval (the sampler missed a counter step). MODE: the game's
 * render or output size changed at this frame. PAUSE: longer than 250 ms (loading, menus); left out of the statistics. */
enum { FL_FLAG_ESTIMATED=1, FL_FLAG_MODE=2, FL_FLAG_RESET=4, FL_FLAG_PAUSE=8 };
typedef struct {
    char title[16],version[32],name[80],source_detail[128],started[40],helix[12],package[48],end[48];
    unsigned source,frame_time,missed_measured,refresh_mhz,recorder_drops;
    unsigned render_width,render_height,output_width,output_height;
    unsigned long long wall_ms,counted_frames;
    int complete;                   /* the closing "end" line was read */
    unsigned count,truncated;       /* frames stored; frames past capacity */
    /* Caller-owned arrays of capacity entries, or all NULL with capacity 0 to read the header only. */
    uint32_t *frame_us,*helix_us;
    uint16_t *repeats;
    uint8_t *flags;
    unsigned capacity;
} fl_log;
/* 0 read, -1 missing, -2 not an HFPS1 log. Unknown keys are ignored; a damaged frame line ends the frames. */
int fl_read(const char *path,fl_log *);
/* root/fps/TITLE-S.txt, S = 1..FL_SLOTS. */
int fl_slot_path(char *out,unsigned size,const char *root,const char *title,unsigned slot);
/* The frame logger payload knows this version's frame counter (its table in payloads/helixsr_framelog.c). */
int fl_recorder_known(const char *title,const char *version);

typedef struct {
    unsigned frames,pauses,estimated,unknown;   /* frames in the statistics; left out as pauses; split; no time */
    unsigned mode_changes;                      /* frames flagged FL_FLAG_MODE (the game changed render or output size) */
    unsigned long long total_us;
    unsigned refresh_us,target_refreshes;       /* the display period; refreshes per frame the game aims at (median) */
    unsigned average_fps_milli;                 /* frames / their total time */
    unsigned low1_fps_milli,low01_fps_milli;    /* average over the slowest 1% / 0.1% of frames; 0 below 100 / 1000 frames */
    unsigned min_us,median_us,p95_us,p99_us,p999_us,max_us;
    unsigned missed;                            /* refreshes a frame stayed beyond the target cadence */
    int missed_measured;                        /* 1: counted in a recording; 0: estimated from frame times */
    unsigned displayed_fps_milli;               /* measured repeats only: frames / time on screen */
    unsigned helix_frames,helix_median_us,helix_p99_us;
    unsigned bin_us,histogram[FL_BINS],overflow;
} fl_stats;
/* scratch holds log->count entries. Returns 0, or -1 when no frame has a known time. */
int fl_compute(const fl_log *,fl_stats *,uint32_t *scratch);
/* The graphs run on play time: the summed times of the frames in the statistics, so a pause takes no width.
 * fl_fps_series: frames ending in each whole second of play time, in tenths of FPS; returns the number of seconds
 * written (at most capacity; the last partial second is left out). */
unsigned fl_fps_series(const fl_log *,uint16_t *fps_tenths,unsigned capacity);
/* Play time split into columns equal parts; shortest and longest frame time ending in each, in 10 us units (capped at
 * 655.35 ms), 0 for a column no frame ends in. */
void fl_columns(const fl_log *,unsigned columns,uint16_t *shortest,uint16_t *longest);
#ifdef __cplusplus
}
#endif
#endif
