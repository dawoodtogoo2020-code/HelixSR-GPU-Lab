#ifndef GP_HOOK_SETTING_H
#define GP_HOOK_SETTING_H
#ifdef __cplusplus
extern "C" {
#endif
int hook_setting_read(const char *path,const char *key,int *on);
int hook_setting_toggle(const char *path,const char *key,int *on);
#ifdef __cplusplus
}
#endif
#endif
