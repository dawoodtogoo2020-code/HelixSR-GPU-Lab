/* SPDX-License-Identifier: MIT */
#include "adapter_contract.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static hc_descriptor d; static hc_frame f; static hc_previous p;
static void init(void) {
 memset(&d,0,sizeof d);memset(&f,0,sizeof f);memset(&p,0,sizeof p);
 strcpy(d.identity.device,"ps5-regular");strcpy(d.identity.title,"PPSA07631");strcpy(d.identity.build,"01.000.000");memset(d.identity.executable_hash,'a',64);memset(d.backend_hash,'b',64);memset(d.model_hash,'c',64);d.adapter_qualified=d.backend_qualified=1;d.required_wave_size=32;d.required_math_mask=HC_MATH_FP16_ARITHMETIC;
 d.min_input_width=1;d.min_input_height=1;d.max_input_width=3840;d.max_input_height=2160;d.min_output_width=1;d.min_output_height=1;d.max_output_width=3840;d.max_output_height=2160;
 d.direction=HC_MOTION_CURRENT_TO_PREVIOUS;d.scale=HC_SCALE_PIXELS;d.motion_resolution=HC_MRES_INPUT;d.jitter_units=HC_JITTER_PIXELS;d.depth=HC_DEPTH_INVERTED;d.depth_encoding=HC_DEPTH_DEVICE_0_1;d.color=HC_COLOR_LINEAR;d.exposure=HC_EXPOSURE_SCALAR;d.required_mask=HC_MANDATORY; for(unsigned i=0;i<HC_RESOURCE_COUNT;i++) d.allowed_formats[i]=0x1feu;
 f.identity=d.identity;memcpy(f.backend_hash,d.backend_hash,65);memcpy(f.model_hash,d.model_hash,65);f.capabilities_confirmed=1;f.observed_wave_size=32;f.available_math_mask=HC_MATH_FP16_ARITHMETIC|HC_MATH_FP32_ARITHMETIC;f.frame_id=10;f.previous_frame_id=9;f.history_id=3;f.input_width=1505;f.input_height=847;f.output_width=2560;f.output_height=1440;f.frame_time_ms=16;f.exposure_value=1;f.barriers_confirmed=f.lifetimes_confirmed=f.output_non_aliasing_confirmed=1;f.present_mask=HC_MANDATORY;
 f.resources[0]=(hc_resource){1,1505,847,HC_FMT_RGBA16F,0,0};f.resources[1]=(hc_resource){2,1505,847,HC_FMT_D32F,0,0};f.resources[2]=(hc_resource){3,1505,847,HC_FMT_RG16F,0,0};f.resources[4]=(hc_resource){4,2560,1440,HC_FMT_RGBA16F,0,0};
}
static void prior(void){p.has_previous=1;p.observed_wave_size=f.observed_wave_size;p.available_math_mask=f.available_math_mask;p.required_wave_size=d.required_wave_size;p.required_math_mask=d.required_math_mask;p.direction=d.direction;p.scale=d.scale;p.motion_resolution=d.motion_resolution;p.jitter_units=d.jitter_units;p.depth=d.depth;p.depth_encoding=d.depth_encoding;p.motion_vectors_include_jitter=d.motion_vectors_include_jitter;p.required_mask=d.required_mask;p.present_mask=f.present_mask;memcpy(p.resources,f.resources,sizeof p.resources);p.color=d.color;p.exposure=d.exposure;p.identity=f.identity;memcpy(p.backend_hash,f.backend_hash,65);memcpy(p.model_hash,f.model_hash,65);p.frame_id=9;p.history_id=f.history_id;p.input_width=f.input_width;p.input_height=f.input_height;p.output_width=f.output_width;p.output_height=f.output_height;}
#define CHECK(expected) assert(hc_check(&d,&f,&p).result==(expected))
int main(void){
 init();CHECK(HC_ACCEPT);assert(hc_check(&d,&f,&p).needs_history_reset==1);
 init();prior();CHECK(HC_ACCEPT);assert(!hc_check(&d,&f,&p).needs_history_reset);
 init();d.adapter_qualified=0;CHECK(HC_UNQUALIFIED);
 init();d.backend_qualified=2;CHECK(HC_UNQUALIFIED);
 init();d.identity.device[0]=0;CHECK(HC_INVALID);
 init();memset(d.identity.build,'x',HC_KEY_SIZE);CHECK(HC_INVALID);
 init();d.identity.title[0]=' ';CHECK(HC_INVALID);
 init();f.identity.title[0]='X';CHECK(HC_IDENTITY_MISMATCH);
 init();f.identity.executable_hash[0]='z';CHECK(HC_INVALID);
 init();f.backend_hash[0]='d';CHECK(HC_IDENTITY_MISMATCH);
 init();d.model_hash[0]='X';CHECK(HC_INVALID);
 init();d.direction=0;CHECK(HC_UNSUPPORTED);
 init();d.scale=0;CHECK(HC_UNSUPPORTED);
 init();d.motion_resolution=0;CHECK(HC_UNSUPPORTED);
 init();d.jitter_units=0;CHECK(HC_UNSUPPORTED);
 init();d.depth=0;CHECK(HC_UNSUPPORTED);
 init();d.color=0;CHECK(HC_UNSUPPORTED);
 init();d.exposure=0;CHECK(HC_UNSUPPORTED);
 init();d.required_mask=0;CHECK(HC_INVALID);
 init();d.exposure=HC_EXPOSURE_TEXTURE;CHECK(HC_INVALID);
 init();f.input_width=0;CHECK(HC_UNSUPPORTED);
 init();f.input_width=3000;CHECK(HC_UNSUPPORTED);
 init();f.jitter_x=NAN;CHECK(HC_INVALID);
 init();f.jitter_y=INFINITY;CHECK(HC_INVALID);
 init();f.frame_time_ms=0;CHECK(HC_INVALID);
 init();f.exposure_value=NAN;CHECK(HC_INVALID);
 init();f.barriers_confirmed=0;CHECK(HC_UNSAFE);
 init();f.lifetimes_confirmed=0;CHECK(HC_UNSAFE);
 init();f.present_mask&=~HC_BIT(HC_MOTION_IN);CHECK(HC_MISSING_RESOURCE);
 init();f.resources[0].handle=0;CHECK(HC_MISSING_RESOURCE);
 init();f.resources[0].format=HC_FMT_UNKNOWN;CHECK(HC_UNSUPPORTED);
 init();f.resources[1].width=1506;CHECK(HC_UNSUPPORTED);
 init();f.resources[4].handle=1;CHECK(HC_ALIAS);
 init();f.resources[0].stride=10;f.resources[0].bytes=2;CHECK(HC_INVALID);
 init();prior();p.frame_id=10;CHECK(HC_STALE);
 init();prior();p.history_id=2;CHECK(HC_ACCEPT);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();p.history_id=2;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();p.input_width=1504;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();p.backend_hash[0]='d';p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();p.direction=HC_MOTION_PREVIOUS_TO_CURRENT;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();p.work_in_flight=1;CHECK(HC_ACCEPT);
 init();p.work_in_flight=1;CHECK(HC_INVALID);
 init();d.depth_encoding=HC_DEPTH_ENCODING_UNKNOWN;CHECK(HC_UNSUPPORTED);
 init();d.motion_vectors_include_jitter=2;CHECK(HC_INVALID);
 init();d.allowed_formats[HC_MOTION_IN]=HC_FORMAT_BIT(HC_FMT_RG16F);f.resources[HC_MOTION_IN].format=HC_FMT_RG32F;CHECK(HC_UNSUPPORTED);
 init();d.allowed_formats[HC_MOTION_IN]=HC_FORMAT_BIT(HC_FMT_RG16F);CHECK(HC_ACCEPT);
 init();d.required_mask|=HC_BIT(HC_REACTIVE_IN);CHECK(HC_MISSING_RESOURCE);
 init();f.present_mask|=HC_BIT(HC_REACTIVE_IN);f.resources[HC_REACTIVE_IN]=(hc_resource){5,1505,847,HC_FMT_R8_UNORM,0,0};CHECK(HC_ACCEPT);
 init();f.present_mask|=HC_BIT(HC_COMPOSITION_IN);f.resources[HC_COMPOSITION_IN]=(hc_resource){6,1505,847,HC_FMT_R16F,0,0};CHECK(HC_ACCEPT);
 init();f.present_mask|=HC_BIT(HC_COMPOSITION_IN);f.resources[HC_COMPOSITION_IN]=(hc_resource){4,1505,847,HC_FMT_R16F,0,0};CHECK(HC_ALIAS);
 init();f.resources[0].stride=1;f.resources[0].bytes=1;CHECK(HC_INVALID);
 init();f.resources[0].stride=12040;f.resources[0].bytes=(uint64_t)12040*847;CHECK(HC_ACCEPT);
 init();f.resources[0].stride=12040;f.resources[0].bytes=1;CHECK(HC_INVALID);
 init();f.resources[0].stride=12040;f.resources[0].bytes=0;CHECK(HC_INVALID);
 init();prior();f.resources[0].format=HC_FMT_RGBA32F;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();f.resources[0].format=HC_FMT_RGBA32F;CHECK(HC_ACCEPT);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();f.resources[0].stride=12040;f.resources[0].bytes=(uint64_t)12040*847;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();d.motion_vectors_include_jitter=1;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();d.required_mask|=HC_BIT(HC_REACTIVE_IN);f.present_mask|=HC_BIT(HC_REACTIVE_IN);f.resources[HC_REACTIVE_IN]=(hc_resource){5,1505,847,HC_FMT_R8_UNORM,0,0};p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();f.output_non_aliasing_confirmed=0;CHECK(HC_UNSAFE);
 init();f.output_non_aliasing_confirmed=2;CHECK(HC_UNSAFE);
 init();prior();p.frame_id=7;f.previous_frame_id=9;CHECK(HC_ACCEPT);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();p.frame_id=7;f.previous_frame_id=9;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();p.frame_id=7;f.previous_frame_id=7;CHECK(HC_ACCEPT);assert(!hc_check(&d,&f,&p).needs_history_reset);
 /* Synthetic routing metadata only: no real hardware is tested. */
 init();d.required_wave_size=64;f.observed_wave_size=64;d.required_math_mask=HC_MATH_FP32_ARITHMETIC;f.available_math_mask=HC_MATH_FP32_ARITHMETIC;CHECK(HC_ACCEPT);
 init();f.capabilities_confirmed=0;CHECK(HC_UNSAFE);
 init();f.capabilities_confirmed=2;CHECK(HC_UNSAFE);
 init();f.observed_wave_size=0;CHECK(HC_UNSUPPORTED);
 init();d.required_wave_size=16;CHECK(HC_UNSUPPORTED);
 init();f.observed_wave_size=64;CHECK(HC_UNSUPPORTED);
 init();d.required_math_mask=HC_MATH_INT_DOT4;CHECK(HC_UNSUPPORTED);
 init();d.required_math_mask=1u<<30;CHECK(HC_UNSUPPORTED);
 init();f.available_math_mask|=1u<<30;CHECK(HC_UNSUPPORTED);
 init();prior();f.available_math_mask|=HC_MATH_INT_DOT4;CHECK(HC_ACCEPT);assert(hc_check(&d,&f,&p).needs_history_reset);
 init();prior();f.available_math_mask|=HC_MATH_INT_DOT4;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 init();prior();d.required_wave_size=64;f.observed_wave_size=64;p.work_in_flight=1;CHECK(HC_DRAIN_REQUIRED);
 puts("adapter contract tests passed");return 0;
}
