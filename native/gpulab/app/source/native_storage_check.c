#include "tiny_store.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
int helix_native_storage_check(FILE *log){
 const char*root="/data/helixsr-speed/settings-selftest";
 const char*ns="eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
 struct record {char magic[8];uint64_t pid,counter;} before={0},next={{'H','E','L','I','X','T','S','1'},0,1},readback={0};
 mkdir(root,0700);ts_store*s=ts_open(root,ns);if(!s){fprintf(log,"storage_check=open_failed\n");return -1;}
 size_t n=0;int r=ts_read(s,"settings",(unsigned char*)&before,sizeof before,&n);
 fprintf(log,"storage_prior read=%d bytes=%zu pid=%llu counter=%llu\n",r,n,(unsigned long long)before.pid,(unsigned long long)before.counter);
 if(r<0||(r==1&&(n!=sizeof before||memcmp(before.magic,next.magic,8)))){ts_close(s);return -2;}
 next.pid=getpid();if(r==1)next.counter=before.counter+1;
 ts_store*second=ts_open(root,ns);if(second){ts_close(second);ts_close(s);fprintf(log,"storage_check=lock_failed\n");return -3;}
 size_t wrote=ts_stage(s,"settings",(const unsigned char*)&next,sizeof next);int sync=-1,commit=-1,dir=-1;
 if(wrote==sizeof next)sync=ts_sync_stage(s,"settings");if(!sync)commit=ts_commit(s,"settings");if(!commit)dir=ts_sync_directory(s,"settings");
 int closed=ts_close(s);fprintf(log,"storage_write bytes=%zu sync=%d commit=%d dirsync=%d close=%d\n",wrote,sync,commit,dir,closed);
 if(wrote!=sizeof next||sync||commit||dir||closed)return -4;
 s=ts_open(root,ns);if(!s)return -5;r=ts_read(s,"settings",(unsigned char*)&readback,sizeof readback,&n);closed=ts_close(s);
 int ok=r==1&&n==sizeof next&&!memcmp(&next,&readback,sizeof next)&&!closed;
 fprintf(log,"storage_check=%s reopened_pid=%llu counter=%llu\n",ok?"passed":"readback_failed",(unsigned long long)readback.pid,(unsigned long long)readback.counter);return ok?0:-6;
}
#include "app_session.h"
static int diagnostic_admit(void *context,const char *root,const ap_identity *id){return !strcmp(root,"/data/helixsr-speed/settings-selftest")&&ps_identity_equal((const ap_identity*)context,id);}
int helix_native_profile_check(FILE*log,const ap_identity*id){
 const char*root="/data/helixsr-speed/settings-selftest";as_session a=AS_SESSION_INITIALIZER,b=AS_SESSION_INITIALIZER;as_view v;
 as_result r=as_open(&a,root,id,diagnostic_admit,(void*)id,1);
 if(r||!as_query(&a,&v)){fprintf(log,"profile_check=open_failed result=%d\n",r);as_close(&a,2);return -1;}
 fprintf(log,"profile_prior desired=%d revision=%llu\n",v.preference.desired,(unsigned long long)v.preference.revision);
 r=as_save_preference(&a,AP_OFF,0,v.preference.revision);as_query(&a,&v);
 if(!r)r=as_save_preference(&a,AP_ON,0,v.preference.revision);
 as_result closed=as_close(&a,2);if(r||closed){fprintf(log,"profile_check=save_failed result=%d close=%d\n",r,closed);return -2;}
 r=as_open(&b,root,id,diagnostic_admit,(void*)id,3);int ok=!r&&as_query(&b,&v)&&v.preference_valid&&v.preference.desired==AP_ON;
 closed=as_close(&b,4);fprintf(log,"profile_check=%s desired=%d revision=%llu close=%d\n",ok&&!closed?"passed":"failed",v.preference.desired,(unsigned long long)v.preference.revision,closed);return ok&&!closed?0:-3;
}
