#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "display.h"
extern "C" {
#include "app_frontend.h"
#include "logical_input.h"
#include "text_panel.h"
#include "pad.h"
#include "render_options.h"
#include "reset_all.h"
#include "ui_screen.h"
#include "game_library.h"
#include "frame_log.h"
#include "resolution_plan.h"
#include "hook_setting.h"
#include "launch_policy.h"
#include "../../common/package_support.h"
int sceUserServiceGetForegroundUser(uint32_t *);
int sceKernelStat(const char*,struct stat*);
int sceKernelFstat(int,struct stat*);
int helix_native_storage_check(FILE*);
int helix_native_render_check(FILE*,const ap_identity*);
int helix_native_reset_check(FILE*,const ap_identity*);
int psbc_probe_run(FILE*);
int encoder_probe_run(FILE*);
int encoder_next_probe_run(FILE*,int);
int model_sequence_probe_run(FILE*,int);
int enc2_saturation_native_run(FILE*,int);
int model_large_native_run(FILE*,int);
int k10_scratch_native_run(FILE*);
int scratch_canary_native_run(FILE*,int);
int gpu_timestamp_native_run(FILE*);
int helix_native_profile_check(FILE*,const ap_identity*);
int sceSystemServiceLoadExec(const char*,const char* const*);
struct launch_context_t {uint32_t size,user_id,options;uint64_t crash_report;uint32_t flags;};
int sceSystemServiceLaunchApp(const char*,char**,launch_context_t*);
}

/* Native integration of the existing control/session/storage/frontend stack.
 * Saving a preference does not assert a working game hook. */
struct Game { ap_identity id; char name[80]; };
static Game games[64];
static unsigned game_count, selected;
static app_controller controller;
static app_frontend frontend;
static app_status_view view;
static FILE *logfile;
static const char *settings_root="/data/helixsr-speed/settings";
static char feedback[80]="Select a game. Hook connection is pending.";
static unsigned menu,focus;
static bool preference_dirty,selection_valid;
static ap_desired edited_desired;
static int reset_armed=-1;
enum { FIELD_HELIX,FIELD_HUD,FIELD_RESOLUTION,FIELD_FRAMES,FIELD_COUNT };
/* u07: the main screen, the resolution editor over it, or the frame-rate screen. */
static unsigned view_screen;
static bool rich_screen;   /* the 1080p screen is up: the u07 screens need it (the text panel has no room) */
/* Library entries: every catalog game, then installed games the catalog does
 * not list yet (from the inventory payload). Only catalog games have
 * profiles; the launch payload scans a game and catalogues it. */
struct Entry { bool catalog; unsigned index; gl_finding finding; };
static gl_game installed[GL_MAX_GAMES];
static unsigned installed_count;
static Entry entries[2*GL_MAX_GAMES];
static unsigned entry_count,cursor;
static const char *helix_root="/data/helixsr-speed";
static_assert((int)FIELD_HELIX==US_HELIX&&(int)FIELD_HUD==US_HUD&&(int)FIELD_RESOLUTION==US_RESOLUTION&&(int)FIELD_FRAMES==US_FRAMES&&
              (int)FIELD_COUNT==US_FIELD_COUNT&&sizeof games/sizeof *games==US_MAX_GAMES&&GL_MAX_GAMES==US_MAX_GAMES,
              "screen field order");

static const char *device_model() { static const char *model=gp_device(helix_root);return model; }
static bool is_pro() { const char *model=device_model();return model&&!strcmp(model,"ps5-pro"); }
static const char *entry_title(unsigned i) { return entries[i].catalog?games[entries[i].index].id.title:installed[entries[i].index].title; }
static const char *entry_version(unsigned i) { return entries[i].catalog?games[entries[i].index].id.build:installed[entries[i].index].version; }
static bool on_installed() { return entry_count&&!entries[cursor].catalog; }
static bool supported_entry(unsigned i) { return gp_supported(device_model(),entry_title(i),entry_version(i)); }
/* The selected catalog game has a verified HelixSR package on the console; Triangle then applies ON/OFF. */
/* The selected game's package and last-switch record. The screen is rebuilt every frame, so the files are re-read only
 * when the selection changes or after a second (only the PC setup tool and the launch payload write them). */
struct PackageView { unsigned cursor; uint64_t stamp_ms; bool loaded,ready,last,ok; char description[96],state[24],reason[200];
    unsigned input_width,input_height,output_width,output_height,name_length;
    /* u07: every pair the package runs at: the READY line's, else the package table's (r18 names none). */
    char id[48]; rp_pair pairs[4]; unsigned pair_count; };
static bool entry_ready[2*GL_MAX_GAMES];
/* The READY line names the resolution the package renders at and outputs, as "WxH -> WxH"; nothing is assumed when
 * the line does not say. */
static void package_resolution(PackageView &v) {
    const char *at=strstr(v.description," -> ");
    if(!at)return;
    const char *start=at;while(start>v.description&&start[-1]!=' ')--start;
    unsigned iw=0,ih=0,ow=0,oh=0;
    if(sscanf(start,"%ux%u -> %ux%u",&iw,&ih,&ow,&oh)==4&&iw&&ih&&ow&&oh&&iw<=ow&&ih<=oh&&ow<=7680&&oh<=4320)
        {v.input_width=iw;v.input_height=ih;v.output_width=ow;v.output_height=oh;
         size_t n=(size_t)(start-v.description);while(n&&(v.description[n-1]==' '||v.description[n-1]==','))--n;v.name_length=(unsigned)n;}
}
static PackageView package_view;
static const PackageView &package_info() {
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    const uint64_t now_ms=(uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
    PackageView &v=package_view;
    if(!v.loaded||v.cursor!=cursor||now_ms-v.stamp_ms>1000u) {
        v=PackageView{};v.loaded=true;v.cursor=cursor;v.stamp_ms=now_ms;
        if(entry_count&&entries[cursor].catalog) {
            v.ready=supported_entry(cursor)&&gl_package_ready(helix_root,entry_title(cursor),entry_version(cursor),v.description,sizeof v.description);
            int ok=0;v.last=gl_install_state(helix_root,entry_title(cursor),entry_version(cursor),v.state,v.reason,&ok);v.ok=ok;
            if(v.ready){
                package_resolution(v);
                gl_package_id(helix_root,entry_title(cursor),entry_version(cursor),v.id,sizeof v.id);
                v.pair_count=rp_parse_pairs(v.description,v.pairs,4);
                if(!v.pair_count)v.pair_count=rp_package_pairs(v.id,v.pairs,4);
                if(!v.input_width&&v.pair_count){v.input_width=v.pairs[0].input_width;v.input_height=v.pairs[0].input_height;
                    v.output_width=v.pairs[0].output_width;v.output_height=v.pairs[0].output_height;}
            }
        }
        for(unsigned i=0;i<entry_count;++i)
            entry_ready[i]=entries[i].catalog&&supported_entry(i)&&gl_package_ready(helix_root,entry_title(i),entry_version(i),nullptr,0);
    }
    return v;
}
static bool package_ready() { return package_info().ready; }
/* The HelixSR HUD of the selected game's hook (x03 wrapper/hud): /data/helixsr-hook/TITLE/hud.txt exists for a game
 * whose hook has a HUD; "overlay=on" shows the FPS box. Re-read on selection change and every second, because the
 * game's hook rewrites the file when the player switches the HUD with L3 + R3. */
struct HudView { unsigned cursor; uint64_t stamp_ms; bool loaded,available,on; };
static HudView hud_view;
static void hud_path(char *path,size_t size) { snprintf(path,size,"/data/helixsr-hook/%s/hud.txt",entry_title(cursor)); }
static const HudView &hud_info() {
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    const uint64_t now_ms=(uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
    HudView &v=hud_view;
    if(!v.loaded||v.cursor!=cursor||now_ms-v.stamp_ms>1000u) {
        v=HudView{};v.loaded=true;v.cursor=cursor;v.stamp_ms=now_ms;
        char path[128];
        if(entry_count&&supported_entry(cursor)&&!strcmp(entry_title(cursor),"CUSA08809")) {
            hud_path(path,sizeof path);
            int on=0;v.available=hook_setting_read(path,"overlay",&on)==0;v.on=on!=0;
        }
    }
    return v;
}
/* Cross on the HUD row: rewrites hud.txt with the other overlay setting and keeps its other lines (scale=). */
static void toggle_hud() {
    if(!entry_count||!hud_info().available){snprintf(feedback,sizeof feedback,"No HelixSR HUD for this game.");return;}
    char path[128];
    hud_path(path,sizeof path);
    int on=0;const bool wrote=hook_setting_toggle(path,"overlay",&on)==0;
    fprintf(logfile,"hud_toggle title=%s on=%d wrote=%d\n",entry_title(cursor),on,wrote);
    hud_view.loaded=false;
    snprintf(feedback,sizeof feedback,"%s",!wrote?"The HUD setting could not be saved.":
             on?"HelixSR HUD on: the FPS box shows in the game (L3 + R3 there switches it).":
             "HelixSR HUD off. In the game, hold L3 + R3 to show it again.");
}
/* u10: the live HelixSR switch of a hooked game that has no package (SHADOW OF THE COLOSSUS on the Pro: x05's helix module
 * in the game's GNM hook runs HelixSR in every frame). /data/helixsr-hook/TITLE/helixsr.txt holds "helixsr=on" or
 * "helixsr=off"; the hook reads it when the game starts (it only loads HelixSR when it says on) and every 61 frames. The
 * file existing is what makes the switch available; GPU Lab then is the injector: Cross sets the switch, Triangle starts
 * the game, and the hook does the rest. */
struct HookView { unsigned cursor; uint64_t stamp_ms; bool loaded,available,on; };
static HookView hook_view;
static void hook_path(char *path,size_t size) { snprintf(path,size,"/data/helixsr-hook/%s/helixsr.txt",entry_title(cursor)); }
static const HookView &hook_info() {
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    const uint64_t now_ms=(uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
    HookView &v=hook_view;
    if(!v.loaded||v.cursor!=cursor||now_ms-v.stamp_ms>1000u) {
        v=HookView{};v.loaded=true;v.cursor=cursor;v.stamp_ms=now_ms;
        char path[128];
        if(entry_count&&supported_entry(cursor)&&!strcmp(entry_title(cursor),"CUSA08809")) {
            hook_path(path,sizeof path);
            int on=0;v.available=hook_setting_read(path,"helixsr",&on)==0;v.on=on!=0;
        }
    }
    return v;
}
static bool hook_switch() { return entry_count&&!package_ready()&&hook_info().available; }
/* Cross on the HelixSR row of such a game: rewrites helixsr.txt with the other setting and keeps its other lines. */
static void toggle_hook() {
    if(!hook_switch()){snprintf(feedback,sizeof feedback,"HelixSR hook setting unavailable.");return;}
    char path[128];
    hook_path(path,sizeof path);
    int on=0;const bool wrote=hook_setting_toggle(path,"helixsr",&on)==0;
    fprintf(logfile,"hook_toggle title=%s on=%d wrote=%d\n",entry_title(cursor),on,wrote);
    hook_view.loaded=false;
    snprintf(feedback,sizeof feedback,"%s",!wrote?"The HelixSR setting could not be saved.":
             on?"HelixSR ON. Triangle starts the game; its hook runs HelixSR.":
             "HelixSR OFF. Triangle starts the game with its own upscale.");
}
static void build_entries() {
    char keep[16]={0};
    if(entry_count)snprintf(keep,sizeof keep,"%s",entry_title(cursor));
    entry_count=0;
    for(unsigned i=0;i<game_count;++i)
        if(gp_inventory_match(installed,installed_count,games[i].id.title,games[i].id.build))entries[entry_count++]={true,i,{}};
    for(unsigned i=0;i<installed_count;++i){
        bool listed=false;
        for(unsigned j=0;j<game_count;++j)
            if(!strcmp(games[j].id.title,installed[i].title)&&!strcmp(games[j].id.build,installed[i].version))listed=true;
        if(!listed)entries[entry_count++]={false,i,{}};
    }
    cursor=0;
    for(unsigned i=0;i<entry_count;++i){
        gl_find(helix_root,entry_title(i),entry_version(i),&entries[i].finding);
        if(keep[0]&&!strcmp(entry_title(i),keep)&&!cursor)cursor=i;
    }
}

static bool save_options();
static void load_preference() {
    // The saved choice is what runs: ON = the HelixSR package, OFF = the game's own files and upscaler.
    edited_desired=view.preference_known?view.saved_desired:package_ready()?AP_ON:AP_OFF;preference_dirty=edited_desired!=view.saved_desired;
    if(preference_dirty)save_options();
    snprintf(feedback,sizeof feedback,"%s",hook_switch()?(hook_info().on?"HelixSR ON in the game's hook. Cross switches it OFF; Triangle starts the game.":
             "HelixSR OFF (the game's own upscale). Cross switches it ON; Triangle starts the game."):
             !package_ready()?"No HelixSR package for this game yet.":
             edited_desired==AP_ON?"HelixSR ON. Cross switches it OFF; Triangle starts the game.":
             "HelixSR OFF (the game's own upscaler). Cross switches it ON; Triangle starts the game.");
}

static as_session *acquire(void *) { return (as_session *)calloc(1,sizeof(as_session)); }
static void retire(void *,as_session *s) { free(s); }
static int admit(void *,const char *root,const ap_identity *id) {
    if(strcmp(root,settings_root)) return 0;
    struct stat st;
    int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); int sr=fd<0?-1:fstat(fd,&st); if(fd>=0)close(fd); fprintf(logfile,"admit fd=%d stat=%d errno=%d mode=%o owner=%u euid=%u\n",fd,sr,errno,(unsigned)st.st_mode,(unsigned)st.st_uid,(unsigned)geteuid());
    struct stat direct{};int direct_rc=sceKernelStat(root,&direct);fprintf(logfile,"direct_stat=%x uid=%u gid=%u mode=%o size=%zu\n",direct_rc,direct.st_uid,direct.st_gid,direct.st_mode,sizeof direct);
    if(sr||!S_ISDIR(st.st_mode)||(st.st_mode&07777)!=0700||st.st_uid!=0)return 0;
    for(unsigned i=0;i<game_count;++i) if(ps_identity_equal(id,&games[i].id))return 1;
    return 0;
}
static int now(void *,uint64_t *value) {
    timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
    *value=(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;return 1;
}
static void publish(void *,const app_status_view *v,const af_outcome *o) {
    view=*v; fprintf(logfile,"controller=%d session=%d refresh=%d display=%d refusal=%s\n",o->controller_result,controller.session_result,o->refresh_result,o->display_result,v->refusal_text);
    fprintf(logfile,"event=%d result=%d save=%d revision=%llu desired=%d runtime=%d\n",
        o->event,o->result,o->save_result,(unsigned long long)v->saved_revision,
        v->saved_desired,v->reported_observed);
}
static af_result step(af_event *e) {
    af_outcome out{};af_result rc=af_step(&frontend,e,&out);
    if(rc!=AF_OK)snprintf(feedback,sizeof feedback,"Action result %d. %.48s",rc,out.view.refusal_text);
    else if(e->kind==AF_SAVE)snprintf(feedback,sizeof feedback,"Preference saved. Runtime: %.40s",view.runtime_text);
    else snprintf(feedback,sizeof feedback,"%.70s",view.refusal_text);
    return rc;
}
static void select_game(unsigned target) {
    af_event e{};e.kind=AF_SELECT;e.value.identity=games[target].id;
    af_result rc=step(&e);
    selection_valid=rc==AF_OK&&view.identity_known&&ps_identity_equal(&view.identity,&games[target].id);
    if(!selection_valid){
        edited_desired=AP_OFF;preference_dirty=false;
        snprintf(feedback,sizeof feedback,"Game profile could not be selected. Save and launch are disabled.");return;
    }
    selected=target;
    load_preference();
}
static bool save_options() {
    if(!game_count||!selection_valid||!view.identity_known||!ps_identity_equal(&view.identity,&games[selected].id))return false;
    if(preference_dirty){
        af_event e{};e.kind=AF_SAVE;e.value.save.desired=edited_desired;
        e.value.save.quality=view.saved_quality;e.value.save.revision=view.saved_revision;step(&e);
        if(!view.preference_known||view.saved_desired!=edited_desired||view.storage_fault){
            snprintf(feedback,sizeof feedback,"HelixSR ON/OFF could not be saved.");return false;
        }
        preference_dirty=false;
    }
    snprintf(feedback,sizeof feedback,"%s",package_ready()?"Saved. Triangle starts the game with this choice.":
             "Saved. No HelixSR package for this game yet.");return true;
}
static bool init_controller() {
    ac_host ah={settings_root,admit,nullptr,acquire,retire};af_host fh={now,publish,nullptr};
    return ac_init(&controller,&ah)==AC_OK&&af_init(&frontend,&controller,&fh)==AF_OK;
}
static void open_editor();
static void open_frames();
static void queue_framelog(const char *title);
static void cross_action() {
    if(menu==US_MENU_LIBRARY){menu=US_MENU_SETTINGS;focus=FIELD_HELIX;return;}
    if(focus==FIELD_HUD){toggle_hud();return;}
    if(focus==FIELD_RESOLUTION){open_editor();return;}
    if(focus==FIELD_FRAMES){open_frames();return;}
    if(focus==FIELD_HELIX&&hook_switch()){toggle_hook();return;}
    reset_armed=-1;
    if(!selection_valid||!package_ready()){
        snprintf(feedback,sizeof feedback,"%s",!selection_valid?"Launch this game once so GPU Lab can catalogue it.":
                 "HelixSR is not available for this game yet.");return;
    }
    edited_desired=edited_desired==AP_ON?AP_OFF:AP_ON;preference_dirty=edited_desired!=view.saved_desired;
    if(preference_dirty&&!save_options())return;
    snprintf(feedback,sizeof feedback,"%s",edited_desired==AP_ON?"HelixSR ON. Triangle switches the game to HelixSR and starts it.":
             "HelixSR OFF. Triangle starts the game with its own upscaler.");
}
static void select_entry(unsigned target) {
    if(!entry_count)return;
    cursor=target;
    if(entries[target].catalog){select_game(entries[target].index);return;}
    selection_valid=false;edited_desired=AP_OFF;preference_dirty=false;
    snprintf(feedback,sizeof feedback,"Not scanned yet. Triangle launches it and scans once.");
}
static void switch_game(int delta) {
    if(preference_dirty&&!save_options())return;
    select_entry((cursor+entry_count+(delta<0?-1:1))%entry_count);
}
/* Asks the local payload manager to run one HelixSR payload; returns the
 * HTTP status, 0 when the manager could not be reached. */
static unsigned load_payload(const char *name) {
    int socket_fd=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in address{};address.sin_len=sizeof address;address.sin_family=AF_INET;
    address.sin_port=htons(8084);address.sin_addr.s_addr=htonl(0x7f000001);
    timeval timeout{3,0};unsigned code=0;
    if(socket_fd>=0&&!setsockopt(socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout)&&
       !setsockopt(socket_fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout)&&
       !connect(socket_fd,(sockaddr*)&address,sizeof address)){
        char request[256];
        int length=snprintf(request,sizeof request,"POST /loadpayload:/data/pldmgr/payloads/local/helixsr-devloop/%s HTTP/1.1\r\n"
            "Host: 127.0.0.1:8084\r\nX-PLDMGR-Request: 1\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",name);
        size_t sent=0;
        while(length>0&&sent<(size_t)length){ssize_t n=send(socket_fd,request+sent,(size_t)length-sent,0);
            if(n<0&&errno==EINTR)continue;if(n<=0)break;sent+=(size_t)n;}
        char response[128]={0};size_t received=0;
        while(length>0&&sent==(size_t)length&&received<sizeof response-1){
            ssize_t n=recv(socket_fd,response+received,sizeof response-1-received,0);
            if(n<0&&errno==EINTR)continue;if(n<=0)break;received+=(size_t)n;
            if(strchr(response,'\n'))break;
        }
        if(sscanf(response,"HTTP/%*u.%*u %u",&code)!=1)code=0;
    }
    if(socket_fd>=0)close(socket_fd);
    return code;
}
/* mode: 0 launches as is (HLX1); 'H' / 'O' first switch the game to HelixSR / its own upscaler (HLX2). The payload
 * moves the files only after GPU Lab has exited. */
static bool launch_original(uint32_t user,char mode) {
    (void)user;timespec issued{};
    if(!entry_count||clock_gettime(CLOCK_MONOTONIC,&issued))return false;
    const char *title=entry_title(cursor);
    const char *path="/data/helixsr-speed/launch-request.txt";
    const char *stage="/data/helixsr-speed/launch-request.tmp";
    char record[128];int length=mode?snprintf(record,sizeof record,"HLX2 %s %u %llu %c\n",
        title,(unsigned)getpid(),(unsigned long long)issued.tv_sec,mode):snprintf(record,sizeof record,"HLX1 %s %u %llu\n",
        title,(unsigned)getpid(),(unsigned long long)issued.tv_sec);
    // The payload manager runs under a different uid. This request contains
    // only the catalog title, caller pid and a short-lived timestamp.
    int fd=open(stage,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0644);
    bool wrote=fd>=0&&length>0&&length<(int)sizeof record;
    size_t used=0;
    while(wrote&&used<(size_t)length){ssize_t n=write(fd,record+used,(size_t)length-used);
        if(n<0&&errno==EINTR)continue;if(n<=0){wrote=false;break;}used+=(size_t)n;}
    if(fd>=0){if(wrote&&fchmod(fd,0644))wrote=false;if(wrote&&fsync(fd))wrote=false;if(close(fd))wrote=false;}
    if(!wrote||rename(stage,path)){snprintf(feedback,sizeof feedback,"Launch request could not be saved.");return false;}
    // helixsr-launch.elf replaces launch-handoff.elf: the same one-shot request,
    // plus a one-time read-only scan of a game with no finding for this version.
    unsigned code=load_payload("helixsr-launch.elf");bool accepted=code==200;
    fprintf(logfile,"launch_handoff_http=%u accepted=%d\n",code,accepted);
    if(!accepted)unlink(path);
    else queue_framelog(title);
    const bool scans=!entries[cursor].finding.present;
    fprintf(logfile,"launch_queued title=%s accepted=%d scan=%d mode=%c\n",title,accepted,scans,mode?mode:'-');
    snprintf(feedback,sizeof feedback,"%s",!accepted?"Game launch service unavailable. Settings are retained.":
        mode=='H'?"Launch queued. HelixSR is switched on, then the game starts.":
        mode=='O'?"Launch queued. The game's own upscaler is used.":
        scans?"Launch queued. GPU Lab closes; the game is scanned once it runs.":"Launch queued. Closing HelixSR to open the game.");
    return accepted;
}
static bool launch_selected(uint32_t user) {
    if(!entry_count)return false;
    hook_view.loaded=false; // Re-read the current hook setting before deciding how to launch.
    const bool ready=entries[cursor].catalog&&supported_entry(cursor)&&gl_package_ready(helix_root,entry_title(cursor),entry_version(cursor),nullptr,0);   // fresh, not cached
    const int mode=gp_choose_launch(device_model(),entry_title(cursor),entry_version(cursor),entries[cursor].catalog,
                                    ready,hook_info().available,edited_desired==AP_ON);
    if(mode<0){snprintf(feedback,sizeof feedback,"Package metadata or game version mismatch. Repair the installation.");return false;}
    if(!on_installed()&&!save_options())return false;
    return launch_original(user,(char)mode);
}
static void load_catalog_file(const char *path,bool package_record) {
    FILE *f=fopen(path,"r");if(!f)return;
    char line[512];
    while(game_count<64&&fgets(line,sizeof line,f)) {
        char *save=nullptr,*parts[5];unsigned n=0;
        for(char *p=strtok_r(line,"\t\r\n",&save);p&&n<5;p=strtok_r(nullptr,"\t\r\n",&save))parts[n++]=p;
        if(n!=5)continue;
        Game g{};
        if(strlen(parts[0])>=sizeof g.id.device||strlen(parts[1])>=sizeof g.id.title||
           strlen(parts[2])>=sizeof g.id.build||strlen(parts[3])!=64||strlen(parts[4])>=sizeof g.name)continue;
        strcpy(g.id.device,parts[0]);strcpy(g.id.title,parts[1]);strcpy(g.id.build,parts[2]);
        strcpy(g.id.executable_hash,parts[3]);strcpy(g.name,parts[4]);
        if(!gp_accept_catalog(device_model(),g.id.device,g.id.title,g.id.build,package_record))continue;
        bool duplicate=false;
        for(unsigned i=0;i<game_count;++i)
            if(!strcmp(games[i].id.title,g.id.title)&&!strcmp(games[i].id.build,g.id.build))duplicate=true;
        if(!duplicate&&ap_identity_valid(&g.id))games[game_count++]=g;
    }
    fclose(f);
}
static void load_catalog() {
    /* Installer-owned records make a clean install usable without first launching and scanning the game.
     * They do not rewrite the scanner's shared catalog. Each is removed with its own package. */
    load_catalog_file("/data/helixsr-speed/packages/PPSA07631-01.000.000/CATALOG",true);
    load_catalog_file("/data/helixsr-speed/packages/PPSA34547-01.300.300/CATALOG",true);
    load_catalog_file("/data/helixsr-speed/packages/PPSA02572-01.200.007/CATALOG",true);
    load_catalog_file("/data/helixsr-speed/packages/CUSA08809-01.00/CATALOG",true);
    // The installer's exact integration identity takes precedence over an old scan.
    load_catalog_file("/data/helixsr-speed/catalog.tsv",false);
}
static const char *game_name(unsigned i) {
    for(const unsigned char*p=(const unsigned char*)games[i].name;*p;++p)
        if(*p<32||*p>126)return games[i].id.title;
    return games[i].name;
}
static const char *entry_name(unsigned i) {
    if(!entries[i].catalog)return installed[entries[i].index].name;
    for(unsigned j=0;j<installed_count;++j)
        if(!strcmp(installed[j].title,games[entries[i].index].id.title)&&installed[j].name[0])return installed[j].name;
    return game_name(entries[i].index);
}
static const char *game_display_name() {
    if(!entry_count)return "No verified game catalog";
    return entry_name(cursor);
}

/* ---- u07: per-game resolution request -----------------------------------------------------------------------------
 * The request is the game's HRO1 render-options record (render_options.c: identity-scoped, revisioned, sealed) under
 * render_root, created on the first save. It is a request: the launch hand-off is unchanged and HelixSR ON still runs
 * the package on the console. A plain-text copy for the PC package tools goes to requests/TITLE-VERSION.txt. */
static const char *render_root="/data/helixsr-speed/render-settings";
struct RequestView { unsigned cursor; bool loaded,saved; ro_options options; int status; };
static RequestView request_view;
static bool tool_rows_stale;   /* a save or the record toggle changed what the two settings rows show */
static uint64_t now_ms() { timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u; }
static const ap_identity *entry_identity() {
    return entry_count&&entries[cursor].catalog&&selection_valid?&games[entries[cursor].index].id:nullptr;
}
static const RequestView &request_info() {
    RequestView &r=request_view;
    if(!r.loaded||r.cursor!=cursor) {
        r=RequestView{};r.loaded=true;r.cursor=cursor;ro_defaults(&r.options);
        struct stat st{};const ap_identity *id=entry_identity();
        if(id&&!stat(render_root,&st)&&S_ISDIR(st.st_mode)){r.status=ro_load(render_root,id,&r.options);r.saved=r.status==RO_OK;}
    }
    return r;
}
static rp_pair pair_of(const ro_options &o) { return {o.input_width,o.input_height,o.output_width,o.output_height}; }
/* The pairs the console's READY file lists for the selected game. */
static unsigned ready_pairs(rp_pair *out,unsigned capacity) {
    const PackageView &p=package_info();unsigned n=0;
    for(;p.ready&&n<p.pair_count&&n<capacity;++n)out[n]=p.pairs[n];
    return n;
}
static int pair_package(const rp_pair &pair,char *text,size_t size) {
    rp_pair ready[8];const unsigned n=ready_pairs(ready,8);
    for(unsigned i=0;i<n;++i)if(rp_pair_equal(&ready[i],&pair)){
        const PackageView &p=package_info();
        if(p.name_length)snprintf(text,size,"Package on this console: %.*s",(int)p.name_length,p.description);
        else snprintf(text,size,"Package on this console: %s",p.id[0]?p.id:"HelixSR");
        return UE_PACKAGE_READY;
    }
    for(unsigned i=0;const rp_known_pair *k=rp_known(entry_title(cursor),entry_version(cursor),i);++i)
        if(rp_pair_equal(&k->pair,&pair)&&k->package!=RP_PACKAGE_NONE){
            snprintf(text,size,k->package==RP_PACKAGE_BUILT?"%s, not on this console":"%s",k->package_note);
            return k->package==RP_PACKAGE_BUILT?UE_PACKAGE_BUILT:UE_PACKAGE_TRIAL;
        }
    snprintf(text,size,"No HelixSR package for this pair: it is saved as a request only");
    return UE_PACKAGE_NONE;
}
/* A game profile's measured size, for games without known pairs. GPU Lab's built-in CONTROL entry stores the measured
 * 1505x847 rounded to even (1506x848), so for a game with known pairs only those exact pairs count as measured. */
static bool measured_finding() {
    const gl_finding &f=entries[cursor].finding;
    return f.measured&&f.input_width&&f.output_width&&!rp_known(entry_title(cursor),entry_version(cursor),0);
}
static int pair_game(const rp_pair &pair,char *text,size_t size) {
    const char *name=game_display_name();
    for(unsigned i=0;const rp_known_pair *k=rp_known(entry_title(cursor),entry_version(cursor),i);++i)
        if(rp_pair_equal(&k->pair,&pair)){snprintf(text,size,"%s renders at this size in %s mode (measured)",name,k->mode);return 1;}
    const gl_finding &f=entries[cursor].finding;
    if(measured_finding()&&f.input_width==pair.input_width&&f.input_height==pair.input_height&&f.output_width==pair.output_width&&
       f.output_height==pair.output_height){snprintf(text,size,"%.90s",f.measured_basis);return 1;}
    snprintf(text,size,"%s was not measured rendering at %u x %u: it would need a game-side change",name,pair.input_width,pair.input_height);
    return 0;
}
static const char *short_package(int package) {
    return package==UE_PACKAGE_READY?"package ready":package==UE_PACKAGE_BUILT?"package not on console":
           package==UE_PACKAGE_TRIAL?"trial package only":"request only";
}
static int package_tone(int package) {
    return package==UE_PACKAGE_READY?US_TONE_GOOD:package==UE_PACKAGE_NONE?US_TONE_NEUTRAL:US_TONE_WARN;
}
/* The editor. Picks: the console's READY pairs, the known pairs, the measured size, the saved request, then the
 * DLSS-style presets at the edited output. */
struct Pick { char label[72]; rp_pair pair; };
static Pick picks[24];
static unsigned pick_count,edit_row,hold_frames;
static char editor_refusal[80];   /* why the last Cross in the editor did not save, until the next press */
static rp_pair edit_pair;
static void add_pick(const char *label,const rp_pair &pair) {
    for(unsigned i=0;i<pick_count;++i)if(rp_pair_equal(&picks[i].pair,&pair))return;
    if(pick_count<sizeof picks/sizeof *picks){snprintf(picks[pick_count].label,sizeof picks[0].label,"%s",label);picks[pick_count++].pair=pair;}
}
static void build_picks() {
    pick_count=0;rp_pair ready[8];char label[72];const unsigned n=ready_pairs(ready,8);
    for(unsigned i=0;i<n;++i)add_pick("Package on this console",ready[i]);
    for(unsigned i=0;const rp_known_pair *k=rp_known(entry_title(cursor),entry_version(cursor),i);++i){
        snprintf(label,sizeof label,"%s (%s)",k->mode,k->package==RP_PACKAGE_TRIAL?"trial package":k->package==RP_PACKAGE_BUILT?"package built":"measured");
        add_pick(label,k->pair);
    }
    const gl_finding &f=entries[cursor].finding;
    if(measured_finding())add_pick("Measured game size",{f.input_width,f.input_height,f.output_width,f.output_height});
    if(request_info().saved)add_pick("Saved request",pair_of(request_info().options));
    static const struct { unsigned preset; const char *label; } presets[]={{RO_QUALITY,"Quality preset (67%)"},
        {RO_BALANCED,"Balanced preset (58%)"},{RO_PERFORMANCE,"Performance preset (50%)"},{RO_ULTRA_PERFORMANCE,"Ultra Performance preset (33%)"}};
    for(const auto &q:presets){
        ro_options o;ro_defaults(&o);o.output_width=edit_pair.output_width;o.output_height=edit_pair.output_height;
        if(!ro_apply_preset(&o,q.preset))add_pick(q.label,pair_of(o));
    }
}
static const unsigned standard_outputs[][2]={{1280,720},{1600,900},{1920,1080},{2560,1440},{3200,1800},{3840,2160}};
static const char *output_name(unsigned w,unsigned h) {
    static const char *const names[]={"720p","900p","1080p","1440p","1800p","4K"};
    for(unsigned i=0;i<sizeof names/sizeof *names;++i)if(standard_outputs[i][0]==w&&standard_outputs[i][1]==h)return names[i];
    return "Custom";
}
static void open_editor() {
    if(!rich_screen){snprintf(feedback,sizeof feedback,"The resolution editor needs the 1080p screen.");return;}
    if(!entry_identity()){snprintf(feedback,sizeof feedback,"Launch this game once so GPU Lab can catalogue it.");return;}
    const RequestView &r=request_info();rp_pair ready[8];
    if(r.saved)edit_pair=pair_of(r.options);
    else if(ready_pairs(ready,8))edit_pair=ready[0];
    else if(const rp_known_pair *k=rp_known(entry_title(cursor),entry_version(cursor),0))edit_pair=k->pair;
    else if(measured_finding()){const gl_finding &f=entries[cursor].finding;
        edit_pair={f.input_width,f.input_height,f.output_width,f.output_height};}
    else edit_pair=pair_of(r.options);
    edit_row=UE_START;hold_frames=0;editor_refusal[0]=0;build_picks();view_screen=US_SCREEN_EDITOR;
    fprintf(logfile,"editor_open title=%s saved=%d status=%d pair=%ux%u->%ux%u\n",entry_title(cursor),r.saved,r.status,
            edit_pair.input_width,edit_pair.input_height,edit_pair.output_width,edit_pair.output_height);
}
static unsigned clamp_to(long value,unsigned low,unsigned high) { return value<(long)low?low:value>(long)high?high:(unsigned)value; }
static unsigned scaled_size(unsigned output,unsigned permille) { return (unsigned)(((unsigned long long)output*permille+500u)/1000u); }
/* One change on the focused row: direction -1 or +1, big for L1/R1, faster while held. Output sizes stay inside the
 * PS5 limits; render sizes can pass the output so the check can say why that cannot be saved. */
static void editor_change(int direction,bool big,unsigned speed) {
    rp_pair &p=edit_pair;const long d=direction*(long)speed;
    switch(edit_row){
    case UE_START:{
        build_picks();int at=-1;
        for(unsigned i=0;i<pick_count;++i)if(rp_pair_equal(&picks[i].pair,&p))at=(int)i;
        if(pick_count)p=picks[at<0?(direction>0?0u:pick_count-1u):(unsigned)((at+direction+(int)pick_count)%(int)pick_count)].pair;
        break;}
    case UE_OUTPUT:{
        /* The next standard output up or down; the render scale stays. */
        const unsigned count=sizeof standard_outputs/sizeof *standard_outputs;int at=-1;
        for(unsigned i=0;i<count;++i){
            const unsigned area=standard_outputs[i][0]*standard_outputs[i][1],now=p.output_width*p.output_height;
            if(direction>0?area>now&&at<0:area<now)at=(int)i;
        }
        if(at<0)break;
        const unsigned long long sx=p.output_width?(unsigned long long)p.input_width*1000000u/p.output_width:1000000u;
        const unsigned long long sy=p.output_height?(unsigned long long)p.input_height*1000000u/p.output_height:1000000u;
        p.output_width=standard_outputs[at][0];p.output_height=standard_outputs[at][1];
        p.input_width=(unsigned)((p.output_width*sx+500000u)/1000000u);p.input_height=(unsigned)((p.output_height*sy+500000u)/1000000u);
        break;}
    case UE_OUTPUT_WIDTH:p.output_width=clamp_to((long)p.output_width+d*(big?64:8),RP_MIN_OUTPUT_WIDTH,RP_MAX_OUTPUT_WIDTH);break;
    case UE_OUTPUT_HEIGHT:p.output_height=clamp_to((long)p.output_height+d*(big?64:8),RP_MIN_OUTPUT_HEIGHT,RP_MAX_OUTPUT_HEIGHT);break;
    case UE_SCALE:{
        const long percent=p.output_width?(long)(((unsigned long long)p.input_width*100u+p.output_width/2u)/p.output_width):100;
        const unsigned next=clamp_to(percent+d*(big?5:1),25,100)*10u;
        p.input_width=scaled_size(p.output_width,next);p.input_height=scaled_size(p.output_height,next);
        break;}
    case UE_INPUT_WIDTH:p.input_width=clamp_to((long)p.input_width+d*(big?16:1),RP_MIN_INPUT_WIDTH,RP_MAX_OUTPUT_WIDTH);break;
    case UE_INPUT_HEIGHT:p.input_height=clamp_to((long)p.input_height+d*(big?16:1),RP_MIN_INPUT_HEIGHT,RP_MAX_OUTPUT_HEIGHT);break;
    }
}
static void export_request(const ap_identity &id,const ro_options &o) {
    mkdir("/data/helixsr-speed/requests",0755);
    char path[160],text[320];
    snprintf(path,sizeof path,"/data/helixsr-speed/requests/%s-%s.txt",id.title,id.build);
    const int n=snprintf(text,sizeof text,"HREQ1\ntitle=%s\nversion=%s\nexecutable=%s\ninput=%ux%u\noutput=%ux%u\nrevision=%llu\n"
        "note=Export of GPU Lab's saved request (render-settings HRO1 record); a request, not a package\n",id.title,id.build,
        id.executable_hash,o.input_width,o.input_height,o.output_width,o.output_height,(unsigned long long)o.revision);
    FILE *f=n>0&&n<(int)sizeof text?fopen(path,"w"):nullptr;
    bool ok=f&&fwrite(text,1,(size_t)n,f)==(size_t)n;
    if(f&&fclose(f))ok=false;
    fprintf(logfile,"request_export path=%s ok=%d\n",path,ok);
}
static void save_request() {
    rp_check check;rp_check_pair(&edit_pair,&check);
    const ap_identity *id=entry_identity();
    if(!id){snprintf(feedback,sizeof feedback,"Resolution not saved: this game has no profile.");view_screen=US_SCREEN_MAIN;return;}
    if(!check.valid){snprintf(feedback,sizeof feedback,"Not saved: %.66s",check.problem);snprintf(editor_refusal,sizeof editor_refusal,"%s",feedback);return;}
    mkdir(render_root,0700);
    ro_options o=request_info().options;
    o.input_width=edit_pair.input_width;o.input_height=edit_pair.input_height;
    o.output_width=edit_pair.output_width;o.output_height=edit_pair.output_height;o.preset=RO_CUSTOM;o.backend=RO_HELIXSR;
    int rc=ro_save(render_root,id,&o,o.revision);
    if(rc==RO_CONFLICT){request_view.loaded=false;o.revision=request_info().options.revision;rc=ro_save(render_root,id,&o,o.revision);}
    char text[96];const int package=pair_package(edit_pair,text,sizeof text);
    fprintf(logfile,"resolution_save title=%s input=%ux%u output=%ux%u rc=%d revision=%llu package=%d\n",id->title,o.input_width,
            o.input_height,o.output_width,o.output_height,rc,(unsigned long long)o.revision,package);
    if(rc){snprintf(feedback,sizeof feedback,"The resolution request could not be saved (error %d).",rc);snprintf(editor_refusal,sizeof editor_refusal,"%s",feedback);return;}
    export_request(*id,o);
    request_view.loaded=false;tool_rows_stale=true;view_screen=US_SCREEN_MAIN;
    snprintf(feedback,sizeof feedback,"Saved %u x %u -> %u x %u: %s.",o.input_width,o.input_height,o.output_width,o.output_height,
             package==UE_PACKAGE_READY?"HelixSR package ready":package==UE_PACKAGE_NONE?"request only, no package":short_package(package));
}
static void editor_input(uint32_t press,uint32_t buttons) {
    const uint32_t sideways=buttons&(PS5_PAD_BUTTON_LEFT|PS5_PAD_BUTTON_RIGHT);
    if(press)editor_refusal[0]=0;
    hold_frames=sideways&&!(press&sideways)?hold_frames+1:0;
    if(press&(PS5_PAD_BUTTON_UP|PS5_PAD_BUTTON_DOWN))
        edit_row=(edit_row+UE_ROW_COUNT+(press&PS5_PAD_BUTTON_UP?UE_ROW_COUNT-1:1))%UE_ROW_COUNT;
    else if(press&sideways||(hold_frames>=20&&!((hold_frames-20)%3)))
        editor_change(sideways&PS5_PAD_BUTTON_RIGHT?1:-1,false,hold_frames>=110?4u:1u);
    else if(press&PS5_PAD_BUTTON_L1)editor_change(-1,true,1);
    else if(press&PS5_PAD_BUTTON_R1)editor_change(1,true,1);
    else if(press&PS5_PAD_BUTTON_CROSS)save_request();
    else if(press&(PS5_PAD_BUTTON_CIRCLE|PS5_PAD_BUTTON_OPTIONS)){view_screen=US_SCREEN_MAIN;snprintf(feedback,sizeof feedback,"Resolution not changed.");}
}
static void fill_editor(us_editor *e) {
    e->row=edit_row;e->pair=edit_pair;rp_check_pair(&edit_pair,&e->check);
    snprintf(e->start,sizeof e->start,"Custom");
    for(unsigned i=0;i<pick_count;++i)if(rp_pair_equal(&picks[i].pair,&edit_pair)){snprintf(e->start,sizeof e->start,"%s",picks[i].label);break;}
    snprintf(e->output_name,sizeof e->output_name,"%s",output_name(edit_pair.output_width,edit_pair.output_height));
    e->package=pair_package(edit_pair,e->package_text,sizeof e->package_text);
    e->game_measured=pair_game(edit_pair,e->game_text,sizeof e->game_text);
    const RequestView &r=request_info();const rp_pair saved=pair_of(r.options);
    e->saved=r.saved&&rp_pair_equal(&edit_pair,&saved);
    if(r.saved)snprintf(e->saved_text,sizeof e->saved_text,"%s %u x %u -> %u x %u (revision %llu)",e->saved?"Saved request:":
        "Currently saved:",saved.input_width,saved.input_height,saved.output_width,saved.output_height,(unsigned long long)r.options.revision);
    else if(r.status<0)snprintf(e->saved_text,sizeof e->saved_text,"The saved request could not be read; saving replaces it");
    else snprintf(e->saved_text,sizeof e->saved_text,"Nothing saved for this game yet");
    if(editor_refusal[0]){snprintf(e->saved_text,sizeof e->saved_text,"%s",editor_refusal);e->refused=1;}
    const PackageView &p=package_info();
    if(p.ready&&!strcmp(entry_title(cursor),"PPSA07631"))snprintf(e->launch_text,sizeof e->launch_text,"Triangle starts SAROS: ON reconstructs to 4K with game-controlled render size; OFF uses the game upscaler. Sizes above are observed examples. Saving a request does not change the game.");
    else if(p.ready&&p.pair_count>1)snprintf(e->launch_text,sizeof e->launch_text,"Triangle runs what is installed: HelixSR ON starts the "
        "package on the console, which runs at the %u layouts above marked ready; OFF the game's own upscaler. Saving does not "
        "change that.",p.pair_count);
    else if(p.ready&&p.input_width)snprintf(e->launch_text,sizeof e->launch_text,"Triangle runs what is installed: HelixSR ON starts the "
        "package on the console (%u x %u -> %u x %u), OFF the game's own upscaler. Saving does not change that.",
        p.input_width,p.input_height,p.output_width,p.output_height);
    else snprintf(e->launch_text,sizeof e->launch_text,"Triangle starts the game as it is: there is no HelixSR package for it "
        "on the console. Saving does not change that.");
}

/* ---- u07: frame-rate logs ----------------------------------------------------------------------------------------
 * HFPS1 logs in fps/TITLE-S.txt (frame_log.h), written by the frame logger payload or the PC strip converter. The
 * arrays are allocated only while the screen is open. Recording is per game: fps/TITLE.record. */
static const char *fps_root="/data/helixsr-speed/fps";
/* This bundle ships launch and inventory payloads only. Enable recording only
 * with a qualified, packaged frame logger; a stale .record file is not enough. */
static constexpr bool recorder_in_build=false;
static bool recorder_available(const char *title,const char *version) {
    return recorder_in_build&&fl_recorder_known(title,version);
}
struct FrameLogs { uint32_t *frame_us,*helix_us,*scratch; uint16_t *repeats; uint8_t *flags;
    unsigned slot[FL_SLOTS],count; char started[FL_SLOTS][40]; };
static FrameLogs frame_logs;
static us_frames frames_view;
static void record_path(char *out,size_t size,const char *title) { snprintf(out,size,"%s/%s.record",fps_root,title); }
static bool record_on(const char *title) { char path[96];struct stat st{};record_path(path,sizeof path,title);return !stat(path,&st); }
static void ensure_fps_dir() { mkdir(fps_root,0777);chmod(fps_root,0777); }
/* The newest-first list of this title's logs, from their headers. */
static unsigned list_logs(const char *title,unsigned *slot,char (*started)[40]) {
    unsigned n=0;
    for(unsigned s=1;s<=FL_SLOTS;++s){
        char path[160];fl_log head{};
        if(fl_slot_path(path,sizeof path,helix_root,title,s)||fl_read(path,&head))continue;
        unsigned at=n++;
        while(at&&strcmp(started[at-1],head.started)<0){slot[at]=slot[at-1];memcpy(started[at],started[at-1],40);--at;}
        slot[at]=s;snprintf(started[at],40,"%s",head.started);
    }
    return n;
}
static void logger_status(char *out,size_t size,const char *title) {
    char text[512]="",path[96];snprintf(path,sizeof path,"%s/framelog-status.txt",fps_root);out[0]=0;
    FILE *f=fopen(path,"r");if(!f)return;
    size_t n=fread(text,1,sizeof text-1,f);fclose(f);text[n]=0;
    char t[16]="",state[24]="",reason[96]="",frames[16]="";
    for(char *line=strtok(text,"\n");line;line=strtok(nullptr,"\n")){
        if(!strncmp(line,"title=",6))snprintf(t,sizeof t,"%.15s",line+6);
        else if(!strncmp(line,"state=",6))snprintf(state,sizeof state,"%.23s",line+6);
        else if(!strncmp(line,"reason=",7))snprintf(reason,sizeof reason,"%.95s",line+7);
        else if(!strncmp(line,"frames=",7))snprintf(frames,sizeof frames,"%.15s",line+7);
    }
    if(!strcmp(t,title))snprintf(out,size,"Last frame logger run: %s (%s), %s frames.",state,reason,frames[0]?frames:"0");
}
static void frames_status() {
    char last[160];logger_status(last,sizeof last,entry_title(cursor));
    const bool available=recorder_available(entry_title(cursor),entry_version(cursor)),on=available&&record_on(entry_title(cursor));
    snprintf(frames_view.status,sizeof frames_view.status,"%s%sRecord next launch: %s.",last,last[0]?"  ":"",
             !recorder_in_build?"recorder not included in this build":!available?"not available for this game":on?"On":"Off");
    frames_view.record_on=on;frames_view.recorder_available=available;
}
static void load_frames_view() {
    FrameLogs &L=frame_logs;us_frames &v=frames_view;
    const unsigned index=v.log_index;v=us_frames{};v.log_index=index;v.log_count=L.count;
    snprintf(v.game,sizeof v.game,"%s",game_display_name());
    frames_status();
    if(!L.count)return;
    if(!L.frame_us){v.state=2;snprintf(v.error,sizeof v.error,"Not enough memory to read a log.");return;}
    char path[160];fl_slot_path(path,sizeof path,helix_root,entry_title(cursor),L.slot[index]);
    fl_log log{};log.frame_us=L.frame_us;log.helix_us=L.helix_us;log.repeats=L.repeats;log.flags=L.flags;log.capacity=FL_MAX_FRAMES;
    const int rc=fl_read(path,&log);
    if(rc){v.state=2;snprintf(v.error,sizeof v.error,"The file is not an HFPS1 log (%d).",rc);return;}
    if(fl_compute(&log,&v.stats,L.scratch)){v.state=2;snprintf(v.error,sizeof v.error,"The log has no frame with a known time.");return;}
    v.state=1;v.engine_source=log.source==FL_SOURCE_ENGINE;v.frame_time=(int)log.frame_time;v.missed_measured=(int)log.missed_measured;
    v.incomplete=!log.complete;
    snprintf(v.started,sizeof v.started,"%s",log.started);
    snprintf(v.source,sizeof v.source,"%s",log.source==FL_SOURCE_ENGINE?"Game frame counter (live)":
             log.source==FL_SOURCE_GPU_STRIP?"HelixSR GPU timer (recording)":
             log.source==FL_SOURCE_RENDER_TRACE?"Render callback trace (imported)":"Unknown source");
    /* FPS per second, averaged into at most US_FPS_POINTS points. */
    static uint16_t seconds[7200];const unsigned n=fl_fps_series(&log,seconds,7200);
    v.seconds_per_point=n>US_FPS_POINTS?(n+US_FPS_POINTS-1)/US_FPS_POINTS:1;
    for(unsigned i=0;i<n;i+=v.seconds_per_point){
        unsigned sum=0,k=0;for(unsigned j=i;j<n&&j<i+v.seconds_per_point;++j,++k)sum+=seconds[j];
        v.fps[v.fps_count++]=(uint16_t)(sum/k);
    }
    fl_columns(&log,US_TIME_COLUMNS,v.shortest,v.longest);
    v.play_us=v.stats.total_us;
    char sizes[64]="",package[64]="";
    if(log.render_width&&log.output_width)snprintf(sizes,sizeof sizes,"  \x83  %u x %u -> %u x %u",log.render_width,log.render_height,
                                                   log.output_width,log.output_height);
    if(log.package[0])snprintf(package,sizeof package," (%s)",log.package);
    snprintf(v.session,sizeof v.session,"%s  \x83  %llu.%llu s of play  \x83  %u frames  \x83  HelixSR %s%s%s",log.started,
             v.stats.total_us/1000000u,v.stats.total_us/100000u%10u,v.stats.frames,log.helix[0]?log.helix:"unknown",package,sizes);
    if(log.source==FL_SOURCE_RENDER_TRACE)snprintf(v.basis[0],sizeof v.basis[0],"Frame times: consecutive SAROS render-callback "
        "CPU timestamps, calibrated against the PS5 monotonic clock. Imported from a verified native trace. "
        "This measures callback cadence, not GPU duration or displayed FPS.");
    else if(log.source==FL_SOURCE_ENGINE)snprintf(v.basis[0],sizeof v.basis[0],"Frame times: the game's own delta for each frame, read from "
        "its engine block every 2 ms by the HelixSR frame logger (read-only). That counter's and delta's meaning is inferred "
        "from the game's 30 and 60 FPS modes.");
    else snprintf(v.basis[0],sizeof v.basis[0],"Frame times: GPU time from one HelixSR block start to the next (one per game frame), "
        "from the in-game timer strip in a 60 Hz recording, decoded on the PC. Clock %s.",
        log.frame_time==FL_TIME_CALIBRATED?"calibrated so the median frame is one refresh":"at its nominal 100 MHz (an estimate, about 3%)");
    if(log.source==FL_SOURCE_RENDER_TRACE)snprintf(v.basis[1],sizeof v.basis[1],"Display refreshes were not captured. "
        "This trace cannot determine repeated refreshes, display latency, or displayed FPS.");
    else if(log.missed_measured)snprintf(v.basis[1],sizeof v.basis[1],"Missed refreshes: counted in the recording (the same game frame on "
        "two refreshes in a row).");
    else snprintf(v.basis[1],sizeof v.basis[1],"Missed refreshes: an estimate from frame times against the game's usual cadence "
        "(%u refresh%s per frame at 59.94 Hz, assumed), not read from the display.%s",v.stats.target_refreshes,
        v.stats.target_refreshes==1?"":"es",v.stats.mode_changes?" The game changed mode in this log; "
        "both modes count against it.":"");
    if(v.stats.helix_frames)snprintf(v.basis[2],sizeof v.basis[2],"HelixSR block: its GPU time inside each frame (%u frames). It is part "
        "of the frame time, never a frame rate.",v.stats.helix_frames);
    else snprintf(v.basis[2],sizeof v.basis[2],"Left out: %u pause%s over 250 ms (loading, menus). Estimated (sampler late): %u frame%s.%s",
        v.stats.pauses,v.stats.pauses==1?"":"s",v.stats.estimated,v.stats.estimated==1?"":"s",log.complete?"":" The log is incomplete.");
    fprintf(logfile,"frames_view slot=%u frames=%u avg_milli=%u low1=%u missed=%u measured=%d source=%u\n",L.slot[index],v.stats.frames,
            v.stats.average_fps_milli,v.stats.low1_fps_milli,v.stats.missed,v.missed_measured,log.source);
}
static void close_frames() {
    FrameLogs &L=frame_logs;
    free(L.frame_us);free(L.helix_us);free(L.scratch);free(L.repeats);free(L.flags);L=FrameLogs{};view_screen=US_SCREEN_MAIN;
}
static void open_frames() {
    if(!entry_count)return;
    if(!rich_screen){snprintf(feedback,sizeof feedback,"The frame-rate screen needs the 1080p screen.");return;}
    FrameLogs &L=frame_logs;
    if(!L.frame_us){
        L.frame_us=(uint32_t*)malloc(FL_MAX_FRAMES*4);L.helix_us=(uint32_t*)malloc(FL_MAX_FRAMES*4);L.scratch=(uint32_t*)malloc(FL_MAX_FRAMES*4);
        L.repeats=(uint16_t*)malloc(FL_MAX_FRAMES*2);L.flags=(uint8_t*)malloc(FL_MAX_FRAMES);
        if(!L.frame_us||!L.helix_us||!L.scratch||!L.repeats||!L.flags)close_frames();
    }
    L.count=list_logs(entry_title(cursor),L.slot,L.started);
    frames_view.log_index=0;load_frames_view();view_screen=US_SCREEN_FRAMES;
    fprintf(logfile,"frames_open title=%s logs=%u memory=%d\n",entry_title(cursor),L.count,L.frame_us!=nullptr);
}
static void toggle_record() {
    const char *title=entry_title(cursor);char path[96];record_path(path,sizeof path,title);
    if(!recorder_in_build){snprintf(feedback,sizeof feedback,"Recorder unavailable in this build. Existing logs can still be viewed.");return;}
    if(!fl_recorder_known(title,entry_version(cursor))){snprintf(feedback,sizeof feedback,"The frame logger does not know this game.");return;}
    const bool on=!record_on(title);bool ok;
    if(on){ensure_fps_dir();FILE *f=fopen(path,"w");ok=f&&fputs("HFLA1\n",f)>=0;if(f&&fclose(f))ok=false;}
    else ok=!unlink(path);
    fprintf(logfile,"record_toggle title=%s on=%d ok=%d\n",title,on,ok);tool_rows_stale=true;
    frames_status();
}
static void frames_input(uint32_t press,uint32_t user,bool *quit) {
    FrameLogs &L=frame_logs;
    if(press&(PS5_PAD_BUTTON_LEFT|PS5_PAD_BUTTON_RIGHT)){
        if(L.count>1){frames_view.log_index=(frames_view.log_index+L.count+(press&PS5_PAD_BUTTON_RIGHT?L.count-1:1))%L.count;load_frames_view();}
    }
    else if(press&PS5_PAD_BUTTON_SQUARE)toggle_record();
    else if(press&PS5_PAD_BUTTON_TRIANGLE){close_frames();*quit=launch_selected(user);}
    else if(press&(PS5_PAD_BUTTON_CIRCLE|PS5_PAD_BUTTON_OPTIONS))close_frames();
}
/* After an accepted launch: when recording is on for this game, hand the session to the frame logger payload. */
static void queue_framelog(const char *title) {
    if(!recorder_available(title,entry_version(cursor))||!record_on(title))return;
    timespec issued{};clock_gettime(CLOCK_MONOTONIC,&issued);
    char text[64],path[96];const int n=snprintf(text,sizeof text,"HFLR1 %s %u %llu\n",title,(unsigned)getpid(),(unsigned long long)issued.tv_sec);
    snprintf(path,sizeof path,"%s/request.txt",fps_root);ensure_fps_dir();
    FILE *f=fopen(path,"w");bool wrote=f&&fwrite(text,1,(size_t)n,f)==(size_t)n;
    if(f&&fclose(f))wrote=false;
    if(wrote)chmod(path,0644);
    const unsigned code=wrote?load_payload("helixsr-framelog.elf"):0;
    fprintf(logfile,"framelog_queued title=%s wrote=%d http=%u\n",title,wrote,code);
}
/* The two new settings rows on the main screen, re-read with the selection or every second. */
struct ToolRows { unsigned cursor; uint64_t stamp_ms; bool loaded; char resolution[96],frames[96]; int resolution_tone,frames_tone; };
static ToolRows tool_rows;
static const ToolRows &tool_info() {
    ToolRows &t=tool_rows;
    if(!t.loaded||t.cursor!=cursor||tool_rows_stale||now_ms()-t.stamp_ms>1000u){
        tool_rows_stale=false;t=ToolRows{};t.loaded=true;t.cursor=cursor;t.stamp_ms=now_ms();
        if(entry_count&&entries[cursor].catalog){
            const RequestView &r=request_info();char text[96];
            if(r.saved){
                const rp_pair p=pair_of(r.options);const int package=pair_package(p,text,sizeof text);
                snprintf(t.resolution,sizeof t.resolution,"%u \x80 %u -> %u \x80 %u, %s",p.input_width,p.input_height,
                         p.output_width,p.output_height,short_package(package));
                t.resolution_tone=package_tone(package);
            } else snprintf(t.resolution,sizeof t.resolution,"Not set: choose a render and output size");
        }
        if(entry_count){
            unsigned slot[FL_SLOTS];char started[FL_SLOTS][40];const unsigned n=list_logs(entry_title(cursor),slot,started);
            const bool rec=recorder_available(entry_title(cursor),entry_version(cursor))&&record_on(entry_title(cursor));
            if(n)snprintf(t.frames,sizeof t.frames,"%u log%s, last %.10s%s",n,n==1?"":"s",started[0],rec?", recording on":"");
            else snprintf(t.frames,sizeof t.frames,"%s",rec?"No log yet, recording on next launch":"No log yet");
            t.frames_tone=n?US_TONE_GOOD:rec?US_TONE_WARN:US_TONE_NONE;
        }
    }
    return t;
}
/* The 1080p screen shows the same state and strings as the text lines. */
static void build_model(us_model *m,bool connected,const char *status,const char *detail) {
    memset(m,0,sizeof *m);
    m->game_count=entry_count;m->selected=cursor;m->menu=menu;m->focus=focus;
    const PackageView &p=package_info();
    for(unsigned i=0;i<entry_count;++i){
        us_game *g=&m->games[i];
        snprintf(g->name,sizeof g->name,"%s",entry_name(i));
        snprintf(g->title,sizeof g->title,"%s",entry_title(i));
        snprintf(g->build,sizeof g->build,"%s",entry_version(i));
        snprintf(g->upscaler,sizeof g->upscaler,"%s",entries[i].finding.primary);
        g->kind=!entries[i].catalog?US_INSTALLED:entries[i].finding.present?US_KNOWN:US_UNSCANNED;
        g->ready=entry_ready[i];
    }
    m->blank=!entry_count||on_installed();
    if(entry_count){
        const Entry &e=entries[cursor];gl_route route;
        gl_recommend(entry_title(cursor),entry_version(cursor),&e.finding,&route);
        m->scan_launch=!e.finding.present;
        m->scan_only=e.catalog&&route.scan_only;
        snprintf(m->engine,sizeof m->engine,"%s",e.finding.engine);
        snprintf(m->basis,sizeof m->basis,"%s",e.finding.present?e.finding.basis:"");
        snprintf(m->route,sizeof m->route,"%s",route.route);
        snprintf(m->route_basis,sizeof m->route_basis,"%s",route.basis);
        m->hook_ready=p.ready;
        snprintf(m->hook,sizeof m->hook,"%s",gl_hook_status(entry_title(cursor),&e.finding,p.ready,&m->hook_in_development));
        if(p.ready){
            snprintf(m->route,sizeof m->route,"Replace %s with HelixSR",e.finding.present?e.finding.primary:"the game's upscaler");
            snprintf(m->route_basis,sizeof m->route_basis,p.last?"Last start: %s":"%s",p.last?p.reason:p.description);
            snprintf(m->package,sizeof m->package,"%.*s",p.name_length?(int)p.name_length:(int)sizeof p.description,p.description);
            m->input_width=p.input_width;m->input_height=p.input_height;m->output_width=p.output_width;m->output_height=p.output_height;
            if(p.pair_count>1){m->input2_width=p.pairs[1].input_width;m->input2_height=p.pairs[1].input_height;
                m->output2_width=p.pairs[1].output_width;m->output2_height=p.pairs[1].output_height;}
        }
    }
    const HudView &hud=hud_info();
    m->hud_available=hud.available;m->hud_on=hud.on;
    m->pro=is_pro();m->controller=connected;m->settings_ok=selection_valid;
    m->unsaved=preference_dirty;m->helix_on=!m->scan_only&&edited_desired==AP_ON;m->reset_armed=reset_armed;
    if(hook_switch()){m->hook_ready=1;m->helix_on=hook_info().on;}   /* u10: the game's hook runs HelixSR */
    // The card is titled Status; drop the line's own prefix.
    if(!strncmp(status,"Status: ",8))status+=8;
    snprintf(m->status,sizeof m->status,"%s",status);
    if(m->status[0]>='a'&&m->status[0]<='z')m->status[0]=(char)(m->status[0]-32);
    snprintf(m->detail,sizeof m->detail,"%s",m->scan_only&&m->settings_ok?m->route_basis:detail);
    snprintf(m->feedback,sizeof m->feedback,"%s",feedback);
    m->screen=view_screen;
    if(entry_count){const ToolRows &t=tool_info();
        snprintf(m->resolution_value,sizeof m->resolution_value,"%s",t.resolution);m->resolution_tone=t.resolution_tone;
        snprintf(m->frames_value,sizeof m->frames_value,"%s",t.frames);m->frames_tone=t.frames_tone;}
    if(view_screen==US_SCREEN_EDITOR)fill_editor(&m->editor);
    else if(view_screen==US_SCREEN_FRAMES)m->frames=frames_view;
}
int main(int argc,char **argv) {
    int next_encoder=0,model_mode=0,large_model_mode=0,canary_stage=0,saturation_copies=-1;bool run_k10=false,pad_observe=false,run_timestamp=false;
    bool exit_check=false,run_probe=false,launch_check=false,run_encoder=false; for(int i=0;i<argc;++i){if(argv[i]&&!strcmp(argv[i],"--exit-check"))exit_check=true;if(argv[i]&&!strcmp(argv[i],"--psbc-probe"))run_probe=true;if(argv[i]&&!strcmp(argv[i],"--launch-original-check"))launch_check=true;if(argv[i]&&!strcmp(argv[i],"--encoder-probe"))run_encoder=true;if(argv[i]&&!strcmp(argv[i],"--encoder-next-1"))next_encoder=1;if(argv[i]&&!strcmp(argv[i],"--encoder-next-2"))next_encoder=2;}
    for(int i=0;i<argc;++i){if(argv[i]&&!strcmp(argv[i],"--model-prefix"))model_mode=1;if(argv[i]&&!strcmp(argv[i],"--model-sequence"))model_mode=2;if(argv[i]&&!strcmp(argv[i],"--model-collect"))model_mode=3;if(argv[i]&&!strcmp(argv[i],"--model-timed"))model_mode=4;if(argv[i]&&!strcmp(argv[i],"--model-inputs"))model_mode=5;}
    for(int i=0;i<argc;++i)if(argv[i]&&!strcmp(argv[i],"--k10-scratch"))run_k10=true;
    for(int i=0;i<argc;++i)if(argv[i]&&!strcmp(argv[i],"--gpu-timestamp"))run_timestamp=true;
    for(int i=0;i<argc;++i){if(argv[i]&&!strcmp(argv[i],"--enc2-saturation-baseline"))saturation_copies=0;if(argv[i]&&!strcmp(argv[i],"--enc2-saturation-n1"))saturation_copies=1;if(argv[i]&&!strcmp(argv[i],"--enc2-saturation-n8"))saturation_copies=8;if(argv[i]&&!strcmp(argv[i],"--enc2-saturation-n32"))saturation_copies=32;}
    for(int i=0;i<argc;++i){if(argv[i]&&!strcmp(argv[i],"--model-large-sequence"))large_model_mode=1;if(argv[i]&&!strcmp(argv[i],"--model-large-collect"))large_model_mode=2;if(argv[i]&&!strcmp(argv[i],"--model-large-timed"))large_model_mode=3;if(argv[i]&&!strcmp(argv[i],"--model-large-inputs"))large_model_mode=4;if(argv[i]&&!strcmp(argv[i],"--model-large-frame-timed"))large_model_mode=5;}
    for(int i=0;i<argc;++i)if(argv[i]&&!strcmp(argv[i],"--pad-observe"))pad_observe=true;
    for(int i=0;i<argc;++i){if(argv[i]&&!strcmp(argv[i],"--scratch-canary-1"))canary_stage=1;if(argv[i]&&!strcmp(argv[i],"--scratch-canary-2"))canary_stage=2;if(argv[i]&&!strcmp(argv[i],"--scratch-canary-3"))canary_stage=3;}
    logfile=fopen("/data/helixsr-speed/control-app.txt","w");if(!logfile)return 2;
    setbuf(logfile,nullptr);fprintf(logfile,"HELIXSR_NATIVE_CONTROL_V1 pid=%d\n",getpid());
    if(saturation_copies>=0){FILE *p=fopen("/data/helixsr-speed/enc2-saturation.txt","w");int rc=enc2_saturation_native_run(p,saturation_copies);if(p)fclose(p);fprintf(logfile,"enc2_saturation_copies=%d rc=%d\n",saturation_copies,rc);if(rc)for(;;)sleep(1);}
    if(large_model_mode){const char *mode=large_model_mode==1?"sequence":large_model_mode==2?"collect":large_model_mode==3?"timed":large_model_mode==5?"frame-timed":"inputs";char path[128];snprintf(path,sizeof path,"/data/helixsr-speed/model-large-%s.txt",mode);FILE *p=fopen(path,"w");int rc=model_large_native_run(p,large_model_mode);if(p)fclose(p);fprintf(logfile,"model_large_mode=%d rc=%d\n",large_model_mode,rc);if(rc)for(;;)sleep(1);}
    if(run_timestamp){FILE *p=fopen("/data/helixsr-speed/gpu-timestamp.txt","w");int rc=gpu_timestamp_native_run(p);if(p)fclose(p);fprintf(logfile,"gpu_timestamp_rc=%d\n",rc);if(rc)for(;;)sleep(1);}
    if(canary_stage){char path[96];snprintf(path,sizeof path,"/data/helixsr-speed/scratch-canary-%d.txt",canary_stage);FILE *p=fopen(path,"w");int rc=scratch_canary_native_run(p,canary_stage);if(p)fclose(p);fprintf(logfile,"scratch_canary_stage=%d rc=%d\n",canary_stage,rc);if(rc)for(;;)sleep(1);}
    if(run_k10){FILE *p=fopen("/data/helixsr-speed/k10-scratch.txt","w");int rc=k10_scratch_native_run(p);if(p)fclose(p);fprintf(logfile,"k10_scratch_rc=%d\n",rc);if(rc)for(;;)sleep(1);}
    if(model_mode){FILE *p=fopen(model_mode==1?"/data/helixsr-speed/model-prefix.txt":model_mode==3?"/data/helixsr-speed/model-collect.txt":model_mode==4?"/data/helixsr-speed/model-timed.txt":model_mode==5?"/data/helixsr-speed/model-inputs.txt":"/data/helixsr-speed/model-sequence.txt","w");int rc=model_sequence_probe_run(p,model_mode>=3?model_mode-1:model_mode==1);if(p)fclose(p);fprintf(logfile,"model_sequence_mode=%d rc=%d\n",model_mode,rc);if(rc)for(;;)sleep(1);}
    if(run_encoder){FILE *p=fopen("/data/helixsr-speed/encoder-probe.txt","w");int rc=encoder_probe_run(p);if(p)fclose(p);fprintf(logfile,"encoder_probe_rc=%d\n",rc);if(rc)for(;;)sleep(1);}
    if(next_encoder){FILE *p=fopen(next_encoder==1?"/data/helixsr-speed/enc1-probe.txt":"/data/helixsr-speed/enc2-probe.txt","w");int rc=encoder_next_probe_run(p,next_encoder);if(p)fclose(p);fprintf(logfile,"encoder_next_stage=%d rc=%d\n",next_encoder,rc);if(rc)for(;;)sleep(1);}
    if(run_probe){FILE *p=fopen("/data/helixsr-speed/psbc-probe.txt","w");int rc=psbc_probe_run(p);if(p)fclose(p);fprintf(logfile,"psbc_probe_rc=%d\n",rc);if(rc)for(;;)sleep(1);}
    helix_native_storage_check(logfile);
    mkdir(settings_root,0700);load_catalog();
    if(game_count)helix_native_profile_check(logfile,&games[0].id);
    if(!init_controller())return 3;
    // The inventory payload's last list; it is refreshed below once the UI runs.
    const char *installed_path="/data/helixsr-speed/installed.tsv";
    installed_count=gl_load_installed(installed_path,installed,GL_MAX_GAMES);build_entries();
    if(entry_count)select_entry(cursor);
    uint32_t user=0;int user_rc=sceUserServiceInitialize(nullptr);
    int pad=-1,pad_owned=0;
    if((user_rc==0||user_rc==(int)0x80960001)&&sceUserServiceGetForegroundUser(&user)==0&&scePadInit()==0) {
        pad=scePadOpen((int)user,PS5_PAD_PORT_TYPE_STANDARD,0,nullptr);
        if(pad>=0)pad_owned=1;
        else if((uint32_t)pad==PS5_PAD_ERROR_ALREADY_OPENED)pad=scePadGetHandle((int)user,0,0);
    }
    fprintf(logfile,"catalog=%u user=%u pad=%d owned=%d\n",game_count,user,pad,pad_owned);
    if(pad_observe){
        /* Observe only this app's existing handle. No UI actions or saves. */
        struct timespec started{};clock_gettime(CLOCK_MONOTONIC,&started);
        for(unsigned sample=0;sample<2250;++sample){
            struct timespec elapsed{};clock_gettime(CLOCK_MONOTONIC,&elapsed);
            if(elapsed.tv_sec-started.tv_sec>=45)break;
            struct {PS5_PadData data;unsigned char reserve[256];} packet{};
            int pr=pad>=0?scePadReadState(pad,&packet.data):-1;
            struct timeval now{};gettimeofday(&now,nullptr);
            fprintf(logfile,"PAD_OBSERVE sample=%u time=%lld.%06ld result=%d connected=%u buttons=%08x stamp=%llu\n",sample,(long long)now.tv_sec,(long)now.tv_usec,pr,packet.data.connected,packet.data.buttons,(unsigned long long)packet.data.timestamp);
            usleep(20000);
        }
        if(pad_owned)scePadClose(pad);
        af_event close{};close.kind=AF_CLOSE;step(&close);
        fputs("PAD_OBSERVE complete owned_pad_closed=1\n",logfile);fclose(logfile);return 0;
    }
    Ps5GpuDisplay display;static uint32_t pixels[480*270];uint32_t previous=0;
    // The 1080p screen needs two full frames; without them the text panel stays.
    static us_screen screen;
    uint32_t *screen_pixels=(uint32_t*)malloc(US_WIDTH*US_HEIGHT*4),*screen_background=(uint32_t*)malloc(US_WIDTH*US_HEIGHT*4);
    const bool rich=screen_pixels&&screen_background;rich_screen=rich;
    if(rich)us_init(&screen,screen_pixels,screen_background);
    fprintf(logfile,"ui=%s\n",rich?"screen-1080":"text-panel");
    // GPU Lab cannot read the system's title metadata itself; the inventory
    // payload lists installed games, and the library reloads when it lands.
    struct stat seen{};stat(installed_path,&seen);
    fprintf(logfile,"library entries=%u installed=%u inventory_http=%u\n",entry_count,installed_count,load_payload("helixsr-inventory.elf"));
    unsigned frame=0;bool quit=false;
    if(launch_check&&game_count&&selection_valid){fprintf(logfile,"explicit_original_launch_test=1 preferences_unchanged=1\n");quit=launch_original(user,0);}
    /* u10 test (--hook-check TITLE): the injector path without a controller: select TITLE, press Cross twice (the hook
     * switch goes to the other setting and back) and Triangle, through the same functions the buttons call. */
    const char *hook_check=nullptr;for(int i=0;i+1<argc;++i)if(argv[i]&&!strcmp(argv[i],"--hook-check"))hook_check=argv[i+1];
    if(hook_check){
        bool found=false;for(unsigned i=0;i<entry_count&&!found;++i)if(!strcmp(entry_title(i),hook_check)){select_entry(i);found=true;}
        const bool usable=found&&hook_switch();
        fprintf(logfile,"hook_check title=%s found=%d switch=%d on=%d\n",hook_check,found,usable,usable&&hook_info().on);
        if(usable){toggle_hook();toggle_hook();fprintf(logfile,"hook_check toggled twice on=%d\n",hook_info().on);quit=launch_selected(user);}
    }
    while(!quit) {
        struct {PS5_PadData data;unsigned char reserve[256];} packet{};
        int pr=pad>=0?scePadReadState(pad,&packet.data):-1;
        uint32_t buttons=(pr==0&&packet.data.connected)?packet.data.buttons:0;
        uint32_t press=buttons&~previous;previous=buttons;
        const bool was_tool=view_screen!=US_SCREEN_MAIN;
        if(press&~PS5_PAD_BUTTON_CROSS)reset_armed=-1;
        if(frame%60==30){
            struct stat now{};
            if(!stat(installed_path,&now)&&(now.st_mtime!=seen.st_mtime||now.st_size!=seen.st_size||now.st_ino!=seen.st_ino)){
                seen=now;char title[16]={0};bool was_catalog=entry_count&&entries[cursor].catalog;
                if(entry_count)snprintf(title,sizeof title,"%s",entry_title(cursor));
                installed_count=gl_load_installed(installed_path,installed,GL_MAX_GAMES);build_entries();
                fprintf(logfile,"library_reload entries=%u installed=%u\n",entry_count,installed_count);
                if(entry_count&&(strcmp(title,entry_title(cursor))||was_catalog!=entries[cursor].catalog))select_entry(cursor);
            }
        }
        if(entry_count&&view_screen==US_SCREEN_EDITOR)editor_input(press,buttons);
        else if(entry_count&&view_screen==US_SCREEN_FRAMES)frames_input(press,user,&quit);
        else if(entry_count) {
            const bool library=menu==US_MENU_LIBRARY;
            if(press&(PS5_PAD_BUTTON_UP|PS5_PAD_BUTTON_DOWN)){
                const int delta=press&PS5_PAD_BUTTON_UP?-1:1;
                if(library)switch_game(delta);
                else focus=(focus+FIELD_COUNT+(delta<0?FIELD_COUNT-1:1))%FIELD_COUNT;
            }
            else if(press&PS5_PAD_BUTTON_LEFT)menu=US_MENU_LIBRARY;
            else if(press&PS5_PAD_BUTTON_RIGHT){if(library){menu=US_MENU_SETTINGS;focus=FIELD_HELIX;}}
            else if(press&PS5_PAD_BUTTON_CROSS)cross_action();
            else if(press&PS5_PAD_BUTTON_CIRCLE){if(library)select_entry(cursor);else{menu=US_MENU_LIBRARY;select_entry(cursor);}}
            else if(press&PS5_PAD_BUTTON_TRIANGLE)quit=launch_selected(user);
            else if(press&PS5_PAD_BUTTON_L1)switch_game(-1);
            else if(press&PS5_PAD_BUTTON_R1)switch_game(1);
            else if(press&PS5_PAD_BUTTON_SQUARE)open_frames();
        }
        /* The editor and the frame-rate screen use Options to close themselves; everywhere else it exits as before. */
        if(press&PS5_PAD_BUTTON_OPTIONS&&!was_tool&&view_screen==US_SCREEN_MAIN)quit=!preference_dirty||save_options();
        char text[22][81]{};
        const bool ready=package_ready();
        snprintf(text[0],81,"HELIXSR GPU LAB | %s",is_pro()?"PS5 PRO":"PS5");
        snprintf(text[1],81,"%s",!selection_valid?"Not catalogued":preference_dirty?"Unsaved changes":"Saved");
        snprintf(text[3],81,"%c Game %u/%u: %.56s",menu==US_MENU_LIBRARY?'>':' ',entry_count?cursor+1:0,entry_count,game_display_name());
        snprintf(text[5],81,"%c HelixSR: %s",menu==US_MENU_SETTINGS&&focus==FIELD_HELIX?'>':' ',
            hook_switch()?(hook_info().on?"ON (in-game hook)":"OFF (game's own upscale)"):
            !ready?"Not available yet":edited_desired==AP_ON?"ON (HelixSR)":"OFF (game's own upscaler)");
        snprintf(text[6],81,"  Build: %.70s",ready?package_info().description:"-");
        snprintf(text[8],81,"%c HelixSR HUD: %s",menu==US_MENU_SETTINGS&&focus==FIELD_HUD?'>':' ',
            !hud_info().available?"Not available for this game":hud_info().on?"ON (FPS box; L3+R3 in game)":"OFF (L3+R3 in game)");
        snprintf(text[15],81,"%s",on_installed()?"Status: not scanned yet; launch once to scan its upscaler":
            !selection_valid?"Status: profile unavailable; save and launch disabled":
            hook_switch()?(hook_info().on?"Status: HelixSR on in the game's hook; Triangle starts the game":
                           "Status: HelixSR off; Triangle starts the game with its own upscale"):
            !ready?"Status: HelixSR is not available for this game yet":
            edited_desired==AP_ON?"Status: HelixSR ready; Triangle starts the game with it":
            "Status: HelixSR off; Triangle starts the game with its own upscaler");
        snprintf(text[16],81,"%s",on_installed()?"HelixSR reads the game's executable, read-only, while it runs.":
            !ready?"Nothing is changed for this game.":"The package below is what the game runs with HelixSR on.");
        if(ready&&selection_valid) {
            const PackageView &p=package_info();
            if(p.last&&!p.ok){snprintf(text[15],81,"Status: the last HelixSR switch did not complete");snprintf(text[16],81,"%.80s",p.reason);}
        }
        snprintf(text[17],81,"%.79s",feedback);
        snprintf(text[19],81,"Up/Down: game or setting | Left/Right: menu | Cross: toggle");
        snprintf(text[20],81,"Triangle: save and launch | Circle: discard | Options: exit");
        snprintf(text[21],81,"Controller: %s",pr==0&&packet.data.connected?"connected":"not connected");
        // Only the first frame is captured (startup evidence); a capture blocks until shown and writes 6 MB.
        const char *capture=frame==0?"/data/helixsr-speed/control-app.ppm":nullptr;
        int dr;
        if(rich){
            static us_model model;build_model(&model,pr==0&&packet.data.connected,text[15],text[16]);
            // An unchanged screen is shown again without a copy; every iteration is one display refresh.
            dr=us_render(&screen,&model)||capture?display.present(screen_pixels,US_WIDTH,US_HEIGHT,US_WIDTH*4,logfile,capture,true):
                display.repeat(logfile);
        } else {
            tp_line lines[22];for(unsigned i=0;i<22;++i)lines[i]={(const unsigned char*)text[i],strlen(text[i])};
            if(tp_render(pixels,480*270,480,270,480,lines,22,0xffedf2f8,0xff141c29)!=TP_OK)break;
            dr=display.present(pixels,480,270,480*4,logfile,capture,true);
        }
        if(dr){fprintf(logfile,"display_error=%x\n",dr);break;}
        if(frame%600==0)fprintf(logfile,"frame=%u pad_read=%d connected=%u buttons=%x copy_us=%llu\n",frame,pr,packet.data.connected,buttons,
            (unsigned long long)display.last_copy_ns/1000);
        ++frame;if(exit_check&&frame==2)quit=true;
    }
    af_event close{};close.kind=AF_CLOSE;step(&close);
    if(pad_owned)scePadClose(pad);
    close_frames();
    display.close(logfile);free(screen_pixels);free(screen_background);
    fprintf(logfile,"closed=1 frames=%u\n",frame);fclose(logfile);
    return 0;
}





extern "C" void catchReturnFromMain(int status) {
 FILE *f=fopen("/data/helixsr-speed/control-exit.txt","w");
 int r=sceSystemServiceLoadExec("exit",nullptr);
 if(f){fprintf(f,"main_status=%d shell_exit_result=%x\n",status,r);fclose(f);}
 for(;;)sleep(1);
}
