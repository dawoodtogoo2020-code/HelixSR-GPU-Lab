#define _POSIX_C_SOURCE 200809L
#include "frame_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bounded copy of printable ASCII; anything else ends the value. */
static void copy(char *out,size_t size,const char *in)
{
    size_t n=0;
    while(in[n]&&n+1<size&&(unsigned char)in[n]>=32&&(unsigned char)in[n]<=126){out[n]=in[n];++n;}
    out[n]=0;
}
/* One line without its end; a longer line is cut and its rest skipped. 0 at the end of the file. */
static int line_of(FILE *f,char *line,size_t size)
{
    if(!fgets(line,(int)size,f))return 0;
    size_t n=strcspn(line,"\r\n");
    if(!line[n]&&n+1==size){int ch;while((ch=fgetc(f))!=EOF&&ch!='\n'){}}
    line[n]=0;return 1;
}
static int number(const char *text,unsigned long long *out)
{
    char *end;
    if(*text<'0'||*text>'9')return 0;
    *out=strtoull(text,&end,10);return !*end;
}
static void size_pair(const char *text,unsigned *w,unsigned *h)
{
    char tail;
    if(sscanf(text,"%ux%u%c",w,h,&tail)!=2||*w>7680||*h>4320)*w=*h=0;
}
static void header(fl_log *log,const char *key,const char *value)
{
    unsigned long long n;
    if(!strcmp(key,"title"))copy(log->title,sizeof log->title,value);
    else if(!strcmp(key,"version"))copy(log->version,sizeof log->version,value);
    else if(!strcmp(key,"name"))copy(log->name,sizeof log->name,value);
    else if(!strcmp(key,"source_detail"))copy(log->source_detail,sizeof log->source_detail,value);
    else if(!strcmp(key,"started"))copy(log->started,sizeof log->started,value);
    else if(!strcmp(key,"helix"))copy(log->helix,sizeof log->helix,value);
    else if(!strcmp(key,"package"))copy(log->package,sizeof log->package,value);
    else if(!strcmp(key,"end"))copy(log->end,sizeof log->end,value);
    else if(!strcmp(key,"source"))log->source=!strcmp(value,"engine")?FL_SOURCE_ENGINE:!strcmp(value,"gpu-strip")?FL_SOURCE_GPU_STRIP:!strcmp(value,"render-trace")?FL_SOURCE_RENDER_TRACE:FL_SOURCE_UNKNOWN;
    else if(!strcmp(key,"frame_time"))log->frame_time=!strcmp(value,"measured")?FL_TIME_MEASURED:!strcmp(value,"calibrated")?FL_TIME_CALIBRATED:!strcmp(value,"cpu-calibrated")?FL_TIME_CPU_CALIBRATED:FL_TIME_NOMINAL;
    else if(!strcmp(key,"missed"))log->missed_measured=!strcmp(value,"measured");
    else if(!strcmp(key,"render"))size_pair(value,&log->render_width,&log->render_height);
    else if(!strcmp(key,"output"))size_pair(value,&log->output_width,&log->output_height);
    else if(!strcmp(key,"refresh_mhz")&&number(value,&n)&&n>=10000&&n<=480000)log->refresh_mhz=(unsigned)n;
    else if(!strcmp(key,"recorder_drops")&&number(value,&n)&&n<1000000)log->recorder_drops=(unsigned)n;
    else if(!strcmp(key,"wall_ms")&&number(value,&n))log->wall_ms=n;
    else if(!strcmp(key,"counted_frames")&&number(value,&n))log->counted_frames=n;
}
/* FRAME_US [REPEATS [HELIX_US [FLAGS]]]; 0 when the line is not a frame. */
static int frame_line(char *line,unsigned long long v[4])
{
    unsigned n=0;char *save=NULL;
    v[0]=v[1]=v[2]=v[3]=0;
    for(char *p=strtok_r(line," \t",&save);p;p=strtok_r(NULL," \t",&save)) {
        if(n==4||!number(p,&v[n]))return 0;
        ++n;
    }
    return n&&v[0]<=60000000ull&&v[1]<=65535&&v[2]<=60000000ull&&v[3]<=255;
}
int fl_read(const char *path,fl_log *log)
{
    uint32_t *frame_us=log->frame_us,*helix_us=log->helix_us;uint16_t *repeats=log->repeats;uint8_t *flags=log->flags;
    const unsigned capacity=frame_us&&helix_us&&repeats&&flags?log->capacity:0;
    memset(log,0,sizeof *log);
    log->frame_us=frame_us;log->helix_us=helix_us;log->repeats=repeats;log->flags=flags;log->capacity=capacity;
    FILE *f=fopen(path,"r");if(!f)return -1;
    char line[512];
    if(!line_of(f,line,sizeof line)||strcmp(line,"HFPS1")){fclose(f);return -2;}
    int frames=0;
    while(line_of(f,line,sizeof line)) {
        if(!frames) {
            if(!strcmp(line,"frames")){frames=1;if(!capacity)break;continue;}
            char *eq=strchr(line,'=');
            if(eq){*eq=0;header(log,line,eq+1);}
            continue;
        }
        if(!strcmp(line,"end")){log->complete=1;break;}
        unsigned long long v[4];
        if(!frame_line(line,v))break;
        if(log->count<capacity) {
            const unsigned i=log->count++;
            frame_us[i]=(uint32_t)v[0];repeats[i]=(uint16_t)v[1];helix_us[i]=(uint32_t)v[2];flags[i]=(uint8_t)v[3];
        } else ++log->truncated;
    }
    fclose(f);
    return 0;
}
int fl_slot_path(char *out,unsigned size,const char *root,const char *title,unsigned slot)
{
    return snprintf(out,size,"%s/fps/%s-%u.txt",root,title,slot)<(int)size?0:-1;
}
int fl_recorder_known(const char *title,const char *version)
{
    return !strcmp(title,"PPSA34547")&&!strcmp(version,"01.300.300");
}

static int ascending(const void *a,const void *b)
{
    const uint32_t x=*(const uint32_t *)a,y=*(const uint32_t *)b;
    return x<y?-1:x>y;
}
/* Nearest rank of the per-mille quantile in sorted values. */
static unsigned rank(const uint32_t *sorted,unsigned n,unsigned permille)
{
    unsigned long long i=((unsigned long long)n*permille+999u)/1000u;
    return sorted[i?i-1:0];
}
/* Average FPS (milli) over the slowest share (1/divisor) of the sorted values; 0 below minimum values. */
static unsigned low(const uint32_t *sorted,unsigned n,unsigned divisor,unsigned minimum)
{
    if(n<minimum)return 0;
    const unsigned k=(n+divisor-1u)/divisor;unsigned long long sum=0;
    for(unsigned i=n-k;i<n;++i)sum+=sorted[i];
    return sum?(unsigned)((unsigned long long)k*1000000000ull/sum):0;
}
static int counted(const fl_log *log,unsigned i) { return log->frame_us[i]&&!(log->flags[i]&FL_FLAG_PAUSE); }
int fl_compute(const fl_log *log,fl_stats *s,uint32_t *scratch)
{
    memset(s,0,sizeof *s);
    unsigned n=0;unsigned long long repeats=0;
    for(unsigned i=0;i<log->count;++i) {
        if(log->flags[i]&FL_FLAG_PAUSE){++s->pauses;continue;}
        if(!log->frame_us[i]){++s->unknown;continue;}
        scratch[n++]=log->frame_us[i];s->total_us+=log->frame_us[i];repeats+=log->repeats[i];
        if(log->flags[i]&FL_FLAG_ESTIMATED)++s->estimated;
        if(log->flags[i]&FL_FLAG_MODE)++s->mode_changes;
    }
    s->frames=n;
    const unsigned mhz=log->refresh_mhz?log->refresh_mhz:59940u;
    s->refresh_us=(unsigned)((1000000000ull+mhz/2u)/mhz);
    if(!n)return -1;
    qsort(scratch,n,sizeof *scratch,ascending);
    s->min_us=scratch[0];s->max_us=scratch[n-1];
    s->median_us=rank(scratch,n,500);s->p95_us=rank(scratch,n,950);s->p99_us=rank(scratch,n,990);s->p999_us=rank(scratch,n,999);
    s->average_fps_milli=(unsigned)((unsigned long long)n*1000000000ull/s->total_us);
    s->low1_fps_milli=low(scratch,n,100,100);s->low01_fps_milli=low(scratch,n,1000,1000);
    s->target_refreshes=(s->median_us+s->refresh_us/2u)/s->refresh_us;
    if(!s->target_refreshes)s->target_refreshes=1;
    s->missed_measured=log->missed_measured;
    if(s->missed_measured) {
        s->missed=repeats>0xffffffffull?0xffffffffu:(unsigned)repeats;
        s->displayed_fps_milli=(unsigned)((unsigned long long)n*mhz/(n+repeats));
    }
    const unsigned range=s->target_refreshes*s->refresh_us*5u/2u>s->p999_us/5u*6u?s->target_refreshes*s->refresh_us*5u/2u:s->p999_us/5u*6u;
    s->bin_us=(range/FL_BINS+99u)/100u*100u;
    if(!s->bin_us)s->bin_us=100;
    for(unsigned i=0;i<log->count;++i) {
        if(!counted(log,i))continue;
        const unsigned f=log->frame_us[i],bin=f/s->bin_us;
        if(bin<FL_BINS)++s->histogram[bin];else ++s->overflow;
        if(!s->missed_measured) {
            const unsigned refreshes=(f+s->refresh_us/2u)/s->refresh_us;
            if(refreshes>s->target_refreshes)s->missed+=refreshes-s->target_refreshes;
        }
    }
    unsigned h=0;
    for(unsigned i=0;i<log->count;++i)if(counted(log,i)&&log->helix_us[i])scratch[h++]=log->helix_us[i];
    if(h) {
        qsort(scratch,h,sizeof *scratch,ascending);
        s->helix_frames=h;s->helix_median_us=rank(scratch,h,500);s->helix_p99_us=rank(scratch,h,990);
    }
    return 0;
}
unsigned fl_fps_series(const fl_log *log,uint16_t *fps_tenths,unsigned capacity)
{
    unsigned elapsed_us=0,seconds=0,frames=0;
    for(unsigned i=0;i<log->count&&seconds<capacity;++i) {
        if(!counted(log,i))continue;
        elapsed_us+=log->frame_us[i];
        while(elapsed_us>=1000000u&&seconds<capacity) {
            /* Each bucket includes a frame that ends at its right boundary. */
            if(elapsed_us==1000000u)++frames;
            fps_tenths[seconds++]=(uint16_t)(frames>6553?65535u:frames*10u);
            frames=0;
            elapsed_us-=1000000u;
        }
        if(elapsed_us)++frames;
    }
    return seconds;
}
void fl_columns(const fl_log *log,unsigned columns,uint16_t *shortest,uint16_t *longest)
{
    unsigned long long total=0,t=0;
    memset(shortest,0,columns*sizeof *shortest);memset(longest,0,columns*sizeof *longest);
    for(unsigned i=0;i<log->count;++i)if(counted(log,i))total+=log->frame_us[i];
    if(!total||!columns)return;
    for(unsigned i=0;i<log->count;++i) {
        if(!counted(log,i))continue;
        t+=log->frame_us[i];
        unsigned long long c=(t*columns-1u)/total;
        if(c>=columns)c=columns-1u;
        const unsigned tens=log->frame_us[i]/10u;const uint16_t v=(uint16_t)(tens>65535u?65535u:tens?tens:1u);
        if(!shortest[c]||v<shortest[c])shortest[c]=v;
        if(v>longest[c])longest[c]=v;
    }
}
