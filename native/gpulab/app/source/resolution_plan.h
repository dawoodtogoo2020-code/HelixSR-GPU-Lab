#ifndef RESOLUTION_PLAN_H
#define RESOLUTION_PLAN_H
#ifdef __cplusplus
extern "C" {
#endif
/* What a HelixSR render -> output pair needs, from the sizes alone (no file access). The rules mirror the HelixSR 1.8.1
 * model planner (setup/lib/model/model.cpp: finalize, network_for, the output-kernel choice) and the PS5 port's helpers
 * (n01-native-r53/motion/build_motion.py, the r17 encoder tiles). They say what a package for the pair would need, never
 * that one exists: packages are listed by the console's READY file and the known-pair table below. */
#define RP_MAX_OUTPUT_WIDTH 3840u
#define RP_MAX_OUTPUT_HEIGHT 2160u
#define RP_MIN_OUTPUT_WIDTH 1280u
#define RP_MIN_OUTPUT_HEIGHT 720u
#define RP_MIN_INPUT_WIDTH 640u
#define RP_MIN_INPUT_HEIGHT 360u
#define RP_ASPECT_PERMILLE 10u
/* Output kernel by the smaller of the two scales: >= 1.7 fastpath (the only one ported to PS5, k5f), >= 1.49 dfn_mid,
 * below that dfn_3x3_256. */
enum { RP_KERNEL_FAST=0, RP_KERNEL_MID, RP_KERNEL_3X3 };
typedef struct { unsigned input_width,input_height,output_width,output_height; } rp_pair;
typedef struct {
    int valid;                              /* inside the limits below, so it can be saved */
    int limits_ok;                          /* the size limits hold (the shape may still differ) */
    char problem[96];                       /* the first limit it breaks, when !valid */
    unsigned scale_x_milli,scale_y_milli;   /* output / render, x1000, rounded */
    unsigned pixels_permille;               /* rendered share of the output's pixels */
    unsigned aspect_permille;               /* how far the render shape is from the output shape */
    unsigned network_width,network_height;  /* the output padded to multiples of 128: the model's working size */
    int ultra_network;                      /* output / render width > 2.5: the Ultra Performance (NHWC) network */
    int kernel;                             /* RP_KERNEL_* */
    int motion_ok;                          /* 1 < scale < 2 on both axes and the motion helper's reciprocal is exact */
    int tiles_exact;                        /* network sizes are multiples of 512: the r17 encoders need no edge guards */
    int ps5_kernels;                        /* NCHW8 network, fastpath kernel and motion helper all exist on PS5 */
} rp_check;
void rp_check_pair(const rp_pair *,rp_check *);
int rp_pair_equal(const rp_pair *,const rp_pair *);
/* Every "WxH -> WxH" in text (a READY description line), in order; returns how many were stored. Pairs whose render
 * size exceeds the output, or with a size over 7680x4320, are skipped (the same rule as the package card). */
unsigned rp_parse_pairs(const char *text,rp_pair *out,unsigned capacity);

/* The pairs a package runs at when its READY line names none, by package id; returns how many were stored. */
unsigned rp_package_pairs(const char *package_id,rp_pair *out,unsigned capacity);
/* Pairs known for one game version: the modes the game was measured rendering at and the HelixSR packages built for
 * them. A package listed here is not necessarily on the console; READY says that. */
enum { RP_PACKAGE_NONE=0, RP_PACKAGE_TRIAL, RP_PACKAGE_BUILT };
typedef struct {
    rp_pair pair;
    const char *mode;          /* the game's name for the mode, e.g. "Performance" */
    int package;               /* RP_PACKAGE_* */
    const char *package_note;  /* what was built and how far it was checked */
    const char *evidence;      /* how the game's render size was measured */
} rp_known_pair;
/* The index-th known pair of this title and version, or NULL past the last one. */
const rp_known_pair *rp_known(const char *title,const char *version,unsigned index);
#ifdef __cplusplus
}
#endif
#endif
