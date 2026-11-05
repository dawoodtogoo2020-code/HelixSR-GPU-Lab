#ifndef UI_SCREEN_H
#define UI_SCREEN_H
#include <stdint.h>
#include "frame_log.h"
#include "resolution_plan.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The GPU Lab screen, drawn at 1920x1080 from a plain snapshot of
 * native_main's state. Presentation only: nothing here saves, launches or
 * decides availability. Strings are bounded printable ASCII. */
#define US_WIDTH 1920
#define US_HEIGHT 1080
#define US_MAX_GAMES 64
/* Two menus: the game library (Up/Down picks the game) and the selected
 * game's HelixSR settings (Up/Down picks the row). Left/Right switches menu. */
enum { US_MENU_LIBRARY=0, US_MENU_SETTINGS, US_MENU_COUNT };
/* Settings rows, identical to native_main's row order: the HelixSR ON/OFF switch, the in-game HUD switch (the Pro
 * session's u06 row), then (u07) the per-game resolution request and the frame-rate screen, which Cross opens. The
 * package rows between them are facts about the package on the console, not switches. */
enum { US_HELIX=0, US_HUD, US_RESOLUTION, US_FRAMES, US_FIELD_COUNT };
/* The main screen, the resolution editor drawn over it, or the frame-rate screen. */
enum { US_SCREEN_MAIN=0, US_SCREEN_EDITOR, US_SCREEN_FRAMES };
enum { US_TONE_NONE=0, US_TONE_GOOD, US_TONE_WARN, US_TONE_BAD, US_TONE_NEUTRAL };
/* Resolution editor rows. */
enum { UE_START=0, UE_OUTPUT, UE_OUTPUT_WIDTH, UE_OUTPUT_HEIGHT, UE_SCALE, UE_INPUT_WIDTH, UE_INPUT_HEIGHT, UE_ROW_COUNT };
/* What a pair has on the HelixSR side: nothing, a trial package, a built package not on this console, a package on
 * this console (its READY file lists the pair). */
enum { UE_PACKAGE_NONE=0, UE_PACKAGE_TRIAL, UE_PACKAGE_BUILT, UE_PACKAGE_READY };
typedef struct {
    unsigned row;
    rp_pair pair;                 /* being edited */
    rp_check check;
    char start[72],output_name[16];
    int package,game_measured;    /* UE_PACKAGE_*; the game was measured rendering at this pair */
    char package_text[128],game_text[128],launch_text[200],saved_text[80];
    int saved;                    /* the edited pair equals the saved request */
    int refused;                  /* saved_text says why the last Cross did not save */
} us_editor;
/* The frame-rate screen: one log, summarised by native_main when it is opened (the arrays are already reduced to
 * what is drawn). state: 0 no log for this game, 1 a log, 2 the log could not be read. */
#define US_FPS_POINTS 1200u
#define US_TIME_COLUMNS 1160u
typedef struct {
    int state,record_on,recorder_available;
    unsigned log_index,log_count;
    char game[80],started[40],source[48],session[200],basis[3][320],status[200],error[96];
    int engine_source,frame_time,missed_measured,mode_changes,incomplete;
    fl_stats stats;
    unsigned fps_count,seconds_per_point;
    uint16_t fps[US_FPS_POINTS];                              /* tenths of FPS per point */
    uint16_t shortest[US_TIME_COLUMNS],longest[US_TIME_COLUMNS]; /* 10 us units, 0 = empty column */
    unsigned long long play_us;
} us_frames;
/* kind: US_KNOWN has a finding (upscaler names it), US_UNSCANNED is catalogued
 * without one, US_INSTALLED is an installed game not yet catalogued. */
enum { US_KNOWN=0, US_UNSCANNED, US_INSTALLED };
typedef struct { char name[80], title[64], build[64], upscaler[24]; int kind, ready; } us_game;
/* Zero the whole model before filling it: render skips a frame whose bytes
 * equal the last drawn model. game_count counts every library entry. */
typedef struct us_model {
    unsigned game_count,selected,menu,focus;
    us_game games[2*US_MAX_GAMES];
    int pro,controller,settings_ok,unsaved,helix_on,reset_armed;
    /* The HelixSR HUD (FPS box) of the selected game's hook: available when its hook has one, on when it is shown. */
    int hud_available,hud_on;
    /* The HelixSR package on the console for the selected game: its build
     * line and the resolution it renders at and outputs, 0 when unknown. */
    unsigned output_width,output_height,input_width,input_height;
    unsigned output2_width,output2_height,input2_width,input2_height;   /* u07: a second layout (r18), 0 when none */
    char package[96],status[81],detail[81],feedback[81];
    /* The selected game's finding and hook; blank hides values. */
    int blank,scan_launch,scan_only,hook_in_development,hook_ready;
    char engine[24],basis[24],route[64],route_basis[96],hook[24];
    /* u07: which screen, the two new settings rows' values, and the editor or frame-rate screen when shown. */
    unsigned screen;
    char resolution_value[96],frames_value[96];
    int resolution_tone,frames_tone;          /* US_TONE_* of the row's dot */
    us_editor editor;
    us_frames frames;
} us_model;
typedef struct us_screen {
    uint32_t *pixels,*background;
    us_model drawn;
    int valid;
} us_screen;
/* pixels and background each hold US_WIDTH*US_HEIGHT words, caller owned. */
void us_init(us_screen *,uint32_t *pixels,uint32_t *background);
/* Returns 1 when pixels were redrawn, 0 when the model was unchanged. */
int us_render(us_screen *,const us_model *);
struct uc_canvas;
/* The HelixSR mark: a size x size gradient tile with its top-left at x, y. */
void us_draw_mark(struct uc_canvas *,float x,float y,float size);
#ifdef __cplusplus
}
#endif
#endif
