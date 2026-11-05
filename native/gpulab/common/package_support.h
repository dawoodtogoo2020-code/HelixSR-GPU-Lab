/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GP_PACKAGE_SUPPORT_H
#define GP_PACKAGE_SUPPORT_H
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
/* POSIX lstat may be hidden by strict C feature profiles. */
extern int lstat(const char *, struct stat *);
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
static inline int gp_safe_root(const char *root) {
    char b[512]; struct stat st; size_t n;
    if(!root || root[0]!='/' || !(n=strlen(root)) || n>=sizeof b)return 0;
    memcpy(b,root,n+1);
    for(size_t i=1;i<=n;i++)if(b[i]=='/'||b[i]==0) {
        char c=b[i];b[i]=0;int bad=lstat(b,&st)||!S_ISDIR(st.st_mode);b[i]=c;if(bad)return 0;
    }
    return 1;
}
static inline const char *gp_device(const char *root) {
    char path[512], b[32]; struct stat st;
    if (!gp_safe_root(root) || snprintf(path,sizeof path,"%s/device.txt",root)>=(int)sizeof path) return NULL;
    struct stat before;
    if(lstat(path,&before)||!S_ISREG(before.st_mode))return NULL;
    int fd=open(path,O_RDONLY|O_NOFOLLOW); if(fd<0)return NULL;
    int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_dev==before.st_dev&&st.st_ino==before.st_ino&&st.st_size>0&&st.st_size<(off_t)(sizeof b-1);
    ssize_t n=ok?read(fd,b,sizeof b-1):-1; close(fd);
    if(!ok||n!=st.st_size)return NULL;
    b[n]=0; while(n>0&&(b[n-1]=='\n'||b[n-1]=='\r'))b[--n]=0;
    if(!strcmp(b,"ps5-regular"))return "ps5-regular";
    if(!strcmp(b,"ps5-pro"))return "ps5-pro";
    return NULL;
}
static inline int gp_supported(const char *model,const char *title,const char *version) {
    if(!model||!title||!version)return 0;
    if(!strcmp(model,"ps5-regular"))return (!strcmp(title,"PPSA07631")&&!strcmp(version,"01.000.000"))||(!strcmp(title,"PPSA02572")&&!strcmp(version,"01.200.007"))||(!strcmp(title,"PPSA34547")&&!strcmp(version,"01.300.300"));
    return !strcmp(model,"ps5-pro")&&!strcmp(title,"CUSA08809")&&!strcmp(version,"01.00");
}
static inline int gp_managed_title(const char *title) {
    return title&&(!strcmp(title,"PPSA07631")||!strcmp(title,"PPSA34547")||!strcmp(title,"PPSA02572")||!strcmp(title,"CUSA08809"));
}
static inline int gp_request_allowed(const char *model,const char *title,const char *version,char mode) {
    if(!gp_managed_title(title))return mode==0;
    if(!gp_supported(model,title,version))return 0;
    if(!strcmp(title,"CUSA08809"))return mode==0;
    return mode=='H'||mode=='O';
}
#endif
