#include "render_options.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Isolated native adapter exercise; never writes a user's selected profile. */
int helix_native_render_check(FILE *log,const ap_identity *id){
 const char *root="/data/helixsr-speed/render-selftest/render-settings";
 mkdir("/data/helixsr-speed/render-selftest",0700);mkdir(root,0700);
 ro_options next,readback;int rc=ro_load(root,id,&next);unsigned checked=0;
 if(rc<0){fprintf(log,"render_check=load_failed rc=%d\n",rc);return rc;}
 fprintf(log,"render_prior revision=%llu input=%ux%u output=%ux%u\n",(unsigned long long)next.revision,next.input_width,next.input_height,next.output_width,next.output_height);
 for(unsigned backend=0;backend<6;backend++)for(unsigned preset=0;preset<6;preset++){
  next.backend=backend;next.output_width=3840;next.output_height=2160;
  if(preset==RO_CUSTOM){next.input_width=1920;next.input_height=1080;}
  rc=ro_apply_preset(&next,preset);
  if(!rc)rc=ro_save(root,id,&next,next.revision);
  if(!rc)rc=ro_load(root,id,&readback);
  if(rc||memcmp(&next,&readback,sizeof next)){
   fprintf(log,"render_check=failed backend=%u preset=%u rc=%d\n",backend,preset,rc);return -1;
  }
  checked++;
 }
 fprintf(log,"render_check=passed combinations=%u revision=%llu last_input=%ux%u last_output=%ux%u isolated=1\n",checked,(unsigned long long)next.revision,next.input_width,next.input_height,next.output_width,next.output_height);
 return 0;
}

