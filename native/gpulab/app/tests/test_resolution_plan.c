/* Host test of resolution_plan.c and the u07 render_options relaxation: the measured CONTROL pairs pass every check
 * they passed in the real package builds, the planner's double-precision rules (model.cpp) agree with the integer
 * ones over a sweep, the limits refuse what they should, READY lines with one or two pairs parse, and an odd custom
 * render size is a valid HRO1 record while presets stay even.
 *   clang -std=c11 -Wall -Wextra -Werror -I../source ../source/resolution_plan.c ../source/render_options.c ... (test.sh) */
#include "resolution_plan.h"
#include "render_options.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static rp_check check(unsigned iw,unsigned ih,unsigned ow,unsigned oh)
{
    rp_pair p={iw,ih,ow,oh};rp_check c;rp_check_pair(&p,&c);return c;
}
/* model.cpp, verbatim logic: output_ratio_min and the kernel choice, network_for. */
static int planner_kernel(unsigned iw,unsigned ih,unsigned ow,unsigned oh)
{
    const double rw=(double)ow/iw,rh=(double)oh/ih,rmin=rw<rh?rw:rh;
    return rmin>=1.7?RP_KERNEL_FAST:rmin>=1.49?RP_KERNEL_MID:RP_KERNEL_3X3;
}
static int planner_ultra(unsigned iw,unsigned ow) { return iw&&(double)ow/iw>2.5; }

int main(void)
{
    /* Performance, installed r17: all checks pass, as in n01-native-r53 (shape-proof, motion build). */
    rp_check c=check(1505,847,2560,1440);
    assert(c.valid&&c.limits_ok&&c.kernel==RP_KERNEL_FAST&&!c.ultra_network&&c.motion_ok&&c.tiles_exact&&c.ps5_kernels);
    assert(c.network_width==2560&&c.network_height==1536&&c.scale_x_milli==1701&&c.scale_y_milli==1700&&c.pixels_permille==346);
    /* Quality, the trial: fastpath, motion helper exact (NATIVE-BUILD-AUDIT.md), padded to 3840 x 2176, tiles not exact
     * (the enc_0/enc_2 partial-tile defect). */
    c=check(2258,1270,3840,2160);
    assert(c.valid&&c.kernel==RP_KERNEL_FAST&&c.motion_ok&&!c.tiles_exact&&c.ps5_kernels&&c.network_width==3840&&c.network_height==2176);
    /* One more render pixel crosses into dfn_mid (the audit's 1.7 boundary). */
    assert(check(2259,1270,3840,2160).kernel==RP_KERNEL_MID&&check(2258,1271,3840,2160).kernel==RP_KERNEL_MID);
    /* 2.0x: fastpath, but the motion store block needs output < 2 x render. 3x: the Ultra Performance network. */
    c=check(1920,1080,3840,2160);assert(c.valid&&c.kernel==RP_KERNEL_FAST&&!c.motion_ok&&!c.ps5_kernels);
    c=check(1280,720,3840,2160);assert(c.valid&&c.ultra_network&&!c.ps5_kernels);
    assert(check(2560,1440,3840,2160).kernel==RP_KERNEL_MID&&check(3840,2160,3840,2160).kernel==RP_KERNEL_3X3);
    /* The integer rules equal the planner's doubles over every render width at several outputs. */
    static const unsigned outputs[][2]={{1280,720},{1920,1080},{2560,1440},{3200,1800},{3840,2160}};
    unsigned compared=0;
    for(unsigned o=0;o<5;++o)for(unsigned iw=640;iw<=outputs[o][0];++iw) {
        const unsigned ih=(unsigned)(((unsigned long long)iw*outputs[o][1]+outputs[o][0]/2)/outputs[o][0]);
        c=check(iw,ih,outputs[o][0],outputs[o][1]);
        assert(c.kernel==planner_kernel(iw,ih,outputs[o][0],outputs[o][1])&&c.ultra_network==planner_ultra(iw,outputs[o][0]));
        ++compared;
    }
    /* Limits. */
    c=check(2560,1440,4096,2304);assert(!c.valid&&!c.limits_ok&&strstr(c.problem,"3840 x 2160"));
    c=check(3201,1800,3200,1800);assert(!c.valid&&strstr(c.problem,"larger than the output"));
    c=check(1505,847,2561,1440);assert(!c.valid&&strstr(c.problem,"even"));
    c=check(600,338,1280,720);assert(!c.valid&&strstr(c.problem,"640 x 360"));
    c=check(1000,720,1280,720);assert(!c.valid&&c.limits_ok&&c.aspect_permille>RP_ASPECT_PERMILLE);
    c=check(2133,1200,3264,1800);assert(!c.valid&&c.limits_ok&&c.aspect_permille==20&&strstr(c.problem,"2.0%"));
    /* READY lines: the installed u05 line, and a two-pair line as a two-mode build could write it. */
    rp_pair pairs[4];
    assert(rp_parse_pairs("HelixSR r17, flash fix + faster encoders (off = stock FSR2), 1505x847 -> 2560x1440",pairs,4)==1);
    assert(pairs[0].input_width==1505&&pairs[0].output_height==1440);
    assert(rp_parse_pairs("HelixSR r18, 1505x847 -> 2560x1440 + 2258x1270 -> 3840x2160",pairs,4)==2&&pairs[1].output_width==3840);
    assert(!rp_parse_pairs("no sizes here",pairs,4)&&!rp_parse_pairs("2560x1440 -> 1280x720",pairs,4));
    /* Known pairs. */
    assert(rp_known("PPSA34547","01.300.300",0)->package==RP_PACKAGE_BUILT&&rp_known("PPSA34547","01.300.300",1)->package==RP_PACKAGE_BUILT);
    assert(rp_known("CUSA08809","01.00",0)->package==RP_PACKAGE_NONE&&rp_known("CUSA08809","01.00",0)->pair.input_width==2560);
    /* R18's READY names no sizes: its pairs come from the package table. */
    assert(rp_package_pairs("control-r18-20261029",pairs,4)==2&&pairs[1].input_width==2258&&!rp_package_pairs("control-r17-20261028",pairs,4));
    assert(!rp_known("PPSA34547","01.300.300",2)&&!rp_known("PPSA34547","01.400.000",0));
    assert(rp_known("PPSA07631","01.000.000",0)->package==RP_PACKAGE_TRIAL);
    assert(!rp_known("PPSA07631","01.000.001",0));
    assert(rp_package_pairs("saros-native-v1",pairs,4)==4&&pairs[0].input_width==1536&&pairs[3].output_height==2160);
    /* HRO1: an odd custom render size is valid; presets stay even; odd outputs stay invalid. */
    ro_options o;ro_defaults(&o);
    o.preset=RO_CUSTOM;o.input_width=1505;o.input_height=847;o.output_width=2560;o.output_height=1440;assert(ro_valid(&o));
    o.preset=RO_BALANCED;assert(!ro_valid(&o));
    o.preset=RO_CUSTOM;o.output_width=2561;assert(!ro_valid(&o));
    ro_defaults(&o);o.output_width=3840;o.output_height=2160;assert(!ro_apply_preset(&o,RO_QUALITY)&&o.input_width==2560&&o.input_height==1440);
    printf("resolution plan: %u planner comparisons, PASS\n",compared);
    return 0;
}
