#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
static int fail_write, fail_rename, write_calls;
static ssize_t injected_write(int fd,const void *p,size_t n) {
    if(fail_write && write_calls++>0){errno=EIO;return -1;}
    return write(fd,p,fail_write&&n>1?1:n);
}
static int injected_rename(const char *a,const char *b) {
    if(fail_rename){errno=EIO;return -1;} return rename(a,b);
}
#define HOOK_WRITE injected_write
#define HOOK_RENAME injected_rename
#include "../source/hook_setting.c"
#include <assert.h>
#include <stdlib.h>
static void put(const char *p,const char *s) { FILE *f=fopen(p,"wb");assert(f);assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(!fclose(f)); }
static void same(const char *p,const char *s) { char b[512];FILE *f=fopen(p,"rb");assert(f);size_t n=fread(b,1,sizeof b,f);assert(!fclose(f));assert(n==strlen(s)&&!memcmp(b,s,n)); }
int main(int argc,char **argv) {
    assert(argc==2);char p[512],tmp[550],alias[512],dir[512];int on;
    snprintf(p,sizeof p,"%s/helixsr.txt",argv[1]);snprintf(tmp,sizeof tmp,"%s.gpulab-tmp",p);
    const char *orig="other=keep\r\nhelixsr=on\r\nscale=2\n";
    put(p,orig);assert(!hook_setting_read(p,"helixsr",&on)&&on==1);
    assert(!hook_setting_toggle(p,"helixsr",&on)&&on==0);same(p,"other=keep\r\nhelixsr=off\r\nscale=2\n");
    assert(!hook_setting_toggle(p,"helixsr",&on)&&on==1);same(p,orig);
    const char *bad[]={"helixsr=on\nhelixsr=off\n","helixsr=maybe\n","helixsr=on","other=on\n","helixsr=on\nhelixsr=on\n"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){put(p,bad[i]);assert(hook_setting_toggle(p,"helixsr",&on));same(p,bad[i]);}
    put(p,orig);put(tmp,"existing");assert(hook_setting_toggle(p,"helixsr",&on));same(p,orig);same(tmp,"existing");assert(!unlink(tmp));
    fail_write=1;write_calls=0;assert(hook_setting_toggle(p,"helixsr",&on));same(p,orig);assert(access(tmp,F_OK));fail_write=0;
    fail_rename=1;assert(hook_setting_toggle(p,"helixsr",&on));same(p,orig);assert(access(tmp,F_OK));fail_rename=0;
    assert(hook_setting_read(p,"invalid",&on));
    snprintf(dir,sizeof dir,"%s/real",argv[1]);snprintf(alias,sizeof alias,"%s/alias",argv[1]);assert(!mkdir(dir,0700));assert(!symlink(dir,alias));
    char through[600];snprintf(through,sizeof through,"%s/file",alias);assert(hook_setting_read(through,"helixsr",&on));
    assert(!unlink(alias));assert(!rmdir(dir));assert(!unlink(p));puts("hook settings: PASS");return 0;
}
