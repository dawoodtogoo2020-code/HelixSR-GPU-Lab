/* GPU Lab launch hand-off with a one-time upscaler scan.
 *
 * Launch: the same contract as launch-handoff.elf. GPU Lab writes
 * launch-request.txt ("HLX1 TITLE PID ISSUED"); this payload consumes it once,
 * refuses stale or malformed requests, waits for GPU Lab to exit, and launches
 * the title. The title must be installed (it no longer needs a catalog row, so
 * new games can be launched and scanned).
 *
 * Scan, the user's read-only exception to "no file operations while a game
 * runs" (26 Oct 2026): only when scan/TITLE.txt is missing or for another
 * version, and only while the launched title is the running app, read its
 * app0/eboot.bin once, throttled, never writing to it. Results go to
 * scan/TITLE.txt; a missing catalog row for this title and version is added
 * with the executable SHA-256 seen here.
 *
 * HelixSR on/off (HLX2 requests, 27 Oct 2026): "HLX2 TITLE PID ISSUED MODE",
 * MODE H (HelixSR on) or O (the game's own upscaler). After GPU Lab has exited
 * and no app runs, helixsr_swap.c moves the game's files and the HelixSR
 * package's files into place (rename only, every file identified by SHA-256,
 * nothing changed when anything is unrecognised). The result goes to
 * install/TITLE.txt for GPU Lab and to an on-screen notification. If HelixSR
 * was asked for and could not be switched on, GPU Lab is reopened instead of
 * the game so the reason is on screen. */
#include "payload_common.h"
#include "../common/package_support.h"
#include "scan_engine.h"
#include "helixsr_swap.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#ifndef HELIX_HOST_TEST
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#endif

int sceLncUtilGetAppIdOfRunningBigApp(void);
int sceLncUtilGetAppTitleId(uint32_t,char *);
int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(uint32_t *);
struct launch_context { uint32_t size,user_id,options; uint64_t crash_report; uint32_t flags; };
int sceSystemServiceLaunchApp(const char *,char **,struct launch_context *);

static const char request_path[]=HELIX_ROOT "/launch-request.txt";
static FILE *log_file;
#ifndef GAME_ROOT_FORMAT
#define GAME_ROOT_FORMAT "/data/homebrew/backports/%s"
#endif
typedef struct notify_request { char useless1[45]; char message[3075]; } notify_request_t;
int sceKernelSendNotificationRequest(int,notify_request_t *,size_t,int);
static void notify(const char *text)
{
    static notify_request_t request;memset(&request,0,sizeof request);
    snprintf(request.message,sizeof request.message,"%s",text);
    sceKernelSendNotificationRequest(0,&request,sizeof request,0);
}
/* HelixSR on (H) or the game's own files (O) for this title and version, recorded in install/TITLE.txt.
 * Returns 1 when the game may start. */
static int apply_mode(const char *title,const char *version,const char *name,char mode)
{
    const hs_package *package=hs_find(title,version);
    char game_root[512],package_root[512],status_path[512],text[512];
    snprintf(game_root,sizeof game_root,GAME_ROOT_FORMAT,title);
    snprintf(package_root,sizeof package_root,HELIX_ROOT "/packages/%s-%s",title,version);
    mkdir(HELIX_ROOT "/install",0755);
    snprintf(status_path,sizeof status_path,HELIX_ROOT "/install/%s.txt",title);
    hs_result result;
    if(!package){memset(&result,0,sizeof result);snprintf(result.state,sizeof result.state,"package_missing");
        snprintf(result.reason,sizeof result.reason,"HelixSR has no package for this game version.");}
    else if(mode=='H'&&!gp_supported(gp_device(HELIX_ROOT),title,version)) {
        memset(&result,0,sizeof result);snprintf(result.state,sizeof result.state,"unsupported");
        snprintf(result.reason,sizeof result.reason,"Console model or game version is unsupported.");
    } else {
        if(mode=='H')notify("HelixSR: switching the game to HelixSR...");
        hs_apply(package,game_root,package_root,mode=='H'?HS_HELIX:HS_ORIGINAL,HELIX_ROOT "/install/hash-cache.txt",&result,log_file);
    }
    int length=snprintf(text,sizeof text,"HINST1\nversion=%s\npackage=%s\nrequested=%s\nok=%d\nstate=%s\nmoves=%u\nreason=%s\n",
        version,package?package->package:"none",mode=='H'?"helix":"original",result.ok,result.state,result.moves,result.reason);
    int saved=length>0&&length<(int)sizeof text?write_atomic(status_path,text,(size_t)length):-1;
    fprintf(log_file,"install mode=%c ok=%d state=%s moves=%u saved=%d reason=%s\n",mode,result.ok,result.state,result.moves,saved,result.reason);
    if(mode=='O'){
        if(result.ok&&result.moves){snprintf(text,sizeof text,"HelixSR is off for %s: the game's own upscaler runs.",name);notify(text);}
        /* Not switched back (HelixSR or an interrupted switch remains): reopen GPU Lab rather than start the game. */
        return result.ok;
    }
    snprintf(text,sizeof text,result.ok?"HelixSR is on for %s. Starting the game.":"HelixSR could not be switched on for %s. Opening GPU Lab.",name);
    notify(text);
    return result.ok;
}

/* 1 when a game process (eboot.bin) is running, 0 when the full process list has none, -1 when the list could not be
 * read completely. Files are only moved on 0: the same rule the PC tools follow before every console file operation. */
#ifdef HELIX_HOST_TEST
static int game_process_state(void) { return 0; }   /* the host test has no PS5 process list */
#else
static int game_process_state(void)
{
    /* The console's records are larger than the SDK's struct (fields appended), so step by the size the kernel reports;
     * the name sits at the SDK offset. The list counts only when every record agrees on that size and the shell's own
     * process is found by name, which also proves the name offset. */
    enum { NAME=offsetof(struct kinfo_proc,ki_comm), NAME_BYTES=sizeof(((struct kinfo_proc *)0)->ki_comm) };
    int mib[4]={CTL_KERN,KERN_PROC,KERN_PROC_PROC,0};size_t size=0;
    if(sysctl(mib,4,NULL,&size,NULL,0)||size<NAME+NAME_BYTES){fprintf(log_file,"process_list size_query=%zu errno=%d\n",size,errno);return -1;}
    size+=size/2+65536;
    unsigned char *list=malloc(size);if(!list)return -1;
    int state=-1,stride=0,shell=0,game=0;size_t count=0;
    if(!sysctl(mib,4,list,&size,NULL,0)&&size>=sizeof(int)) {
        memcpy(&stride,list,sizeof stride);
        if(stride>=(int)(NAME+NAME_BYTES)&&stride<=16384&&!(size%(size_t)stride)&&(count=size/(size_t)stride)>=20) {
            state=0;
            for(size_t i=0;i<count&&!state;++i) {
                int record;memcpy(&record,list+i*(size_t)stride,sizeof record);
                char name[NAME_BYTES+1];memcpy(name,list+i*(size_t)stride+NAME,NAME_BYTES);name[NAME_BYTES]=0;
                if(record!=stride)state=-1;
                if(!strcmp(name,"eboot.bin"))game=1;
                if(!strcmp(name,"SceShellUI"))shell=1;
            }
            if(!state)state=!shell?-1:game;
        }
    }
    if(state)fprintf(log_file,"process_list state=%d bytes=%zu stride=%d sdk=%zu count=%zu shell=%d game=%d\n",
                     state,size,stride,sizeof(struct kinfo_proc),count,shell,game);
    free(list);return state;
}
#endif
static int gate_no_game(void)
{
    int state=game_process_state();
    if(state)fprintf(log_file,"gate_refused process_state=%d\n",state);
    return state!=0;
}
/* The running big app's title, or "" when none. */
static void running_title(char title[64])
{
    title[0]=0;int app=sceLncUtilGetAppIdOfRunningBigApp();
    if(app>0&&sceLncUtilGetAppTitleId((uint32_t)app,title))title[0]=0;
}
static int still_running(const char *title) { char now[64];running_title(now);return !strcmp(now,title); }

static int scan_cached(const char *title,const char *version)
{
    char path[512],data[4096],want[48];
    snprintf(path,sizeof path,HELIX_ROOT "/scan/%s.txt",title);
    if(read_bounded(path,data,sizeof data)<0)return 0;
    snprintf(want,sizeof want,"\nversion=%s\n",version);
    return strstr(data,want)!=NULL;
}
/* Adds "device title version sha name" unless this title and version exist.
 * The device label follows the first existing row. */
static const char *catalog_add(const char *title,const char *version,const char *sha,const char *name)
{
    static char catalog[32768],copy[32768];
    int length=read_bounded(HELIX_ROOT "/catalog.tsv",catalog,sizeof catalog);
    if(length<0){if(errno!=ENOENT)return "read_error";length=0;catalog[0]=0;}
    char device[64];const char *result="added";
    const char *label=gp_device(HELIX_ROOT); if(!label)return "device_unknown";
    snprintf(device,sizeof device,"%s",label);
    memcpy(copy,catalog,(size_t)length+1);
    char *state=NULL;int first=1;
    for(char *line=strtok_r(copy,"\n",&state);line;line=strtok_r(NULL,"\n",&state)) {
        char *fields[5]={0};unsigned n=0;char *field_state=NULL;
        for(char *f=strtok_r(line,"\t\r",&field_state);f&&n<5;f=strtok_r(NULL,"\t\r",&field_state))fields[n++]=f;
        if(n!=5)continue;
        if(first)first=0;
        if(!strcmp(fields[1],title)&&!strcmp(fields[2],version))return strcmp(fields[3],sha)?"present_other_hash":"present";
    }
    char row[256];
    int n=snprintf(row,sizeof row,"%s%s\t%s\t%s\t%s\t%s\n",length&&catalog[length-1]!='\n'?"\n":"",device,title,version,sha,name);
    if(n<=0||(size_t)length+(size_t)n>=sizeof catalog)return "full";
    memcpy(catalog+length,row,(size_t)n);
    return write_atomic(HELIX_ROOT "/catalog.tsv",catalog,(size_t)length+(size_t)n)?"write_error":result;
}
static void scan(const char *title,const char *version,const char *name)
{
    /* The game's own files are mounted while it runs; wait for them. */
    char path[96]={0};struct stat st;unsigned waited=0;
    for(;waited<240;++waited,sleep(1)) {
        if(waited>5&&!still_running(title)){fprintf(log_file,"scan=game_closed waited=%u\n",waited);return;}
        for(unsigned slot=0;slot<10&&!path[0];++slot) {
            char candidate[96];snprintf(candidate,sizeof candidate,"/mnt/sandbox/%s_%03u/app0/eboot.bin",title,slot);
            if(!stat(candidate,&st)&&S_ISREG(st.st_mode))snprintf(path,sizeof path,"%s",candidate);
        }
        if(path[0])break;
    }
    if(!path[0]){fprintf(log_file,"scan=no_mount waited=%u\n",waited);return;}
    /* Let the game finish its first load before reading alongside it. */
    sleep(20);
    if(!still_running(title)){fputs("scan=game_closed before_read\n",log_file);return;}
    int fd=open(path,O_RDONLY|O_CLOEXEC);
    if(fd<0){fprintf(log_file,"scan=open_failed errno=%d path=%s\n",errno,path);return;}
    enum { CHUNK=4u<<20 };
    unsigned char *buffer=malloc(CHUNK);
    static se_state state;se_init(&state);
    struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);
    int ok=buffer!=NULL;unsigned chunks=0;
    while(ok) {
        ssize_t n=read(fd,buffer,CHUNK);
        if(n<0&&errno==EINTR)continue;
        if(n<0){ok=0;break;}
        if(!n)break;
        se_update(&state,buffer,(size_t)n);
        /* About 4 MB per 30 ms at most, and stop if the game has closed. */
        usleep(30000);
        if(!(++chunks%16)&&!still_running(title)){fputs("scan=game_closed during_read\n",log_file);ok=0;}
    }
    close(fd);free(buffer);
    if(!ok){fprintf(log_file,"scan=read_failed errno=%d bytes=%llu\n",errno,(unsigned long long)state.bytes);return;}
    clock_gettime(CLOCK_MONOTONIC,&end);
    char sha[65];se_finish(&state,sha);
    const char *catalog=catalog_add(title,version,sha,name);
    static char report[2048];size_t used=0;
    used+=(size_t)snprintf(report+used,sizeof report-used,"HSCAN1\ntitle=%s\nversion=%s\nname=%s\npath=%s\nbytes=%llu\nsha256=%s\n"
        "primary=%s\nengine=%s\ncatalog=%s\nread_ms=%lld\nhits=",title,version,name,path,(unsigned long long)state.bytes,sha,
        se_primary(&state),se_engine(&state),catalog,
        (long long)(end.tv_sec-start.tv_sec)*1000+(end.tv_nsec-start.tv_nsec)/1000000);
    for(unsigned t=0;t<SE_TECH_COUNT;++t)
        if(state.hits[t])used+=(size_t)snprintf(report+used,sizeof report-used,"%s:%u;",se_tech_name(t),(unsigned)state.hits[t]);
    used+=(size_t)snprintf(report+used,sizeof report-used,"\n");
    mkdir(HELIX_ROOT "/scan",0755);
    char out[512];snprintf(out,sizeof out,HELIX_ROOT "/scan/%s.txt",title);
    int rc=write_atomic(out,report,used);
    fprintf(log_file,"scan=done primary=%s engine=%s bytes=%llu sha256=%s catalog=%s write=%d\n",
            se_primary(&state),se_engine(&state),(unsigned long long)state.bytes,sha,catalog,rc);
}
int main(void)
{
    log_file=fopen(HELIX_ROOT "/launch-handoff.txt","w");if(!log_file)return 2;setbuf(log_file,NULL);
    char request[128],title[16]={0},extra;unsigned pid=0;unsigned long long issued=0;
    if(read_bounded(request_path,request,sizeof request)<0){fprintf(log_file,"refused=request_read errno=%d\n",errno);return 1;}
    /* Consume this one request. Retriggering the payload cannot relaunch it. */
    if(unlink(request_path)){fputs("refused=request_consume\n",log_file);return 1;}
    char mode=0,tail=0;
    if(sscanf(request,"HLX2 %15s %u %llu %c %c",title,&pid,&issued,&mode,&tail)==4){
        if(mode!='H'&&mode!='O'){fputs("refused=request_mode\n",log_file);return 1;}
    }else if(sscanf(request,"HLX1 %15s %u %llu %c",title,&pid,&issued,&extra)!=3)mode=1;
    if(mode==1||!valid_title(title)||!pid){fputs("refused=request_format\n",log_file);return 1;}
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now)||issued>(unsigned long long)now.tv_sec||(unsigned long long)now.tv_sec-issued>30)
        {fputs("refused=stale\n",log_file);return 1;}
    char version[32],name[80];
    if(title_info(title,version,name)){fputs("refused=not_installed\n",log_file);return 1;}
    if(!gp_request_allowed(gp_device(HELIX_ROOT),title,version,mode)){
        fputs("refused=managed_game_request_or_model\n",log_file);return 1;
    }
    fprintf(log_file,"request_pid=%u title=%s version=%s mode=%c\n",pid,title,version,mode?mode:'-');
    int free_slot=0;
    for(unsigned attempt=0;attempt<75;attempt++) {
        char running[64];running_title(running);
        if(!running[0]){free_slot=1;break;}
        if(strcmp(running,"PPSA99272")){fprintf(log_file,"refused=other_app title=%.63s\n",running);return 1;}
        usleep(200000);
    }
    if(!free_slot){fputs("refused=lab_did_not_exit\n",log_file);return 1;}
    /* The running-app query alone cannot tell "none" from "unknown": switching also needs a complete process list with
     * no game in it, checked again before every single rename. */
    int processes=mode?game_process_state():0;
    for(unsigned attempt=0;attempt<25&&processes;attempt++){usleep(200000);processes=game_process_state();}
    fprintf(log_file,"process_state=%d\n",processes);
    if(processes){fputs("refused=game_process\n",log_file);return 1;}
    hs_gate=gate_no_game;
    /* Files are moved only now: GPU Lab has exited and no app runs. */
    const char *start=title;
    if(mode&&!apply_mode(title,version,name,mode))start="PPSA99272";
    struct launch_context context={.size=sizeof context};char *args[]={0};
    int rc=sceUserServiceInitialize(0);
    if(rc==0||rc==(int)0x80960001){rc=sceUserServiceGetForegroundUser(&context.user_id);
        if(!rc)rc=sceSystemServiceLaunchApp(start,args,&context);}
    fprintf(log_file,"launch_result=%x user=%u started=%s\n",rc,context.user_id,start);
    if(rc<0)return 1;
    if(start!=title){fclose(log_file);return 0;}
    if(scan_cached(title,version))fputs("scan=cached\n",log_file);
    else scan(title,version,name);
    fclose(log_file);return 0;
}


