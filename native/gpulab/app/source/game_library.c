#define _POSIX_C_SOURCE 200809L
#include "game_library.h"
#include "../../common/package_support.h"
#include <stdio.h>
#include <string.h>

/* In-game measurements. CONTROL: the native-size probe hooked the FSR 2
 * constants path, 1,024 samples of iRenderSize 1505x847 and iUpscaleSize
 * 2560x1440 (2026-10-24 please-continue-work-on-bringing-helix, work/
 * native-size-probe-v2/observed-sizes.json). Profiles need even sizes.
 * SHADOW OF THE COLOSSUS (PS4 title, PS4 Pro mode on a PS5 Pro): its final
 * submission draws into a 2560x1440 RGBA16F colour target with an RG16F
 * motion-vector target and a depth target (17 Oct 2026, fresh-start-for-
 * the-helixsr-in work/sotc-sync, camera pans move the vectors both ways).
 * The Pro session read SOTC's final SubmitAndFlip command buffer on 29 Oct:
 * the scene renders natively at 2560x1440 with one sample (no MSAA, no custom
 * sample positions, so no checkerboard), and the game upscales it to a
 * 3840x2160 8_8_8_8 target for the HUD; the display buffer is 3840x2160.
 * motion=1: the game was seen to produce motion vectors, so HelixSR can take
 * over its reconstruction even when the upscaler is not one of the temporal
 * upscalers HelixSR normally replaces. */
typedef struct {
    const char *title,*version,*primary,*engine;
    unsigned output_width,output_height,input_width,input_height;
    int motion;
    const char *basis;
} gl_measured;
static const gl_measured measured[]={
    {"PPSA07631","01.000.000","Unreal TSR","Unreal Engine",3840,2160,1536,864,1,"4K output; ~60 FPS in tests; shadows and volumetric fog off"},
    {"PPSA02572","01.200.007","FSR 2","Northlight",2560,1440,1505,847,1,"AW2 t12: Performance 1505x847 to 2560x1440; Quality uses FSR 2"},
    {"PPSA34547","01.300.300","FSR 2","Northlight",2560,1440,1506,848,1,"Measured in-game: FSR 2 Balanced, 1505x847 to 2560x1440"},
    {"CUSA08809","01.00","Game upscale","Bluepoint",3840,2160,2560,1440,1,"Measured on PS5 Pro: native 2560x1440, game upscale to 4K"}};
/* Games with HelixSR runtime work under way (see the HelixSR STATUS.md). */
static const char *const hooks_in_development[]={"PPSA07631","PPSA34547","PPSA02572","CUSA08809"};

static void copy(char *out,size_t size,const char *in)
{
    size_t n=strlen(in);
    if(n>=size)n=size-1;
    memcpy(out,in,n);out[n]=0;
}
static int printable(const char *s)
{
    for(;*s;++s)if((unsigned char)*s<32||(unsigned char)*s>126)return 0;
    return 1;
}
unsigned gl_load_installed(const char *path,gl_game *out,unsigned capacity)
{
    FILE *f=fopen(path,"r");if(!f)return 0;
    char line[256];unsigned count=0;
    while(count<capacity&&fgets(line,sizeof line,f)) {
        char *save=NULL,*parts[3];unsigned n=0;
        for(char *p=strtok_r(line,"\t\r\n",&save);p&&n<3;p=strtok_r(NULL,"\t\r\n",&save))parts[n++]=p;
        if(n!=3||strlen(parts[0])!=9||strlen(parts[1])>=sizeof out->version||!printable(parts[2]))continue;
        gl_game *g=&out[count++];
        copy(g->title,sizeof g->title,parts[0]);copy(g->version,sizeof g->version,parts[1]);copy(g->name,sizeof g->name,parts[2]);
    }
    fclose(f);
    return count;
}
static int value(const char *text,const char *key,char *out,size_t size)
{
    char pattern[32];snprintf(pattern,sizeof pattern,"\n%s=",key);
    const char *p=strstr(text,pattern);if(!p)return 0;
    p+=strlen(pattern);size_t n=strcspn(p,"\r\n");
    if(n>=size)n=size-1;
    memcpy(out,p,n);out[n]=0;
    return printable(out);
}
/* "WxH" with even sizes up to 8K. */
static int size_pair(const char *text,unsigned *w,unsigned *h)
{
    char tail;
    return sscanf(text,"%ux%u%c",w,h,&tail)==2&&*w&&*h&&*w<=7680&&*h<=4320&&!(*w&1)&&!(*h&1);
}
/* profiles/TITLE.txt: "HPROF1", then version, upscaler, engine, input and output ("WxH"), motion (0/1),
 * hook ("development" or "none") and basis lines. Any missing or malformed field rejects the whole profile. */
static int load_profile(const char *root,const char *title,const char *version,gl_finding *out)
{
    char path[512],text[2048],field[96];
    if(snprintf(path,sizeof path,"%s/profiles/%s.txt",root,title)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"r");if(!f)return 0;
    size_t n=fread(text+1,1,sizeof text-2,f);fclose(f);
    text[0]='\n';text[n+1]=0;
    gl_finding p;memset(&p,0,sizeof p);
    if(strncmp(text+1,"HPROF1\n",7)||!value(text,"version",p.version,sizeof p.version)||strcmp(p.version,version)||
       !value(text,"upscaler",p.primary,sizeof p.primary)||!value(text,"engine",p.engine,sizeof p.engine)||
       !value(text,"input",field,sizeof field)||!size_pair(field,&p.input_width,&p.input_height)||
       !value(text,"output",field,sizeof field)||!size_pair(field,&p.output_width,&p.output_height)||
       p.input_width>p.output_width||p.input_height>p.output_height||
       !value(text,"motion",field,sizeof field)||(strcmp(field,"0")&&strcmp(field,"1"))||
       !value(text,"hook",p.hook,sizeof p.hook)||(strcmp(p.hook,"development")&&strcmp(p.hook,"none"))||
       !value(text,"basis",p.measured_basis,sizeof p.measured_basis)||!p.primary[0]||!p.engine[0])return 0;
    p.motion=field[0]=='1';
    if(!strcmp(p.hook,"none"))p.hook[0]=0;
    p.basis="Game profile";p.measured=1;p.present=1;
    *out=p;return 1;
}
int gl_find(const char *root,const char *title,const char *version,gl_finding *out)
{
    memset(out,0,sizeof *out);
    if(load_profile(root,title,version,out))return 1;
    for(unsigned i=0;i<sizeof measured/sizeof *measured;++i)
        if(!strcmp(measured[i].title,title)&&!strcmp(measured[i].version,version)) {
            copy(out->version,sizeof out->version,version);copy(out->primary,sizeof out->primary,measured[i].primary);
            copy(out->engine,sizeof out->engine,measured[i].engine);out->basis="Runtime probe";out->present=1;
            out->measured=1;out->motion=measured[i].motion;
            out->output_width=measured[i].output_width;out->output_height=measured[i].output_height;
            out->input_width=measured[i].input_width;out->input_height=measured[i].input_height;
            copy(out->measured_basis,sizeof out->measured_basis,measured[i].basis);
            return 1;
        }
    char path[512],text[4096];
    snprintf(path,sizeof path,"%s/scan/%s.txt",root,title);
    FILE *f=fopen(path,"r");
    if(f) {
        size_t n=fread(text+1,1,sizeof text-2,f);fclose(f);
        text[0]='\n';text[n+1]=0;
        if(!strncmp(text+1,"HSCAN1\n",7)&&value(text,"version",out->version,sizeof out->version)&&
           !strcmp(out->version,version)&&value(text,"primary",out->primary,sizeof out->primary)&&
           value(text,"engine",out->engine,sizeof out->engine)) {
            value(text,"sha256",out->sha256,sizeof out->sha256);
            out->basis="Executable scan";out->present=1;return 1;
        }
        memset(out,0,sizeof *out);
    }
    return 0;
}
void gl_recommend(const char *title,const char *version,const gl_finding *finding,gl_route *out)
{
    (void)version;
    memset(out,0,sizeof *out);
    if(!finding||!finding->present){copy(out->route,sizeof out->route,"Scan needed: launch the game once");return;}
    if(!finding->measured&&!gp_managed_title(title)) {
        out->scan_only=1;
        copy(out->route,sizeof out->route,"Launch as installed");
        copy(out->basis,sizeof out->basis,"The scan cannot verify the active upscaler or its frame inputs.");
        return;
    }
    /* HelixSR is a temporal upscaler: it replaces upscalers that already give
     * it colour, depth, motion vectors and jitter, and any other reconstruction
     * (checkerboard, the game's own TAA) whose motion vectors were measured. */
    static const char *const temporal[]={"FSR 2","FSR 3","FSR 4","Unreal TSR","TAAU","XeSS","PSSR"};
    int replace=finding->measured&&finding->motion;
    for(unsigned i=0;i<sizeof temporal/sizeof *temporal;++i)if(!strcmp(finding->primary,temporal[i]))replace=1;
    if(!replace) {
        if(!strcmp(finding->primary,"Unknown"))copy(out->route,sizeof out->route,"Upscaler not identified: keep game default");
        else snprintf(out->route,sizeof out->route,"Keep game default: %s has no motion vectors",finding->primary);
        copy(out->basis,sizeof out->basis,"HelixSR needs a temporal upscaler's inputs to replace it.");
        return;
    }
    out->replace=1;
    snprintf(out->route,sizeof out->route,"Replace %s with HelixSR",finding->primary);
    out->output_width=2560;out->output_height=1440;out->input_width=1706;out->input_height=960;
    copy(out->basis,sizeof out->basis,"Render size not measured yet: Quality preset as a starting point");
    if(finding->measured) {
        out->measured=1;
        out->output_width=finding->output_width;out->output_height=finding->output_height;
        out->input_width=finding->input_width;out->input_height=finding->input_height;
        copy(out->basis,sizeof out->basis,finding->measured_basis);
    }
}
int gl_package_ready(const char *root,const char *title,const char *version,char *description,size_t size)
{
    char path[512],line[128];
    if(description&&size)description[0]=0;
    if(snprintf(path,sizeof path,"%s/packages/%s-%s/READY",root,title,version)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"r");if(!f)return 0;
    const char *expected=NULL;
    if(!strcmp(title,"PPSA07631")&&!strcmp(version,"01.000.000"))expected="HPKG1 saros-native-v1\n";
    if(!strcmp(title,"PPSA34547")&&!strcmp(version,"01.300.300"))expected="HPKG1 control-r18-20261029\n";
    if(!strcmp(title,"PPSA02572")&&!strcmp(version,"01.200.007"))expected="HPKG1 aw2-t12\n";
    int ok=expected&&fgets(line,sizeof line,f)&&!strcmp(line,expected);
    if(ok&&description&&size&&fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;if(printable(line))copy(description,size,line);}
    fclose(f);return ok;
}
int gl_package_id(const char *root,const char *title,const char *version,char *id,size_t size)
{
    char path[512],line[128];
    if(id&&size)id[0]=0;
    if(snprintf(path,sizeof path,"%s/packages/%s-%s/READY",root,title,version)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"r");if(!f)return 0;
    int ok=fgets(line,sizeof line,f)&&!strncmp(line,"HPKG1 ",6);
    fclose(f);
    if(ok){line[strcspn(line,"\r\n")]=0;ok=printable(line+6)&&line[6];if(ok&&id&&size)copy(id,size,line+6);}
    return ok;
}
int gl_install_state(const char *root,const char *title,const char *version,char state[24],char reason[200],int *ok)
{
    char path[512],line[256],seen_version[32]="";
    state[0]=reason[0]=0;*ok=0;
    if(snprintf(path,sizeof path,"%s/install/%s.txt",root,title)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"r");if(!f)return 0;
    int magic=0;
    while(fgets(line,sizeof line,f)) {
        line[strcspn(line,"\r\n")]=0;
        if(!strcmp(line,"HINST1"))magic=1;
        else if(!strncmp(line,"version=",8))copy(seen_version,sizeof seen_version,line+8);
        else if(!strncmp(line,"state=",6))copy(state,24,line+6);
        else if(!strncmp(line,"reason=",7))copy(reason,200,line+7);
        else if(!strncmp(line,"ok=",3))*ok=line[3]=='1';
    }
    fclose(f);
    if(!magic||strcmp(seen_version,version)||!state[0]||!printable(state)||!printable(reason)){state[0]=reason[0]=0;*ok=0;return 0;}
    return 1;
}
const char *gl_hook_status(const char *title,const gl_finding *finding,int ready,int *in_development)
{
    *in_development=0;
    if(ready)return "Ready";
    if(finding&&finding->present&&!strcmp(finding->hook,"development")){*in_development=1;return "In development";}
    for(unsigned i=0;i<sizeof hooks_in_development/sizeof *hooks_in_development;++i)
        if(!strcmp(hooks_in_development[i],title)){*in_development=1;return "In development";}
    return "Not available yet";
}
