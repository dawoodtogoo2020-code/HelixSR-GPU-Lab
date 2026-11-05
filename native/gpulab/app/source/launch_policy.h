/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GP_LAUNCH_POLICY_H
#define GP_LAUNCH_POLICY_H
#include "game_library.h"
#ifdef __cplusplus
extern "C" {
#endif
int gp_inventory_match(const gl_game *,unsigned count,const char *title,const char *version);
int gp_accept_catalog(const char *model,const char *row_model,const char *title,const char *version,int package_record);
/* -1 refuses; 0 launches an unmanaged game or a verified SOTC hook; H/O applies an exact folder package. */
int gp_choose_launch(const char *model,const char *title,const char *version,int catalog,int ready,int hook,int on);
#ifdef __cplusplus
}
#endif
#endif
