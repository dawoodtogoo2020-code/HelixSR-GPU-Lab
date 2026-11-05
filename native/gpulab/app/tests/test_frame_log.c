/* Host test of frame_log.c: the HFPS1 reader and the statistics on logs whose answers are known by construction, plus
 * the real 28 Oct r17 route log (evidence/logs/PPSA34547-1.txt) against the PC analyzer's numbers
 * (hey-i-need-you-to-take work/runtime/control-summary.json: 11,348 frames, 34 repeats, period median 16.655 ms at the
 * nominal clock, span median 3.978 ms).
 *   ./test_frame_log TMPDIR REAL_LOG (test.sh) */
#include "frame_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t fus[FL_MAX_FRAMES],hus[FL_MAX_FRAMES],scratch[FL_MAX_FRAMES];
static uint16_t rep[FL_MAX_FRAMES];static uint8_t fl[FL_MAX_FRAMES];
static fl_log log_at(void) { fl_log l;memset(&l,0,sizeof l);l.frame_us=fus;l.helix_us=hus;l.repeats=rep;l.flags=fl;l.capacity=FL_MAX_FRAMES;return l; }
static void write_log(const char *path,const char *header,unsigned frames,unsigned us,unsigned every,unsigned slow_us,int measured)
{
    FILE *f=fopen(path,"w");assert(f);
    fprintf(f,"HFPS1\n%sframes\n",header);
    for(unsigned i=0;i<frames;++i) {
        const int slow=every&&i%every==every-1;
        if(measured)fprintf(f,"%u %u 4000\n",slow?slow_us:us,slow?1u:0u);else fprintf(f,"%u\n",slow?slow_us:us);
    }
    fputs("end\n",f);fclose(f);
}

int main(int argc,char **argv)
{
    assert(argc==2||argc==3);char path[512];snprintf(path,sizeof path,"%s/a.txt",argv[1]);
    /* 6,000 frames at one refresh, every 100th takes two: average, lows, missed (estimated), median. */
    write_log(path,"title=PPSA34547\nsource=engine\nrefresh_mhz=59940\nframe_time=measured\nmissed=estimated\n",6000,16683,100,33367,0);
    fl_log l=log_at();fl_stats s;
    assert(!fl_read(path,&l)&&l.complete&&l.count==6000&&l.source==FL_SOURCE_ENGINE&&!l.missed_measured);
    assert(!fl_compute(&l,&s,scratch));
    assert(s.frames==6000&&s.missed==60&&s.target_refreshes==1&&s.median_us==16683&&s.max_us==33367);
    /* slowest 1% = the 60 two-refresh frames: 29.97 FPS; average = 6000 / (5940 x 16683 + 60 x 33367) us. */
    assert(s.low1_fps_milli==29969&&s.low01_fps_milli==29969);
    assert(s.average_fps_milli==(unsigned)(6000ull*1000000000ull/(5940ull*16683+60ull*33367)));
    assert(s.histogram[16683/s.bin_us]==5940&&s.histogram[33367/s.bin_us]==60&&!s.overflow);
    /* Per-second series: 59 or 60 frames in each full second; columns cover the play time. */
    uint16_t fps[200];const unsigned n=fl_fps_series(&l,fps,200);
    assert(n==(unsigned)(s.total_us/1000000u));
    for(unsigned i=0;i<n;++i)assert(fps[i]>=580&&fps[i]<=600);
    uint16_t lo[100],hi[100];fl_columns(&l,100,lo,hi);
    for(unsigned i=0;i<100;++i)assert(hi[i]&&lo[i]==1668&&(hi[i]==1668||hi[i]==3336));
    /* A frame ending exactly at 1.000 s belongs to the first completed second. */
    write_log(path,"",100,20000,0,0,0);
    l=log_at();assert(!fl_read(path,&l));
    assert(fl_fps_series(&l,fps,200)==2&&fps[0]==500&&fps[1]==500);
    /* Only full seconds count. A long frame can span a whole second with no endings. */
    l.count=3;fus[0]=2250000;fus[1]=750000;fus[2]=20000;
    assert(fl_fps_series(&l,fps,200)==3&&fps[0]==0&&fps[1]==0&&fps[2]==20);
    assert(fl_fps_series(&l,fps,1)==1&&fps[0]==0);
    /* Measured repeats: missed and displayed FPS come from the recording, HelixSR block from its column. */
    write_log(path,"source=gpu-strip\nrefresh_mhz=59940\nframe_time=calibrated\nmissed=measured\n",2000,16683,50,16683,1);
    l=log_at();assert(!fl_read(path,&l)&&!fl_compute(&l,&s,scratch));
    assert(s.missed_measured&&s.missed==40&&s.displayed_fps_milli==(unsigned)(2000ull*59940/2040)&&s.helix_median_us==4000&&s.helix_frames==2000);
    assert(s.low01_fps_milli&&s.low1_fps_milli);
    /* Pauses are left out; too few frames give no lows; header-only reads stop at "frames". */
    FILE *f=fopen(path,"w");fputs("HFPS1\nstarted=2026-10-29 10:00:00 UTC\nframes\n16683\n900000 0 0 8\n16683\n",f);fclose(f);
    l=log_at();assert(!fl_read(path,&l)&&!l.complete&&l.count==3&&!fl_compute(&l,&s,scratch));
    assert(s.frames==2&&s.pauses==1&&!s.low1_fps_milli&&!s.low01_fps_milli);
    fl_log head;memset(&head,0,sizeof head);assert(!fl_read(path,&head)&&!head.count&&!strcmp(head.started,"2026-10-29 10:00:00 UTC"));
    /* Damage: not HFPS1, a bad frame line ends the frames, capacity truncates. */
    f=fopen(path,"w");fputs("HFPS2\nframes\n1\n",f);fclose(f);l=log_at();assert(fl_read(path,&l)==-2);
    f=fopen(path,"w");fputs("HFPS1\nframes\n16683\n16683x\n16683\nend\n",f);fclose(f);l=log_at();assert(!fl_read(path,&l)&&l.count==1&&!l.complete);
    snprintf(path,sizeof path,"%s/missing.txt",argv[1]);l=log_at();assert(fl_read(path,&l)==-1);
    snprintf(path,sizeof path,"%s/a.txt",argv[1]);write_log(path,"",50,16683,0,0,0);
    l=log_at();l.capacity=20;assert(!fl_read(path,&l)&&l.count==20&&l.truncated==30);
    assert(!fl_compute(&l,&s,scratch)&&!s.low1_fps_milli);
    char slot[160];assert(!fl_slot_path(slot,sizeof slot,"/data/helixsr-speed","PPSA34547",3)&&!strcmp(slot,"/data/helixsr-speed/fps/PPSA34547-3.txt"));
    assert(fl_recorder_known("PPSA34547","01.300.300")&&!fl_recorder_known("PPSA34547","01.200.000")&&!fl_recorder_known("CUSA08809","01.00"));
    /* Imported CPU callback traces keep their distinct measurement basis. */
    write_log(path,"title=PPSA07631\nsource=render-trace\nframe_time=cpu-calibrated\nmissed=estimated\n",1500,18449,0,0,0);
    l=log_at();assert(!fl_read(path,&l)&&l.complete&&l.source==FL_SOURCE_RENDER_TRACE&&l.frame_time==FL_TIME_CPU_CALIBRATED);
    assert(!fl_compute(&l,&s,scratch)&&s.frames==1500&&s.average_fps_milli==54203&&!s.missed_measured&&!s.helix_frames);
    assert(!fl_recorder_known("PPSA07631","01.000.000")); /* Imports do not advertise an unattended recorder. */
    /* The real r17 route log. */
    if(argc==2){puts("frame log synthetic regressions: PASS (no private recording supplied)");return 0;}
    l=log_at();assert(!fl_read(argv[2],&l)&&l.complete&&!fl_compute(&l,&s,scratch));
    printf("real r17 log: frames=%u repeats(missed)=%u avg=%u.%03u low1=%u.%03u low01=%u.%03u median=%u p99=%u max=%u helix median=%u p99=%u displayed=%u.%03u\n",
           s.frames,s.missed,s.average_fps_milli/1000,s.average_fps_milli%1000,s.low1_fps_milli/1000,s.low1_fps_milli%1000,
           s.low01_fps_milli/1000,s.low01_fps_milli%1000,s.median_us,s.p99_us,s.max_us,s.helix_median_us,s.helix_p99_us,
           s.displayed_fps_milli/1000,s.displayed_fps_milli%1000);
    assert(s.frames==11348&&s.missed==34&&s.missed_measured&&s.median_us==16683);
    /* Span median 3.978 ms at the nominal clock; the log's clock is calibrated by 100/99.832. */
    assert(s.helix_median_us>=3975&&s.helix_median_us<=3990);
    puts("frame log: PASS");
    return 0;
}
