/* Shared helpers for the GPU Lab payloads. Header-only: each payload is one
 * translation unit plus scan_engine.c. */
#ifndef PAYLOAD_COMMON_H
#define PAYLOAD_COMMON_H
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef HELIX_ROOT
#define HELIX_ROOT "/data/helixsr-speed"
#endif
#ifndef USER_APPMETA
#define USER_APPMETA "/user/appmeta"
#endif
#ifndef SYSTEM_APPMETA
#define SYSTEM_APPMETA "/system_data/priv/appmeta"
#endif

/* Whole small file, NUL-terminated; returns its length or -1. */
static int read_bounded(const char *path,char *data,size_t capacity)
{
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;
    struct stat st;size_t used=0;int rc=-1;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(unsigned long long)st.st_size>=capacity)goto done;
    for(;;) {
        ssize_t n=read(fd,data+used,capacity-1-used);
        if(n<0){if(errno==EINTR)continue;goto done;}
        if(!n)break;
        used+=(size_t)n;if(used==capacity-1)goto done;
    }
    data[used]=0;rc=(int)used;
done:
    if(close(fd))rc=-1;
    return rc;
}
/* Write a sibling .tmp, fsync, then rename over path. */
static int write_atomic(const char *path,const char *data,size_t length)
{
    char stage[256];
    if(snprintf(stage,sizeof stage,"%s.tmp",path)>=(int)sizeof stage)return -1;
    int fd=open(stage,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0644);if(fd<0)return -1;
    size_t used=0;int ok=1;
    while(ok&&used<length) {
        ssize_t n=write(fd,data+used,length-used);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)ok=0;else used+=(size_t)n;
    }
    if(ok&&fchmod(fd,0644))ok=0;
    if(ok&&fsync(fd))ok=0;
    if(close(fd))ok=0;
    if(!ok||rename(stage,path)){unlink(stage);return -1;}
    return 0;
}
/* PPSA or CUSA plus five digits. */
static int valid_title(const char *title)
{
    if(strlen(title)!=9||(strncmp(title,"PPSA",4)&&strncmp(title,"CUSA",4)))return 0;
    for(unsigned i=4;i<9;i++)if(title[i]<'0'||title[i]>'9')return 0;
    return 1;
}
/* The string value of the first "key" at or after from; escapes are kept
 * literally except \" and \\. Returns where the value ends, or NULL. */
static const char *json_string(const char *from,const char *key,char *out,size_t capacity)
{
    char quoted[64];snprintf(quoted,sizeof quoted,"\"%s\"",key);
    const char *p=strstr(from,quoted);if(!p)return NULL;
    p+=strlen(quoted);while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;
    if(*p!=':')return NULL;
    ++p;while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;
    if(*p!='"')return NULL;
    ++p;size_t n=0;
    while(*p&&*p!='"') {
        char c=*p++;
        if(c=='\\'&&*p){c=*p++;}
        if(n+1<capacity)out[n++]=c;
    }
    out[n]=0;return *p=='"'?p+1:NULL;
}
/* Printable ASCII without tabs, single spaces, at most 79 characters. Other
 * bytes (UTF-8 marks such as the trademark sign) are dropped. */
static void plain_name(const char *in,char out[80])
{
    size_t n=0;
    for(const unsigned char *p=(const unsigned char *)in;*p&&n<79;++p) {
        if(*p<32||*p>126)continue;
        if(*p==' '&&(!n||out[n-1]==' '))continue;
        out[n++]=(char)*p;
    }
    while(n&&out[n-1]==' ')--n;
    out[n]=0;
}
/* One UTF-8 string entry of a PS4 param.sfo (PSF) image; 0 on success. Every offset and length is checked against the
 * image size, so a damaged file only fails the lookup. */
static int sfo_string(const unsigned char *sfo,size_t size,const char *key,char *out,size_t capacity)
{
    if(size<20||memcmp(sfo,"\0PSF",4))return -1;
    unsigned keys=sfo[8]|sfo[9]<<8|sfo[10]<<16|(unsigned)sfo[11]<<24,data=sfo[12]|sfo[13]<<8|sfo[14]<<16|(unsigned)sfo[15]<<24;
    unsigned count=sfo[16]|sfo[17]<<8|sfo[18]<<16|(unsigned)sfo[19]<<24;
    if(keys>size||data>size||count>1024||20+(size_t)count*16>size)return -1;
    for(unsigned i=0;i<count;++i) {
        const unsigned char *e=sfo+20+i*16;
        unsigned key_at=keys+(e[0]|e[1]<<8),format=e[2]|e[3]<<8;
        unsigned length=e[4]|e[5]<<8|e[6]<<16|(unsigned)e[7]<<24,at=e[12]|e[13]<<8|e[14]<<16|(unsigned)e[15]<<24;
        if(key_at>=size||strnlen((const char *)sfo+key_at,size-key_at)==size-key_at||strcmp((const char *)sfo+key_at,key))continue;
        if((format!=0x0204&&format!=0x0004)||(size_t)data+at+length>size||!capacity)return -1;
        size_t n=strnlen((const char *)sfo+data+at,length);if(n>=capacity)n=capacity-1;
        memcpy(out,sfo+data+at,n);out[n]=0;return 0;
    }
    return -1;
}
/* Installed title metadata; 0 on success. PS5 titles have /user/appmeta/TITLE/param.json; PS4 titles (CUSA, run in
 * backward-compatibility mode) have param.sfo, whose APP_VER ("01.00") is the version their patches use. The PS5 Pro
 * (13.60) keeps a PS4 title's param.sfo in /system_data/priv/appmeta/TITLE (seen for CUSA08809, 29 Oct 2026), not in
 * /user/appmeta, so both places are tried. */
static int title_info(const char *title,char version[32],char name[80])
{
    static char json[65536];char path[512],language[16]="en-US",raw[256];
    if(!valid_title(title))return -1;
    snprintf(path,sizeof path,USER_APPMETA "/%s/param.json",title);
    if(read_bounded(path,json,sizeof json)<0) {
        snprintf(path,sizeof path,SYSTEM_APPMETA "/%s/param.sfo",title);
        int size=read_bounded(path,json,sizeof json);
        if(size<0) {
            snprintf(path,sizeof path,USER_APPMETA "/%s/param.sfo",title);
            size=read_bounded(path,json,sizeof json);
        }
        if(size<0||sfo_string((const unsigned char *)json,(size_t)size,"APP_VER",version,32)||!version[0])return -1;
        if(sfo_string((const unsigned char *)json,(size_t)size,"TITLE",raw,sizeof raw))raw[0]=0;
        plain_name(raw,name);
        if(!name[0])snprintf(name,80,"%s",title);
        return 0;
    }
    if(!json_string(json,"contentVersion",version,32)||!version[0])return -1;
    json_string(json,"defaultLanguage",language,sizeof language);
    char block[24];snprintf(block,sizeof block,"\"%s\"",language);
    const char *localized=strstr(json,"\"localizedParameters\"");
    const char *section=localized?strstr(localized,block):NULL;
    if(!(section&&json_string(section,"titleName",raw,sizeof raw))&&!json_string(json,"titleName",raw,sizeof raw))raw[0]=0;
    plain_name(raw,name);
    if(!name[0])snprintf(name,80,"%s",title);
    return 0;
}
#endif
