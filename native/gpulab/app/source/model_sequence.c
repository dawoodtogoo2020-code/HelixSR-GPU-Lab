/* SPDX-License-Identifier: GPL-3.0-or-later
 * Diagnostic persistent sequence. Descriptor/barrier packing follows the
 * existing PS5_Vulkan ps5vk_draw.c, not backwards-compatible GNM surfaces. */
#include "model_sequence.h"
#include "model_scratch.h"
#include "encoder_sha256.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "k8_dispatch_select.h"

enum { MS_ALIGN=65536,MS_COMMAND_BYTES=65536,MS_COMMAND_CAP=256,MS_GUARD=64,MS_TIMING_STRIDE=0x3000 };
struct timing_reading {uint64_t start,end;unsigned char zpass[0x400];};
enum { MS_SCRATCH_BYTES=25296896,MS_SCRATCH_OFFSET=65536,MS_SCRATCH_PAYLOAD=25159680 };
struct region { size_t offset,bytes; };
struct runtime_resource { struct region region;size_t pitch,row_bytes; };
static int add_size(size_t a,size_t b,size_t*out){if(a>SIZE_MAX-b)return -1;*out=a+b;return 0;}
static int mul_size(size_t a,size_t b,size_t*out){if(b&&a>SIZE_MAX/b)return -1;*out=a*b;return 0;}
static int aligned(size_t n,size_t a,size_t*out){size_t t;if(add_size(n,a-1,&t))return -1;*out=t&~(a-1);return 0;}
static unsigned bpp(uint32_t f){return f==10?8:f==34||f==41?4:f==54?2:0;}
static int file_valid(const struct ms_file*f){
 if(!f||!f->name||!f->name[0]||!f->sha256||strlen(f->sha256)!=64||!f->bytes||f->bytes>SIZE_MAX)return 0;
 for(unsigned i=0;i<64;i++)if(!((f->sha256[i]>='0'&&f->sha256[i]<='9')||(f->sha256[i]>='a'&&f->sha256[i]<='f')))return 0;
 return 1;
}
static int digest(const void*d,size_t n,const char*h){char s[65];encoder_sha256_hex(d,n,s);return !strcmp(s,h);}
static void flush(const void*base,size_t bytes){
 uintptr_t a=(uintptr_t)base&~(uintptr_t)63,end=(uintptr_t)base+bytes;
 for(;a<end;a+=64)__asm__ volatile("clflush (%0)"::"r"(a):"memory");
 __asm__ volatile("mfence":::"memory");
}
static int shape(const struct ms_resource*r,size_t*row,size_t*pitch,size_t*storage){
 if(!file_valid(&r->initial)||r->flags&~MS_IMMUTABLE)return -1;
 if(r->kind==MS_BUFFER){if(r->width||r->height||r->dxgi_format||r->initial.bytes>UINT32_MAX)return -1;*row=*pitch=0;*storage=(size_t)r->initial.bytes;return 0;}
 unsigned n=bpp(r->dxgi_format);size_t logical;
 if(r->kind!=MS_IMAGE||!n||!r->width||!r->height||r->width>16384||r->height>16384||
    mul_size(r->width,n,row)||aligned(*row,256,pitch)||mul_size(*row,r->height,&logical)||logical!=r->initial.bytes||mul_size(*pitch,r->height,storage))return -1;
 return 0;
}
int model_sequence_descriptor(uint32_t out[8],const struct ms_resource*r,uint64_t address,uint64_t storage,uint64_t pitch){
 uint32_t d[8]={0};size_t row,rp,bytes;
 if(!out||!r||shape(r,&row,&rp,&bytes)||!address||address>=(UINT64_C(1)<<48)||storage!=bytes||address>(UINT64_C(1)<<48)-storage)return -1;
 if(r->kind==MS_BUFFER){if(pitch)return -1;d[0]=(uint32_t)address;d[1]=(uint32_t)(address>>32);d[2]=(uint32_t)bytes;d[3]=0x31016fac;}
 else{
  if(address&255||pitch!=rp)return -1;
  unsigned format=r->dxgi_format==10?71:r->dxgi_format==34?29:r->dxgi_format==41?22:13;
  unsigned swizzle=r->dxgi_format==10?0xfac:r->dxgi_format==34?0x22c:0x204;
  d[0]=(uint32_t)(address>>8);d[1]=(format<<20)|(((r->width-1)&3)<<30)|(uint32_t)(address>>40);
  d[2]=((r->width-1)>>2)|((r->height-1)<<14)|0x80000000u;d[3]=0x90000000u|swizzle;
  d[4]=rp==row?0:(uint32_t)(rp/bpp(r->dxgi_format)-1);d[5]=0x00400000;
 }
 memcpy(out,d,sizeof(d));return 0;
}
static int rows(void*to,size_t to_bytes,const void*from,size_t from_bytes,uint32_t w,uint32_t h,uint32_t bytes,int pack){
 size_t row,pitch,tight,storage;
 if(!to||!from||!w||!h||!(bytes==2||bytes==4||bytes==8)||mul_size(w,bytes,&row)||aligned(row,256,&pitch)||mul_size(row,h,&tight)||mul_size(pitch,h,&storage)||to_bytes!=(pack?storage:tight)||from_bytes!=(pack?tight:storage))return -1;
 for(uint32_t y=0;y<h;y++)memcpy((unsigned char*)to+y*(pack?pitch:row),(const unsigned char*)from+y*(pack?row:pitch),row);
 return 0;
}
int model_sequence_pack_rows(void*t,size_t tn,const void*s,size_t sn,uint32_t w,uint32_t h,uint32_t p){return rows(t,tn,s,sn,w,h,p,1);}
int model_sequence_unpack_rows(void*t,size_t tn,const void*s,size_t sn,uint32_t w,uint32_t h,uint32_t p){return rows(t,tn,s,sn,w,h,p,0);}
static int reserve(size_t*cursor,size_t bytes,struct region*r){
 size_t n;if(add_size(*cursor,MS_GUARD,&n)||aligned(n,256,&r->offset)||add_size(r->offset,bytes,&n)||add_size(n,MS_GUARD,cursor))return -1;r->bytes=bytes;return 0;
}
static uint32_t role_index(const struct ms_fixture*f,uint32_t role,unsigned parity){return parity?f->roles[role].resource_odd:f->roles[role].resource_even;}
static int range(uint64_t offset,uint64_t bytes,uint64_t extent){return bytes&&offset<=extent&&bytes<=extent-offset;}
static int checkpoints_valid(const struct ms_fixture*f,const struct ms_checkpoint*c,uint32_t count){
 if(count&&!c)return -1;
 for(uint32_t i=0;i<count;i++){
  if(c[i].role>=f->role_count||!file_valid(&c[i].expected)||c[i].numeric_format>MS_E5M3||(!c[i].numeric_format&&c[i].require_final_psnr))return -1;
  unsigned step=c[i].numeric_format==MS_F16?2:c[i].numeric_format==MS_F32?4:1;
  if(c[i].logical_offset%step||c[i].expected.bytes%step)return -1;
  for(unsigned p=0;p<2;p++)if(!range(c[i].logical_offset,c[i].expected.bytes,f->resources[role_index(f,c[i].role,p)].initial.bytes))return -1;
 }
 return 0;
}
static int scratch_shader_valid(const struct ms_shader*s){
 struct model_scratch_plan plan;
 /* Reviewed exponent-tag variant retains the identical scratch ABI/prologue. */
 int packed_code=s->code.bytes==MS_SCRATCH_K10_PACKED_CODE_BYTES&&!strcmp(s->code.sha256,MS_SCRATCH_K10_PACKED_CODE_SHA256);
 int qualified_code=(s->code.bytes==MODEL_SCRATCH_K10_CODE_BYTES&&!strcmp(s->code.sha256,MS_SCRATCH_K10_CODE_SHA256))||
  (s->code.bytes==MS_SCRATCH_K10_TAGS_CODE_BYTES&&!strcmp(s->code.sha256,MS_SCRATCH_K10_TAGS_CODE_SHA256));
 return !model_scratch_full_ring_requirements(&plan)&&
  /* The packed variant needs 5120 B/wave. Keep the proven 6144 B/wave
   * ring and its unchanged allocation/address ABI; the extra space is safe. */
  ((qualified_code&&s->scratch_bytes_per_wave==plan.bytes_per_wave)||
   (packed_code&&s->scratch_bytes_per_wave==5120&&plan.bytes_per_wave>=5120))&&s->rsrc1==plan.rsrc1_value&&s->rsrc2==plan.rsrc2_value&&
  s->user_slot==plan.descriptor_user_slot&&s->block[0]==128&&s->block[1]==1&&s->block[2]==1;
}
static int manifest_valid(const struct ms_fixture*f,size_t*max_args,size_t*max_table,size_t*max_file,int*needs_scratch){
 if(!f||f->abi!=MS_FIXTURE_ABI||!f->resources||!f->resource_count||f->resource_count>256||!f->roles||!f->role_count||f->role_count>512||!f->shaders||!f->shader_count||f->shader_count>128||!f->launches||!f->launch_count||f->launch_count>8192||!f->frames||!f->frame_count||f->frame_count>256)return -1;
 *max_args=*max_table=*max_file=0;*needs_scratch=0;
 for(uint32_t i=0;i<f->resource_count;i++){size_t row,pitch,n;if(shape(f->resources+i,&row,&pitch,&n))return -1;if(f->resources[i].initial.bytes>*max_file)*max_file=(size_t)f->resources[i].initial.bytes;}
 for(uint32_t i=0;i<f->role_count;i++)if(f->roles[i].resource_even>=f->resource_count||f->roles[i].resource_odd>=f->resource_count)return -1;
 for(uint32_t i=0;i<f->shader_count;i++){
  const struct ms_shader*s=f->shaders+i;uint64_t threads=(uint64_t)s->block[0]*s->block[1]*s->block[2];unsigned users=(s->rsrc2>>1)&31;
  if(!file_valid(&s->code)||(s->code.bytes&3)||!s->block[0]||!s->block[1]||!s->block[2]||threads>1024||s->block[0]>1024||s->block[1]>1024||s->block[2]>64||s->user_slot>=16||users<=s->user_slot||users>16)return -1;
 }
 for(uint32_t i=0;i<f->launch_count;i++){
  const struct ms_launch*l=f->launches+i;
  if(l->shader>=f->shader_count||!file_valid(&l->args)||l->args.bytes>UINT32_MAX||!l->grid[0]||!l->grid[1]||!l->grid[2]||l->grid[0]>65535||l->grid[1]>65535||l->grid[2]>65535||!l->table_bytes||(l->table_bytes&15)||l->table_bytes>1048576||!l->bindings||!l->binding_count||l->binding_count>1024||checkpoints_valid(f,l->checkpoints,l->checkpoint_count))return -1;
  if(l->args.bytes>*max_args)*max_args=(size_t)l->args.bytes;
  if(l->table_bytes>*max_table)*max_table=l->table_bytes;
  for(uint32_t b=0;b<l->binding_count;b++){
   const struct ms_binding*v=l->bindings+b;unsigned bytes=v->kind==MS_DESC_IMAGE?32:16;
   if((v->kind!=MS_DESC_BUFFER&&v->kind!=MS_DESC_IMAGE&&v->kind!=MS_DESC_SAMPLER)||(v->table_offset&15)||!range(v->table_offset,bytes,l->table_bytes))return -1;
   for(uint32_t j=0;j<b;j++){const struct ms_binding*u=l->bindings+j;unsigned un=u->kind==MS_DESC_IMAGE?32:16;if(v->table_offset<u->table_offset+un&&u->table_offset<v->table_offset+bytes)return -1;}
   if(v->kind==MS_DESC_SAMPLER){if(v->role||v->view_offset||v->view_bytes)return -1;continue;}
   if(v->role==MS_ARGS_ROLE){if(v->kind!=MS_DESC_BUFFER||!range(v->view_offset,v->view_bytes,l->args.bytes))return -1;continue;}
   if(v->role>=f->role_count)return -1;
   for(unsigned p=0;p<2;p++){
    const struct ms_resource*r=f->resources+role_index(f,v->role,p);
    if(v->kind==MS_DESC_BUFFER){if(r->kind!=MS_BUFFER||v->view_bytes>UINT32_MAX||!range(v->view_offset,v->view_bytes,r->initial.bytes))return -1;}
    else if(r->kind!=MS_IMAGE||v->view_offset||v->view_bytes!=r->initial.bytes)return -1;
   }
  }
 }
 uint32_t covered=0;
 for(uint32_t i=0;i<f->frame_count;i++){
  const struct ms_frame*fr=f->frames+i;
  if(!fr->launch_count||fr->first_launch!=covered||fr->first_launch>f->launch_count||fr->launch_count>f->launch_count-fr->first_launch||(fr->upload_count&&!fr->uploads)||checkpoints_valid(f,fr->checkpoints,fr->checkpoint_count))return -1;
  covered+=fr->launch_count;
  for(uint32_t j=0;j<fr->launch_count;j++){
   const struct ms_shader*s=f->shaders+f->launches[fr->first_launch+j].shader;
   if(s->scratch_bytes_per_wave||(s->rsrc2&1)){if(!scratch_shader_valid(s))return -1;*needs_scratch=1;}
  }
  for(uint32_t j=0;j<fr->upload_count;j++){
   const struct ms_upload*u=fr->uploads+j;if(u->role>=f->role_count||!file_valid(&u->file))return -1;
   for(unsigned p=0;p<2;p++){const struct ms_resource*r=f->resources+role_index(f,u->role,p);if((r->flags&MS_IMMUTABLE)||!range(u->logical_offset,u->file.bytes,r->initial.bytes)||(r->kind==MS_IMAGE&&(u->logical_offset||u->file.bytes!=r->initial.bytes)))return -1;}
   if(u->file.bytes>*max_file)*max_file=(size_t)u->file.bytes;
  }
 }
 return covered==f->launch_count?0:-1;
}
static int load_file(const struct encoder_ops*p,const struct ms_file*file,void*to){return p->load(p->ctx,file->name,to,(size_t)file->bytes)||!digest(to,(size_t)file->bytes,file->sha256)?-1:0;}
static int upload(const struct ms_resource*r,const struct runtime_resource*rr,unsigned char*base,size_t offset,const void*from,size_t n){
 if(r->kind==MS_BUFFER){memcpy(base+rr->region.offset+offset,from,n);return 0;}
 return model_sequence_pack_rows(base+rr->region.offset,rr->region.bytes,from,n,r->width,r->height,bpp(r->dxgi_format));
}
static void sh(uint32_t*p,size_t*n,uint32_t reg,const uint32_t*v,size_t count){p[(*n)++]=0xc0007600u|((uint32_t)count<<16);p[(*n)++]=reg;memcpy(p+*n,v,count*4);*n+=count;}
static void release_words(uint32_t*p,size_t*n,uint64_t fence,uint32_t token,uint32_t control){const uint32_t v[]={0xc0064900,control,0x20000000,(uint32_t)fence,(uint32_t)(fence>>32),token,0,0};memcpy(p+*n,v,sizeof(v));*n+=8;}
static void wait_words(uint32_t*p,size_t*n,uint64_t address,uint32_t token){const uint32_t w[]={0xc0079300,0x06000113,(uint32_t)address,(uint32_t)(address>>32),token,0,UINT32_MAX,0,0x40};memcpy(p+*n,w,sizeof(w));*n+=9;}
static void timestamp_words(uint32_t*p,size_t*n,uint64_t t,uint64_t z){const uint32_t v[]={0xc0024600,0x115,(uint32_t)z,(uint32_t)(z>>32),0xc0064900,0x0030c528,0x63000000,(uint32_t)t,(uint32_t)(t>>32),0,0,0};memcpy(p+*n,v,sizeof(v));*n+=12;}
static size_t commands(uint32_t*p,const struct ms_shader*s,const struct ms_launch*l,uint64_t code,uint64_t table,uint64_t barrier,uint64_t fence,uint32_t token,const struct model_scratch_plan*scratch,uint64_t timing){
 uint32_t zero[4]={0},program[]={(uint32_t)(code>>8),(uint32_t)(code>>40)},resources[]={s->rsrc1,s->rsrc2},user[16]={0},dest[]={UINT32_MAX,UINT32_MAX};size_t n=0;
 unsigned users=(s->rsrc2>>1)&31;user[s->user_slot]=(uint32_t)table;
 if(s->scratch_bytes_per_wave){
  if(!scratch||!scratch->bound||scratch->user_data_first!=0||scratch->user_data_count!=2||s->user_slot!=2)return 0;
  user[0]=scratch->user_data[0];user[1]=scratch->user_data[1];
 }
 sh(p,&n,0x204,zero,3);sh(p,&n,0x207,s->block,3);sh(p,&n,0x20c,program,2);sh(p,&n,0x212,resources,2);sh(p,&n,0x215,zero,1);sh(p,&n,0x228,zero,1);
 sh(p,&n,0x240,user,users);sh(p,&n,0x216,dest,2);sh(p,&n,0x219,zero,2);sh(p,&n,0x224,zero,4);
 uint32_t ring=s->scratch_bytes_per_wave?scratch->tmpring_value:0;
 sh(p,&n,0x218,&ring,1);
 if(timing){p[n++]=0xc0004600;p[n++]=0x407;timestamp_words(p,&n,timing,timing+0x1000);release_words(p,&n,timing+0x80,token,0x0000c514);wait_words(p,&n,timing+0x80,token);}
 p[n++]=0xc0031500;for(unsigned i=0;i<3;i++)p[n++]=l->grid[i];p[n++]=0x8041;
 p[n++]=0xc0004600;p[n++]=0x407;
 sh(p,&n,0x218,zero,1);
 if(timing)timestamp_words(p,&n,timing+0x40,timing+0x2000);
 /* Exact driver R70 release+PFP WAIT_REG_MEM64; final separate marker makes
  * the CPU completion follow the barrier wait, rather than precede it. */
 release_words(p,&n,barrier,token,0x0000c514);
 wait_words(p,&n,barrier,token);
 release_words(p,&n,fence,token,0x0030c514);return n<=MS_COMMAND_CAP?n:0;
}
static uint64_t timing_read(const unsigned char*p){return __atomic_load_n((const uint64_t*)p,__ATOMIC_ACQUIRE);}
static int timing_mutable(size_t at,uint32_t count){size_t i=at/MS_TIMING_STRIDE,o=at%MS_TIMING_STRIDE;return i<count&&(o<8||(o>=0x40&&o<0x48)||(o>=0x80&&o<0x88)||(o>=0x1000&&o<0x1200)||(o>=0x2000&&o<0x2200));}
/* completed records are immutable after their confirmed final fence. Current
 * and future records must remain zero until dispatched. Full mapping guards
 * include ample space beyond each source-observed0x200 ZPASS footprint. */
static int timing_check(FILE*log,const unsigned char*t,size_t bytes,const struct timing_reading*saved,uint32_t count,uint32_t completed,uint32_t current,int accept_current,struct ms_result*r){
 flush(t,bytes);unsigned bad=0;
 for(size_t i=0;i<bytes;i++)if(!timing_mutable(i,count))bad+=t[i]!=0xa5;
 r->guard_errors+=bad;
 for(uint32_t i=0;i<count;i++){const unsigned char*b=t+(size_t)i*MS_TIMING_STRIDE;uint64_t a=timing_read(b),z=timing_read(b+0x40),token=timing_read(b+0x80);if(i<completed){if(a!=saved[i].start||z!=saved[i].end||token!=0x4d530001u+i||memcmp(b+0x1000,saved[i].zpass,0x200)||memcmp(b+0x2000,saved[i].zpass+0x200,0x200))bad++;}else if(!(accept_current&&i==current)){if(a||z||token)bad++;for(size_t j=0;j<0x200;j++)bad+=(b[0x1000+j]!=0)+(b[0x2000+j]!=0);}}
 if(bad)fprintf(log,"MODEL_SEQUENCE gpu_timing_invalid frame_slot=%u completed=%u errors=%u\n",current,completed,bad);
 return bad?-1:0;
}
static unsigned guards(const unsigned char*a,size_t bytes,const struct region*r,size_t count,const struct ms_fixture*f,const struct runtime_resource*rr){
 unsigned bad=0;size_t cursor=0;
 for(size_t i=0;i<count;i++){for(size_t j=cursor;j<r[i].offset;j++)bad+=a[j]!=0xa5;cursor=r[i].offset+r[i].bytes;}
 for(size_t j=cursor;j<bytes;j++)bad+=a[j]!=0xa5;
 for(uint32_t i=0;i<f->resource_count;i++)if(f->resources[i].kind==MS_IMAGE)for(uint32_t y=0;y<f->resources[i].height;y++)for(size_t x=rr[i].row_bytes;x<rr[i].pitch;x++)bad+=a[rr[i].region.offset+y*rr[i].pitch+x]!=0xa5;
 return bad;
}
static double numeric(const unsigned char*p,unsigned format){
 if(format==MS_F32){float f;memcpy(&f,p,4);return f;}
 uint16_t h;if(format==MS_E5M3)h=(uint16_t)*p<<7;else memcpy(&h,p,2);
 unsigned e=(h>>10)&31,m=h&1023;double v=e==31?(m?NAN:INFINITY):e?ldexp((double)(1024+m),(int)e-25):ldexp((double)m,-24);return h&32768?-v:v;
}
static int check(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,const struct runtime_resource*rr,const unsigned char*a,const struct ms_checkpoint*c,uint32_t count,unsigned parity,struct ms_result*r,int collect){
 for(uint32_t i=0;i<count;i++){
  const struct ms_resource*res=f->resources+role_index(f,c[i].role,parity);const struct runtime_resource*rt=rr+role_index(f,c[i].role,parity);size_t n=(size_t)c[i].expected.bytes;unsigned char*expected=malloc(n);
  if(!expected)return -1;
  if(load_file(p,&c[i].expected,expected)){free(expected);return -1;}
  uint64_t mismatches=0,first=UINT64_MAX;unsigned format=c[i].numeric_format,step=format==MS_F32?4:format==MS_F16?2:1;size_t nonfinite=0;double error=0,reference=0,peak=1,max_abs=0;
  for(size_t j=0;j<n;j++){size_t logical=(size_t)c[i].logical_offset+j;size_t at=res->kind==MS_IMAGE?(logical/rt->row_bytes)*rt->pitch+logical%rt->row_bytes:logical;
   unsigned char actual=a[rt->region.offset+at];if(actual!=expected[j]){if(!mismatches)first=j;mismatches++;}
   if(format&&j%step==0){unsigned char value[4];for(unsigned k=0;k<step;k++){size_t lo=logical+k,where=res->kind==MS_IMAGE?(lo/rt->row_bytes)*rt->pitch+lo%rt->row_bytes:lo;value[k]=a[rt->region.offset+where];}double x=numeric(value,format),y=numeric(expected+j,format);if(!__builtin_isfinite(x)||!__builtin_isfinite(y))nonfinite++;else{double d=x-y;error+=d*d;reference+=y*y;if(fabs(y)>peak)peak=fabs(y);if(fabs(d)>max_abs)max_abs=fabs(d);}}
  }
  double relative=nonfinite?INFINITY:reference>0?sqrt(error/reference):error>0?INFINITY:0;
  double psnr=nonfinite?-INFINITY:error>0?10*log10(peak*peak/(error/(double)(n/step))):INFINITY;
  int failed=format?(nonfinite||!(relative<1e-3)||(c[i].require_final_psnr&&!(psnr>=45))):mismatches!=0;
  int continued=collect&&failed&&format&&!nonfinite&&reference>0&&__builtin_isfinite(relative)&&(__builtin_isfinite(psnr)||psnr==INFINITY);
  r->byte_mismatches+=mismatches;if(mismatches&&r->first_mismatch==UINT64_MAX)r->first_mismatch=first;
  if(format){if(relative>r->max_relative_l2)r->max_relative_l2=relative;if(c[i].require_final_psnr){r->final_psnr_checks++;if(psnr<r->min_final_psnr)r->min_final_psnr=psnr;}if(failed)r->numerical_failures++;}
  /* Reuse the CPU expected allocation only after all numeric reads finish. */
  for(size_t j=0;j<n;j++){size_t logical=(size_t)c[i].logical_offset+j;size_t at=res->kind==MS_IMAGE?(logical/rt->row_bytes)*rt->pitch+logical%rt->row_bytes:logical;expected[j]=a[rt->region.offset+at];}
  char name[128];int printed=snprintf(name,sizeof(name),"observed-f%u-l%u-r%u-o%"PRIu64".bin",r->frame,r->launch,c[i].role,c[i].logical_offset);
  if(printed<0||(size_t)printed>=sizeof(name)){free(expected);return -1;}
  int saved=p->save?p->save(p->ctx,name,expected,n):0;free(expected);
  fprintf(log,"MODEL_SEQUENCE checkpoint=%s frame=%u launch=%u role=%u offset=%"PRIu64" bytes=%zu mismatches=%"PRIu64" first=%"PRIu64" numeric_format=%u nonfinite=%zu relative_l2=%.12g max_abs=%.12g final_psnr_required=%u psnr_db=%.12g numerical_failed=%d diagnostic_collect=%d collect_continued=%d expected_file=%s observed_file=%s save_rc=%d\n",c[i].name?c[i].name:"",r->frame,r->launch,c[i].role,c[i].logical_offset,n,mismatches,first,format,nonfinite,relative,max_abs,(unsigned)c[i].require_final_psnr,psnr,format?failed:0,collect,continued&&!saved,c[i].expected.name,p->save?name:"disabled",saved);fflush(log);
  if(saved){r->save_errors++;return -3;}
  if(failed&&!continued)return -2;
 }
 return 0;
}
/* Explicit diagnostic only: capture complete mutable physical resources before
 * submission, after the preceding dispatch has fenced and external uploads
 * have completed. Never put expected/checkpoint data in this file. */
static void input_u32(unsigned char*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void input_u64(unsigned char*p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(8*i));}
static int input_layout(const struct ms_fixture*f,size_t*bytes,uint32_t*count){
 size_t payload=0,records;*count=0;
 for(uint32_t i=0;i<f->resource_count;i++)if(!(f->resources[i].flags&MS_IMMUTABLE)){
  if(add_size(payload,(size_t)f->resources[i].initial.bytes,&payload))return -1;
  ++*count;
 }
 if(!*count||mul_size(*count,32,&records)||add_size(records,64,&records)||add_size(records,payload,bytes)||*bytes>64u*1024u*1024u)return -1;
 return 0;
}
static int input_snapshot(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,const struct runtime_resource*rr,const unsigned char*a,struct ms_result*r,unsigned char*out,size_t bytes,uint32_t count){
 size_t cursor=64+(size_t)count*32;uint32_t record=0;
 memset(out,0,cursor);memcpy(out,"HSRINP1",7);input_u32(out+8,1);input_u32(out+12,64);
 input_u32(out+16,r->frame);input_u32(out+20,r->launch);input_u32(out+24,r->parity);input_u32(out+28,count);input_u64(out+32,bytes);input_u64(out+40,cursor);
 for(uint32_t i=0;i<f->resource_count;i++)if(!(f->resources[i].flags&MS_IMMUTABLE)){
  const struct ms_resource*res=f->resources+i;const struct runtime_resource*rt=rr+i;size_t n=(size_t)res->initial.bytes;
  if(record>=count||cursor>bytes||n>bytes-cursor)return -1;
  unsigned char*entry=out+64+(size_t)record++*32;input_u32(entry,i);input_u32(entry+4,res->kind);input_u64(entry+8,n);input_u64(entry+16,cursor);input_u32(entry+24,res->flags);
  const void*data=a+rt->region.offset;
  if(res->kind==MS_IMAGE){if(model_sequence_unpack_rows(out+cursor,n,data,rt->region.bytes,res->width,res->height,bpp(res->dxgi_format)))return -1;}
  else memcpy(out+cursor,data,n);
  cursor+=n;
 }
 if(record!=count||cursor!=bytes)return -1;
 char name[80];int printed=snprintf(name,sizeof(name),"inputs-f%u-l%u.bin",r->frame,r->launch);
 if(printed<0||(size_t)printed>=sizeof(name))return -1;
 int saved=p->save(p->ctx,name,out,bytes);
 fprintf(log,"MODEL_SEQUENCE input_snapshot frame=%u launch=%u parity=%u resources=%u bytes=%zu file=%s save_rc=%d\n",r->frame,r->launch,r->parity,count,bytes,name,saved);fflush(log);
 return saved?-1:0;
}
static int run_mode(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r,int collect,int timed,int capture_inputs,uint32_t input_first,uint32_t input_last,int frame_audit){
 if(!r)return -1;
 memset(r,0,sizeof(*r));r->rc=-1;r->first_mismatch=UINT64_MAX;r->min_final_psnr=INFINITY;
 if(!log||!p||!p->allocate||!p->release||!p->load||!p->submit||!p->suspend_point||!p->ns||!p->sleep_us)return -1;
 if(capture_inputs&&!p->save)return -1;
 if(frame_audit)fprintf(log,"MODEL_SEQUENCE diagnostic_collect=%d gpu_timed=%d capture_inputs=%d frame_audit=1\n",collect,timed,capture_inputs);
 else fprintf(log,"MODEL_SEQUENCE diagnostic_collect=%d gpu_timed=%d capture_inputs=%d\n",collect,timed,capture_inputs);
 fflush(log);
 uint64_t begin=p->ns(p->ctx),submitted=0;size_t args_max=0,table_max=0,file_max=0,arena_bytes=0,cursor=0,region_count=0;int needs_scratch=0;
 struct encoder_allocation arena={0},command={0},scratch={0},timing={0};struct model_scratch_plan scratch_plan={0};struct runtime_resource*rr=NULL;struct region*regions=NULL,*codes=NULL;unsigned char*temp=NULL,*table_saved=NULL,*input_copy=NULL;unsigned char*a=NULL,*cmd=NULL,*sp=NULL,*tp=NULL;struct region args={0},table={0};struct timing_reading*readings=NULL;size_t timing_bytes=0,input_bytes=0;uint32_t input_count=0;uint64_t total_gpu_ticks=0;
 if(manifest_valid(f,&args_max,&table_max,&file_max,&needs_scratch)){r->rc=-2;goto cleanup;}
 if(frame_audit){
  if(!timed||capture_inputs){r->rc=-2;goto cleanup;}
  for(uint32_t i=0;i<f->launch_count;i++)if(f->launches[i].checkpoint_count){r->rc=-2;goto cleanup;}
 }
 if(capture_inputs==2){if(input_first>input_last||input_last>=f->launch_count){r->rc=-2;goto cleanup;}fprintf(log,"MODEL_SEQUENCE input_range first=%u last=%u strict_checks=1\n",input_first,input_last);}
 if(capture_inputs){if(input_layout(f,&input_bytes,&input_count)){r->rc=-2;goto cleanup;}input_copy=malloc(input_bytes);if(!input_copy){r->rc=-3;goto cleanup;}}
 rr=calloc(f->resource_count,sizeof(*rr));codes=calloc(f->shader_count,sizeof(*codes));regions=calloc(f->resource_count+f->shader_count+2,sizeof(*regions));temp=malloc(file_max);table_saved=malloc(table_max);
 if(!rr||!codes||!regions||!temp||!table_saved){r->rc=-3;goto cleanup;}
 for(uint32_t i=0;i<f->resource_count;i++){size_t bytes;if(shape(f->resources+i,&rr[i].row_bytes,&rr[i].pitch,&bytes)||reserve(&cursor,bytes,&rr[i].region)){r->rc=-4;goto cleanup;}regions[region_count++]=rr[i].region;}
 for(uint32_t i=0;i<f->shader_count;i++){if(reserve(&cursor,(size_t)f->shaders[i].code.bytes,codes+i)){r->rc=-4;goto cleanup;}regions[region_count++]=codes[i];}
 if(reserve(&cursor,args_max,&args)||reserve(&cursor,table_max,&table)||aligned(cursor,MS_ALIGN,&arena_bytes)||arena_bytes>UINT32_MAX){r->rc=-4;goto cleanup;}regions[region_count++]=args;regions[region_count++]=table;
 r->stage=1;r->rc=p->allocate(p->ctx,arena_bytes,&arena);if(r->rc)goto cleanup;
 if(!arena.ptr||arena.bytes<arena_bytes||((uintptr_t)arena.ptr&(MS_ALIGN-1))||((uintptr_t)arena.ptr>>32)!=2||(((uintptr_t)arena.ptr+arena_bytes-1)>>32)!=2){r->rc=-5;goto cleanup;}
 r->rc=p->allocate(p->ctx,MS_COMMAND_BYTES,&command);if(r->rc)goto cleanup;
 if(!command.ptr||command.bytes<MS_COMMAND_BYTES||((uintptr_t)command.ptr&(MS_ALIGN-1))||(uintptr_t)command.ptr>(UINT64_C(1)<<48)-MS_COMMAND_BYTES){r->rc=-5;goto cleanup;}
 if(needs_scratch){
  r->rc=p->allocate(p->ctx,MS_SCRATCH_BYTES,&scratch);if(r->rc)goto cleanup;
  if(!scratch.ptr||scratch.bytes<MS_SCRATCH_BYTES||((uintptr_t)scratch.ptr&(MS_ALIGN-1))||(uintptr_t)scratch.ptr>(UINT64_C(1)<<48)-MS_SCRATCH_BYTES){r->rc=-5;goto cleanup;}
  sp=scratch.ptr;memset(sp,0xa5,MS_SCRATCH_BYTES);memset(sp+MS_SCRATCH_OFFSET,0,MS_SCRATCH_PAYLOAD);
  if(model_scratch_full_ring_build((uintptr_t)sp+MS_SCRATCH_OFFSET,MS_SCRATCH_PAYLOAD,&scratch_plan)||scratch_plan.allocation_bytes!=MS_SCRATCH_PAYLOAD){r->rc=-7;goto cleanup;}
  flush(sp,MS_SCRATCH_BYTES);
 }
 if(timed){
  if(mul_size(f->launch_count,MS_TIMING_STRIDE,&timing_bytes)||aligned(timing_bytes,MS_ALIGN,&timing_bytes)){r->rc=-4;goto cleanup;}
  readings=calloc(f->launch_count,sizeof(*readings));if(!readings){r->rc=-3;goto cleanup;}
  r->rc=p->allocate(p->ctx,timing_bytes,&timing);if(r->rc)goto cleanup;
  if(!timing.ptr||timing.bytes<timing_bytes||((uintptr_t)timing.ptr&(MS_ALIGN-1))||(uintptr_t)timing.ptr>(UINT64_C(1)<<48)-timing_bytes){r->rc=-5;goto cleanup;}
  tp=timing.ptr;memset(tp,0xa5,timing_bytes);for(uint32_t i=0;i<f->launch_count;i++){unsigned char*b=tp+(size_t)i*MS_TIMING_STRIDE;memset(b,0,8);memset(b+0x40,0,8);memset(b+0x80,0,8);memset(b+0x1000,0,0x200);memset(b+0x2000,0,0x200);}flush(tp,timing_bytes);
 }
 a=arena.ptr;cmd=command.ptr;memset(a,0xa5,arena_bytes);memset(cmd,0,MS_COMMAND_BYTES);r->stage=2;
 for(uint32_t i=0;i<f->resource_count;i++)if(load_file(p,&f->resources[i].initial,temp)||upload(f->resources+i,rr+i,a,0,temp,(size_t)f->resources[i].initial.bytes)){r->rc=-6;goto cleanup;}
 for(uint32_t i=0;i<f->shader_count;i++)if(load_file(p,&f->shaders[i].code,a+codes[i].offset)){r->rc=-6;goto cleanup;}
 flush(a,arena_bytes);flush(cmd,MS_COMMAND_BYTES);r->setup_ns=p->ns(p->ctx)-begin;
 fprintf(log,"MODEL_SEQUENCE begin fixture=%s resources=%u shaders=%u launches=%u frames=%u arena_bytes=%zu scratch=%p scratch_payload=%u scratch_tmpring=0x%x diagnostic=1 pure_gpu_timing=0\n",f->name?f->name:"",f->resource_count,f->shader_count,f->launch_count,f->frame_count,arena_bytes,needs_scratch?(void*)(sp+MS_SCRATCH_OFFSET):NULL,needs_scratch?MS_SCRATCH_PAYLOAD:0,scratch_plan.tmpring_value);fflush(log);
 for(uint32_t fi=0;fi<f->frame_count;fi++){
  r->frame=fi;const struct ms_frame*fr=f->frames+fi;uint64_t frame_gpu_ticks=0;
  for(uint32_t i=0;i<fr->upload_count;i++){const struct ms_upload*u=fr->uploads+i;uint32_t ri=role_index(f,u->role,r->parity);if(load_file(p,&u->file,temp)||upload(f->resources+ri,rr+ri,a,(size_t)u->logical_offset,temp,(size_t)u->file.bytes)){r->rc=-6;goto cleanup;}flush(a+rr[ri].region.offset,rr[ri].region.bytes);}
  for(uint32_t li=0;li<fr->launch_count;li++){
   uint32_t ix=fr->first_launch+li;r->launch=ix;const struct ms_launch*l=f->launches+ix;const struct ms_shader*s=f->shaders+l->shader;
   if(capture_inputs&&(capture_inputs==1||(r->launch>=input_first&&r->launch<=input_last))&&input_snapshot(log,p,f,rr,a,r,input_copy,input_bytes,input_count)){r->save_errors++;r->rc=-6;goto cleanup;}
   if(load_file(p,&l->args,a+args.offset)){r->rc=-6;goto cleanup;}
   uint32_t selected_shader=ms_k8_select(f,l,ix,a+args.offset);
   s=f->shaders+selected_shader;
   if(selected_shader!=l->shader)fprintf(log,"MODEL_SEQUENCE shader_variant launch=%u original=%u selected=%u guard=exact_original_cbv\n",ix,l->shader,selected_shader);
   memset(a+table.offset,0,table.bytes);
   for(uint32_t j=0;j<l->binding_count;j++){
    const struct ms_binding*b=l->bindings+j;uint32_t*d=(void*)(a+table.offset+b->table_offset);
    if(b->kind==MS_DESC_SAMPLER){memcpy(d,b->sampler_words,16);continue;}
    if(b->kind==MS_DESC_BUFFER){uint64_t base=b->role==MS_ARGS_ROLE?(uintptr_t)(a+args.offset):(uintptr_t)(a+rr[role_index(f,b->role,r->parity)].region.offset);base+=b->view_offset;d[0]=(uint32_t)base;d[1]=(uint32_t)(base>>32);d[2]=(uint32_t)b->view_bytes;d[3]=0x31016fac;}
    else{uint32_t ri=role_index(f,b->role,r->parity);if(model_sequence_descriptor(d,f->resources+ri,(uintptr_t)(a+rr[ri].region.offset),rr[ri].region.bytes,rr[ri].pitch)){r->rc=-7;goto cleanup;}}
   }
   memcpy(table_saved,a+table.offset,table.bytes);
   memset(cmd,0,MS_COMMAND_BYTES);
   volatile uint64_t*barrier=(void*)(cmd+0x1100),*fence=(void*)(cmd+0x1140);*barrier=*fence=0;
   if(timed&&timing_check(log,tp,timing_bytes,readings,f->launch_count,r->completed_launches,ix,0,r)){r->rc=-15;r->retained=1;goto done;}
   uint32_t token=0x4d530000u+r->completed_launches+1;size_t count=commands((void*)cmd,s,l,(uintptr_t)(a+codes[selected_shader].offset),(uintptr_t)(a+table.offset),(uintptr_t)barrier,(uintptr_t)fence,token,&scratch_plan,timed?(uintptr_t)(tp+(size_t)ix*MS_TIMING_STRIDE):0);
   if(!count){r->rc=-7;goto cleanup;}uint32_t saved_commands[MS_COMMAND_CAP];memcpy(saved_commands,cmd,count*4);
   flush(a+args.offset,args.bytes);flush(a+table.offset,table.bytes);flush(cmd,MS_COMMAND_BYTES);r->stage=3;submitted=p->ns(p->ctx);r->retained=1;
   r->submit_rc=p->submit(p->ctx,(void*)cmd,(uint32_t)count);if(r->submit_rc){r->rc=-8;goto done;}
   r->suspend_rc=p->suspend_point(p->ctx);if(r->suspend_rc){r->rc=-9;goto done;}
   for(;;){flush((const void*)fence,8);uint64_t observed=__atomic_load_n(fence,__ATOMIC_ACQUIRE);if(observed==token)break;if(p->ns(p->ctx)-submitted>(f->timeout_ns?f->timeout_ns:UINT64_C(10000000000))){r->rc=-10;goto done;}p->sleep_us(p->ctx,1000);}
   r->submit_to_fence_ns+=p->ns(p->ctx)-submitted;
   if(timed){
    unsigned char*b=tp+(size_t)ix*MS_TIMING_STRIDE;
    if(timing_check(log,tp,timing_bytes,readings,f->launch_count,r->completed_launches,ix,1,r)){r->rc=-15;goto done;}
    uint64_t start=timing_read(b),end=timing_read(b+0x40);if(!start||end<=start||timing_read(b+0x80)!=token||(ix&&start<readings[ix-1].end)||end-start>UINT64_MAX-total_gpu_ticks){fprintf(log,"MODEL_SEQUENCE gpu_timing_invalid frame=%u launch=%u start=%"PRIu64" end=%"PRIu64" start_barrier=%"PRIu64"\n",fi,ix,start,end,timing_read(b+0x80));r->rc=-15;goto done;}
    readings[ix].start=start;readings[ix].end=end;memcpy(readings[ix].zpass,b+0x1000,0x200);memcpy(readings[ix].zpass+0x200,b+0x2000,0x200);frame_gpu_ticks+=end-start;total_gpu_ticks+=end-start;
    fprintf(log,"MODEL_SEQUENCE gpu_interval frame=%u launch=%u start_ticks=%"PRIu64" end_ticks=%"PRIu64" delta_ticks=%"PRIu64" command_dwords=%zu includes_start_release_wait_and_end_zpass=1 excludes_cpu_file_checks_and_final_fence=1 assumed_ns_per_tick=none\n",fi,ix,start,end,end-start,count);fflush(log);
   }
   r->retained=0;r->stage=4;r->completed_launches++;
   if(frame_audit){flush(a+args.offset,args.bytes);flush(a+table.offset,table.bytes);}
   else flush(a,arena_bytes);
   flush(cmd,MS_COMMAND_BYTES);
   if(!frame_audit&&needs_scratch){
    flush(sp,MS_SCRATCH_OFFSET);flush(sp+MS_SCRATCH_OFFSET+MS_SCRATCH_PAYLOAD,MS_SCRATCH_BYTES-MS_SCRATCH_OFFSET-MS_SCRATCH_PAYLOAD);
    for(size_t j=0;j<MS_SCRATCH_OFFSET;j++)r->guard_errors+=sp[j]!=0xa5;
    for(size_t j=MS_SCRATCH_OFFSET+MS_SCRATCH_PAYLOAD;j<MS_SCRATCH_BYTES;j++)r->guard_errors+=sp[j]!=0xa5;
   }
   if(!frame_audit)r->guard_errors+=guards(a,arena_bytes,regions,region_count,f,rr);
   for(size_t j=count*4;j<MS_COMMAND_BYTES;j++)if((j<0x1100||j>=0x1108)&&(j<0x1140||j>=0x1148))r->guard_errors+=cmd[j]!=0;
   r->immutable_errors+=memcmp(cmd,saved_commands,count*4)!=0;r->immutable_errors+=memcmp(a+table.offset,table_saved,table.bytes)!=0;r->immutable_errors+=!digest(a+args.offset,(size_t)l->args.bytes,l->args.sha256);
   r->immutable_errors+=__atomic_load_n(barrier,__ATOMIC_ACQUIRE)!=token;
   if(!frame_audit){
    for(uint32_t j=0;j<f->shader_count;j++)r->immutable_errors+=!digest(a+codes[j].offset,codes[j].bytes,f->shaders[j].code.sha256);
    for(uint32_t j=0;j<f->resource_count;j++)if(f->resources[j].flags&MS_IMMUTABLE){const void*data=a+rr[j].region.offset;if(f->resources[j].kind==MS_IMAGE){if(model_sequence_unpack_rows(temp,(size_t)f->resources[j].initial.bytes,data,rr[j].region.bytes,f->resources[j].width,f->resources[j].height,bpp(f->resources[j].dxgi_format))){r->rc=-7;goto cleanup;}data=temp;}r->immutable_errors+=!digest(data,(size_t)f->resources[j].initial.bytes,f->resources[j].initial.sha256);}
   }
   if(r->guard_errors||r->immutable_errors){r->rc=-11;goto cleanup;}
   int checked=check(log,p,f,rr,a,l->checkpoints,l->checkpoint_count,r->parity,r,collect);if(checked){r->rc=checked==-2?-12:checked==-3?-14:-6;goto cleanup;}
  }
  if(frame_audit){
   flush(a,arena_bytes);
   if(needs_scratch){
    flush(sp,MS_SCRATCH_OFFSET);flush(sp+MS_SCRATCH_OFFSET+MS_SCRATCH_PAYLOAD,MS_SCRATCH_BYTES-MS_SCRATCH_OFFSET-MS_SCRATCH_PAYLOAD);
    for(size_t j=0;j<MS_SCRATCH_OFFSET;j++)r->guard_errors+=sp[j]!=0xa5;
    for(size_t j=MS_SCRATCH_OFFSET+MS_SCRATCH_PAYLOAD;j<MS_SCRATCH_BYTES;j++)r->guard_errors+=sp[j]!=0xa5;
   }
   r->guard_errors+=guards(a,arena_bytes,regions,region_count,f,rr);
   for(uint32_t j=0;j<f->shader_count;j++)r->immutable_errors+=!digest(a+codes[j].offset,codes[j].bytes,f->shaders[j].code.sha256);
   for(uint32_t j=0;j<f->resource_count;j++)if(f->resources[j].flags&MS_IMMUTABLE){const void*data=a+rr[j].region.offset;if(f->resources[j].kind==MS_IMAGE){if(model_sequence_unpack_rows(temp,(size_t)f->resources[j].initial.bytes,data,rr[j].region.bytes,f->resources[j].width,f->resources[j].height,bpp(f->resources[j].dxgi_format))){r->rc=-7;goto cleanup;}data=temp;}r->immutable_errors+=!digest(data,(size_t)f->resources[j].initial.bytes,f->resources[j].initial.sha256);}
   if(r->guard_errors||r->immutable_errors){r->rc=-11;goto cleanup;}
   fprintf(log,"MODEL_SEQUENCE frame_audit_done frame=%u completed_launches=%u guards=%u immutable=%u\n",fi,r->completed_launches,r->guard_errors,r->immutable_errors);fflush(log);
  }
  int checked=check(log,p,f,rr,a,fr->checkpoints,fr->checkpoint_count,r->parity,r,collect);if(checked){r->rc=checked==-2?-12:checked==-3?-14:-6;goto cleanup;}
  if(timed)fprintf(log,"MODEL_SEQUENCE gpu_frame frame=%u interval_sum_ticks=%"PRIu64" first_start_ticks=%"PRIu64" last_end_ticks=%"PRIu64" frame_span_includes_cpu_gaps=1 exact_period_claim=0\n",fi,frame_gpu_ticks,readings[fr->first_launch].start,readings[fr->first_launch+fr->launch_count-1].end);
  r->completed_frames++;r->parity^=1;
 }
 r->rc=collect&&r->numerical_failures?-12:0;
cleanup:
 if(tp&&r->completed_launches&&timing_check(log,tp,timing_bytes,readings,f->launch_count,r->completed_launches,r->completed_launches,0,r)){r->rc=-15;r->retained=1;goto done;}
 if(timing.ptr&&p->release(p->ctx,&timing)){r->rc=-13;r->retained=1;goto done;}
 if(command.ptr&&p->release(p->ctx,&command)){r->rc=-13;r->retained=1;goto done;}
 if(scratch.ptr&&p->release(p->ctx,&scratch)){r->rc=-13;r->retained=1;goto done;}
 if(arena.ptr&&p->release(p->ctx,&arena)){r->rc=-13;r->retained=1;goto done;}
 r->cleanup=1;
done:
 free(rr);free(codes);free(regions);free(temp);free(table_saved);free(readings);free(input_copy);r->total_ns=p->ns(p->ctx)-begin;
 if(timed)fprintf(log,"MODEL_SEQUENCE gpu_total interval_sum_ticks=%"PRIu64" exact_period_claim=0 includes_ordering_overhead=1\n",total_gpu_ticks);
 fprintf(log,"MODEL_SEQUENCE result rc=%d stage=%d frame=%u launch=%u completed_launches=%u completed_frames=%u parity=%u retained=%u cleanup=%u guards=%u immutable=%u save_errors=%u mismatches=%"PRIu64" first=%"PRIu64" numerical_failures=%u max_relative_l2=%.12g final_psnr_checks=%u min_final_psnr=%.12g submit=%d suspend=%d\n",r->rc,r->stage,r->frame,r->launch,r->completed_launches,r->completed_frames,r->parity,r->retained,r->cleanup,r->guard_errors,r->immutable_errors,r->save_errors,r->byte_mismatches,r->first_mismatch,r->numerical_failures,r->max_relative_l2,r->final_psnr_checks,r->min_final_psnr,r->submit_rc,r->suspend_rc);
 fprintf(log,"MODEL_SEQUENCE timing_cpu_ns setup=%"PRIu64" submit_to_fence_including_suspend_poll=%"PRIu64" total=%"PRIu64" pure_gpu_timing=0\n",r->setup_ns,r->submit_to_fence_ns,r->total_ns);fflush(log);return r->rc;
}
int model_sequence_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r){return run_mode(log,p,f,r,0,0,0,0,0,0);}
int model_sequence_collect_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r){return run_mode(log,p,f,r,1,0,0,0,0,0);}
int model_sequence_timed_collect_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r){return run_mode(log,p,f,r,1,1,0,0,0,0);}
int model_sequence_frame_timed_collect_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r){return run_mode(log,p,f,r,1,1,0,0,0,1);}
int model_sequence_input_collect_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r){return run_mode(log,p,f,r,1,0,1,0,0,0);}
int model_sequence_input_range_run_ops(FILE*log,const struct encoder_ops*p,const struct ms_fixture*f,struct ms_result*r,uint32_t first,uint32_t last){return run_mode(log,p,f,r,0,0,2,first,last,0);}
