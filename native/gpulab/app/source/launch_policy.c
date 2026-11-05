/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "launch_policy.h"
#include "../../common/package_support.h"
#include <string.h>
int gp_inventory_match(const gl_game *games,unsigned count,const char *title,const char *version) {
    for(unsigned i=0;i<count;i++)if(!strcmp(games[i].title,title)&&!strcmp(games[i].version,version))return 1;
    return 0;
}
int gp_accept_catalog(const char *model,const char *row_model,const char *title,const char *version,int package_record) {
    if(!model||!row_model||strcmp(model,row_model))return 0;
    if(gp_managed_title(title))return package_record&&gp_supported(model,title,version);
    return !package_record;
}
int gp_choose_launch(const char *model,const char *title,const char *version,int catalog,int ready,int hook,int on) {
    if(!gp_managed_title(title))return 0;
    if(!gp_supported(model,title,version)||!catalog)return -1;
    if(!strcmp(title,"CUSA08809"))return hook?0:-1;
    return ready?(on?'H':'O'):-1;
}
