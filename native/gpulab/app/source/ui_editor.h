#ifndef UI_EDITOR_H
#define UI_EDITOR_H
#include "ui_canvas.h"
#include "ui_screen.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The u07 screens, drawn by us_render from the same model. Presentation only, like ui_screen.c.
 * ue_draw (ui_editor.c): the resolution editor, over the dimmed main screen, with its own footer.
 * uk_draw (ui_frames.c): the frame-rate screen, over the whole card area, with its own footer. */
void ue_draw(uc_canvas *,const us_model *);
void uk_draw(uc_canvas *,const us_model *);
#ifdef __cplusplus
}
#endif
#endif
