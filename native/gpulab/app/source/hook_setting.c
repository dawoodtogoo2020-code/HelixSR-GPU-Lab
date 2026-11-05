#define _POSIX_C_SOURCE 200809L
#include "hook_setting.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef HOOK_WRITE
#define HOOK_WRITE write
#endif
#ifndef HOOK_RENAME
#define HOOK_RENAME rename
#endif
#define LIMIT 4096

static int valid_key(const char *key) {
    return key && (!strcmp(key,"helixsr") || !strcmp(key,"overlay"));
}
static int safe_parents(const char *path) {
    char p[1024]; struct stat st; size_t n;
    if(!path||path[0]!='/'||(n=strlen(path))>=sizeof p||n<2)return 0;
    memcpy(p,path,n+1);
    for(size_t i=1;i<n;i++) if(p[i]=='/') {
        p[i]=0; int bad=lstat(p,&st)||!S_ISDIR(st.st_mode); p[i]='/';
        if(bad)return 0;
    }
    return 1;
}
/* Reject incomplete lines, NULs, duplicate or unrecognised setting values. */
static int load(const char *path,const char *key,char *buf,size_t *len,int *value,size_t *at,size_t *width) {
    struct stat before,after; size_t used=0,count=0,k=strlen(key);
    if(!safe_parents(path)||lstat(path,&before)||!S_ISREG(before.st_mode)||before.st_size<=0||before.st_size>LIMIT)return -1;
    int fd=open(path,O_RDONLY|O_NOFOLLOW); if(fd<0)return -1;
    int bad=fstat(fd,&after)||!S_ISREG(after.st_mode)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size;
    while(!bad&&used<(size_t)before.st_size) {
        ssize_t n=read(fd,buf+used,(size_t)before.st_size-used);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){bad=1;break;} used+=(size_t)n;
    }
    if(!bad&&(fstat(fd,&after)||after.st_size!=before.st_size||after.st_mtim.tv_sec!=before.st_mtim.tv_sec||after.st_mtim.tv_nsec!=before.st_mtim.tv_nsec))bad=1;
    if(close(fd))bad=1;
    if(bad||used!=(size_t)before.st_size||buf[used-1]!='\n'||memchr(buf,0,used))return -1;
    for(size_t i=0;i<used;) {
        size_t end=i; while(end<used&&buf[end]!='\n')end++;
        size_t content=end-i; if(content&&buf[i+content-1]=='\r')content--;
        if(content>=k+1&&!memcmp(buf+i,key,k)&&buf[i+k]=='=') {
            size_t v=content-k-1; const char *p=buf+i+k+1;
            if(++count!=1)return -1;
            if(v==2&&!memcmp(p,"on",2))*value=1;
            else if(v==3&&!memcmp(p,"off",3))*value=0;
            else return -1;
            *at=i+k+1; *width=v;
        }
        i=end+1;
    }
    if(count!=1)return -1;
    *len=used; return 0;
}
int hook_setting_read(const char *path,const char *key,int *on) {
    char b[LIMIT]; size_t n,at,width; int value;
    if(!on||!valid_key(key)||load(path,key,b,&n,&value,&at,&width))return -1;
    *on=value;return 0;
}
int hook_setting_toggle(const char *path,const char *key,int *on) {
    char original[LIMIT],check[LIMIT],output[LIMIT],tmp[1100]; size_t n,at,width,cn,ca,cw; int value,cv;
    if(!on||!valid_key(key)||load(path,key,original,&n,&value,&at,&width))return -1;
    const char *replacement=value?"off":"on";size_t z=value?3:2, out=n-width+z;
    if(out>LIMIT||snprintf(tmp,sizeof tmp,"%s.gpulab-tmp",path)>=(int)sizeof tmp)return -1;
    memcpy(output,original,at);memcpy(output+at,replacement,z);memcpy(output+at+z,original+at+width,n-at-width);
    struct stat original_stat;
    if(lstat(path,&original_stat)||!S_ISREG(original_stat.st_mode))return -1;
    int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return -1;
    int bad=fchmod(fd,original_stat.st_mode & 0777)!=0; size_t done=0;
    while(!bad&&done<out){ssize_t w=HOOK_WRITE(fd,output+done,out-done);if(w<0&&errno==EINTR)continue;if(w<=0){bad=1;break;}done+=(size_t)w;}
    if(!bad&&fsync(fd))bad=1;
    if(close(fd))bad=1;
    /* Refuse to replace a file that changed since the original read. */
    if(!bad&&(load(path,key,check,&cn,&cv,&ca,&cw)||cn!=n||memcmp(check,original,n)))bad=1;
    if(!bad&&HOOK_RENAME(tmp,path))bad=1;
    if(bad){unlink(tmp);return -1;}
    if(load(path,key,check,&cn,&cv,&ca,&cw)||cn!=out||memcmp(check,output,out)||cv==value)return -1;
    *on=cv;return 0;
}
