#include "resolution_plan.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* CONTROL Resonant 01.300.300. Performance: the FSR 2 constants probe (24 Oct, 1,024 samples of iRenderSize 1505x847,
 * iUpscaleSize 2560x1440) and eight executable-anchored memory reads (28-29 Oct). Quality, which the game calls
 * Cinematic: eight anchored reads right after selecting it (28 Oct). The Quality package (hey-i-need-you-to-take
 * work/cinematic/quality-2258x1270 + quality-game-bounds) passed the native six-frame model test and a short Quality-only
 * gameplay smoke; R18 (29 Oct) is the installed two-mode package with both layouts. */
static const struct { const char *title,*version; rp_known_pair known; } known_pairs[]={
    {"PPSA07631","01.000.000",{{1536,864,3840,2160},"Dynamic (observed)",RP_PACKAGE_TRIAL,
        "SAROS 4K performance","4K output; ~60 FPS in tests; shadows and volumetric fog off"}},
    {"PPSA02572","01.200.007",{{1505,847,2560,1440},"Performance",RP_PACKAGE_BUILT,
        "HelixSR AW2 t12 package","AW2 developer-console t12 evidence; Quality uses FSR 2"}},
    {"PPSA34547","01.300.300",{{1505,847,2560,1440},"Performance",RP_PACKAGE_BUILT,
        "HelixSR r17 and r18 packages","Measured in-game: FSR 2 render size probe and memory reads"}},
    {"PPSA34547","01.300.300",{{2258,1270,3840,2160},"Cinematic",RP_PACKAGE_BUILT,
        "HelixSR r18 package (Performance + Cinematic)","Measured in-game: memory reads after selecting Quality"}},
    /* SHADOW OF THE COLOSSUS on the PS5 Pro (PS4 Pro mode): its final SubmitAndFlip, read by the Pro session on 29 Oct:
     * native 2560x1440, one sample, upscaled by the game to 3840x2160. The HelixSR hook is in development. */
    {"CUSA08809","01.00",{{2560,1440,3840,2160},"PS4 Pro",RP_PACKAGE_NONE,"",
        "Measured on PS5 Pro: native 2560x1440, game upscale to 4K"}}};
/* Pairs of packages whose READY line names no sizes: r18 writes "HelixSR r18, Performance + Cinematic, ..." and runs
 * HelixSR at both of these layouts; every other layout runs stock FSR2 (work/r18-dual-cinematic/RESULTS.md). */
static const struct { const char *id; rp_pair pair; } package_pairs[]={
    {"saros-native-v1",{1536,864,3840,2160}},{"saros-native-v1",{1600,900,3840,2160}},{"saros-native-v1",{1866,1050,3840,2160}},{"saros-native-v1",{2304,1296,3840,2160}},
    {"aw2-t12",{1505,847,2560,1440}},
    {"control-r18-20261029",{1505,847,2560,1440}},{"control-r18-20261029",{2258,1270,3840,2160}}};

unsigned rp_package_pairs(const char *package_id,rp_pair *out,unsigned capacity)
{
    unsigned n=0;
    for(unsigned i=0;i<sizeof package_pairs/sizeof *package_pairs&&n<capacity;++i)
        if(package_id&&!strcmp(package_pairs[i].id,package_id))out[n++]=package_pairs[i].pair;
    return n;
}

const rp_known_pair *rp_known(const char *title,const char *version,unsigned index)
{
    for(unsigned i=0;i<sizeof known_pairs/sizeof *known_pairs;++i)
        if(!strcmp(known_pairs[i].title,title)&&!strcmp(known_pairs[i].version,version)&&!index--)return &known_pairs[i].known;
    return NULL;
}
int rp_pair_equal(const rp_pair *a,const rp_pair *b)
{
    return a->input_width==b->input_width&&a->input_height==b->input_height&&
           a->output_width==b->output_width&&a->output_height==b->output_height;
}
static unsigned gcd(unsigned a,unsigned b) { while(b){unsigned t=a%b;a=b;b=t;}return a; }
/* build_motion.py: the source-domain store block maps each output pixel back with a 32-bit reciprocal of the reduced
 * render step; it refuses a shape unless 1 < output/render < 2 and floor(N*R/2^32) == floor(N/num) for every N used. */
static int motion_axis(unsigned render,unsigned output)
{
    if(!(output>render&&output<2u*render))return 0;
    const unsigned g=gcd(render,output),num=render/g,den=output/g;
    const uint64_t reciprocal=((1ull<<32)+num-1u)/num,error=reciprocal*num-(1ull<<32);
    const uint64_t largest=(uint64_t)render*den+num-1u;
    return largest*error<(1ull<<32);
}
static unsigned milli(unsigned output,unsigned input) { return input?(unsigned)(((uint64_t)output*1000u+input/2u)/input):0u; }
void rp_check_pair(const rp_pair *p,rp_check *c)
{
    memset(c,0,sizeof *c);
    const unsigned iw=p->input_width,ih=p->input_height,ow=p->output_width,oh=p->output_height;
    c->scale_x_milli=milli(ow,iw);c->scale_y_milli=milli(oh,ih);
    const uint64_t area=(uint64_t)ow*oh,shape=(uint64_t)ow*ih,cross=(uint64_t)iw*oh;
    if(area)c->pixels_permille=(unsigned)(((uint64_t)iw*ih*1000u+area/2u)/area);
    if(shape)c->aspect_permille=(unsigned)(((cross>shape?cross-shape:shape-cross)*1000u+shape-1u)/shape);
    c->network_width=(ow+127u)/128u*128u;c->network_height=(oh+127u)/128u*128u;
    c->ultra_network=2ull*ow>5ull*iw;
    c->kernel=10ull*ow>=17ull*iw&&10ull*oh>=17ull*ih?RP_KERNEL_FAST:100ull*ow>=149ull*iw&&100ull*oh>=149ull*ih?RP_KERNEL_MID:RP_KERNEL_3X3;
    c->motion_ok=iw&&ih&&motion_axis(iw,ow)&&motion_axis(ih,oh);
    c->tiles_exact=c->network_width%512u==0&&c->network_height%512u==0;
    c->ps5_kernels=!c->ultra_network&&c->kernel==RP_KERNEL_FAST&&c->motion_ok;
    const char *problem=NULL;char text[96];
    if(ow>RP_MAX_OUTPUT_WIDTH||oh>RP_MAX_OUTPUT_HEIGHT)problem="Output is larger than 3840 x 2160, the PS5's largest";
    else if(ow<RP_MIN_OUTPUT_WIDTH||oh<RP_MIN_OUTPUT_HEIGHT)problem="Output is smaller than 1280 x 720";
    else if((ow|oh)&1u)problem="Output width and height must be even";
    else if(iw<RP_MIN_INPUT_WIDTH||ih<RP_MIN_INPUT_HEIGHT)problem="Render size is smaller than 640 x 360";
    else if(iw>ow||ih>oh)problem="Render size is larger than the output";
    else if(c->aspect_permille>RP_ASPECT_PERMILLE) {
        snprintf(text,sizeof text,"Render and output shapes differ by %u.%u%% (at most 1%%)",
                 c->aspect_permille/10u,c->aspect_permille%10u);
        problem=text;
    }
    c->limits_ok=!problem||problem==text;   /* text only ever holds the shape problem, the last check */
    if(problem)snprintf(c->problem,sizeof c->problem,"%s",problem);
    c->valid=!problem;
}
unsigned rp_parse_pairs(const char *text,rp_pair *out,unsigned capacity)
{
    unsigned count=0;
    for(const char *at=text?strstr(text," -> "):NULL;at&&count<capacity;at=strstr(at+4," -> ")) {
        const char *start=at;while(start>text&&start[-1]!=' ')--start;
        unsigned iw=0,ih=0,ow=0,oh=0;
        if(sscanf(start,"%ux%u -> %ux%u",&iw,&ih,&ow,&oh)==4&&iw&&ih&&ow&&oh&&iw<=ow&&ih<=oh&&ow<=7680&&oh<=4320)
            out[count++]=(rp_pair){iw,ih,ow,oh};
    }
    return count;
}
