/* GPU Lab inventory payload: lists installed PS5/PS4 games for the library.
 * GPU Lab runs as an unprivileged user and cannot read /user/appmeta, so it
 * starts this payload and reads the result. Writes only
 * /data/helixsr-speed/installed.tsv (title, version, name) and its log.
 * Homebrew IDs (PPSA99xxx) are left out. */
#include "payload_common.h"
#include <dirent.h>
#include <stdlib.h>

typedef struct { char title[16],version[32],name[80]; } game;

static int by_name(const void *a,const void *b) { return strcmp(((const game *)a)->name,((const game *)b)->name); }

int main(void)
{
    FILE *log=fopen(HELIX_ROOT "/inventory.txt","w");if(!log)return 2;
    setbuf(log,NULL);fprintf(log,"HELIXSR_INVENTORY_V1 pid=%d\n",getpid());
    static game games[64];unsigned count=0;
    const char *roots[]={USER_APPMETA,SYSTEM_APPMETA};unsigned opened=0;
    for(unsigned root=0;root<2;++root) {
        DIR *dir=opendir(roots[root]);
        if(!dir){if(errno==ENOENT)continue;fprintf(log,"refused=appmeta errno=%d\n",errno);fclose(log);return 1;}
        opened++;
        for(;;) {
            errno=0;struct dirent *e=readdir(dir);
            if(!e){if(errno){closedir(dir);fclose(log);return 1;}break;}
            if(!valid_title(e->d_name)||!strncmp(e->d_name,"PPSA99",6))continue;
            int duplicate=0;for(unsigned i=0;i<count;++i)if(!strcmp(games[i].title,e->d_name))duplicate=1;
            if(duplicate)continue;
            if(count==64){closedir(dir);fputs("refused=inventory_capacity\n",log);fclose(log);return 1;}
            game g;snprintf(g.title,sizeof g.title,"%s",e->d_name);
            if(title_info(g.title,g.version,g.name)){fprintf(log,"skipped=%s param\n",g.title);continue;}
            games[count++]=g;
        }
        closedir(dir);
    }
    if(!opened){fputs("refused=no_appmeta\n",log);fclose(log);return 1;}
    qsort(games,count,sizeof *games,by_name);
    static char out[64*128];size_t used=0;
    for(unsigned i=0;i<count;++i) {
        used+=(size_t)snprintf(out+used,sizeof out-used,"%s\t%s\t%s\n",games[i].title,games[i].version,games[i].name);
        fprintf(log,"game=%s version=%s name=%s\n",games[i].title,games[i].version,games[i].name);
    }
    int rc=write_atomic(HELIX_ROOT "/installed.tsv",out,used);
    fprintf(log,"games=%u write=%d\n",count,rc);fclose(log);
    return rc?1:0;
}
