#include "render_options.h"
#include "profile_namespace.h"
#include "profile_store.h"
#include "tiny_store.h"
#include <stddef.h>
#include <string.h>

enum { RO_RECORD_BYTES=301, RO_FIELDS_OFFSET=273 };
_Static_assert(RO_RECORD_BYTES<=TS_MAX_RECORD,"bounded render options record");
static const char key[]="settings";
static const uint32_t numerators[5]={1,1,58,2,1};
static const uint32_t denominators[5]={3,2,100,3,1};

static int dimensions(uint32_t w,uint32_t h)
{
    return w>=160 && w<=7680 && h>=90 && h<=4320 && !(w&1) && !(h&1);
}
/* A custom render size may be odd: games render at sizes such as CONTROL's 1505x847 (u07). Presets stay even. */
static int render_dimensions(uint32_t w,uint32_t h,uint32_t preset)
{
    return preset==RO_CUSTOM?w>=160 && w<=7680 && h>=90 && h<=4320:dimensions(w,h);
}
static uint32_t scaled(uint32_t value,unsigned preset)
{
    uint64_t n=(uint64_t)value*numerators[preset];
    uint64_t d=(uint64_t)denominators[preset]*2u,q=n/d,r=n%d;
    if(r*2u>d || (r*2u==d && (q&1u)))++q;
    return (uint32_t)(q*2u);
}
int ro_valid(const ro_options *o)
{
    if(!o || o->preset>RO_CUSTOM || o->backend>RO_GAME_DEFAULT ||
       !render_dimensions(o->input_width,o->input_height,o->preset) ||
       !dimensions(o->output_width,o->output_height) ||
       o->input_width>o->output_width || o->input_height>o->output_height)return 0;
    return o->preset==RO_CUSTOM ||
        (o->input_width==scaled(o->output_width,o->preset) &&
         o->input_height==scaled(o->output_height,o->preset));
}
int ro_apply_preset(ro_options *o,unsigned preset)
{
    ro_options candidate;
    if(!o || preset>RO_CUSTOM)return -1;
    candidate=*o;candidate.preset=preset;
    if(preset!=RO_CUSTOM) {
        candidate.input_width=scaled(candidate.output_width,preset);
        candidate.input_height=scaled(candidate.output_height,preset);
    }
    if(!ro_valid(&candidate))return -1;
    *o=candidate;return 0;
}
void ro_defaults(ro_options *o)
{
    if(!o)return;
    memset(o,0,sizeof *o);o->output_width=2560;o->output_height=1440;
    o->backend=RO_GAME_DEFAULT;(void)ro_apply_preset(o,RO_QUALITY);
}
const char *ro_preset_name(unsigned preset)
{
    static const char *names[]={"Ultra Performance","Performance","Balanced",
                                "Quality","Native","Custom"};
    return preset<=RO_CUSTOM?names[preset]:"Unknown";
}
const char *ro_backend_name(unsigned backend)
{
    static const char *names[]={"HelixSR","HelixSR + DLSS 3.7",
        "FSR 4.1.1 Exact","FSR 4.1.1 Lossy","PSSR","Game default"};
    return backend<=RO_GAME_DEFAULT?names[backend]:"Unknown";
}
static int root_valid(const char *root)
{
    static const char suffix[]="/render-settings";
    size_t n;
    if(!root || root[0]!='/')return 0;
    n=strlen(root);
    return n>=sizeof suffix-1 && n<=4096 &&
        !strcmp(root+n-(sizeof suffix-1),suffix);
}
static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(unsigned char *p,uint32_t n)
{
    unsigned j;for(j=0;j<4;++j)p[j]=(unsigned char)(n>>(j*8));
}
static int decode(ts_store *store,const ap_identity *id,ro_options *out)
{
    unsigned char raw[TS_MAX_RECORD];size_t n=0;ap_identity stored;
    ro_options candidate;int result=ts_read(store,key,raw,sizeof raw,&n);
    ro_defaults(out);
    if(result==0)return RO_DEFAULT;
    if(result!=1)return RO_IO_ERROR;
    if(n!=RO_RECORD_BYTES || memcmp(raw,"HRO1",4) || raw[4]!=1 ||
       raw[5] || raw[6] || raw[7] || !ps_sealed(raw,n) ||
       !ps_decode_identity(&stored,raw+16) || !ps_identity_equal(&stored,id))
        return RO_CORRUPT;
    candidate.input_width=get32(raw+RO_FIELDS_OFFSET);
    candidate.input_height=get32(raw+RO_FIELDS_OFFSET+4);
    candidate.output_width=get32(raw+RO_FIELDS_OFFSET+8);
    candidate.output_height=get32(raw+RO_FIELDS_OFFSET+12);
    candidate.preset=get32(raw+RO_FIELDS_OFFSET+16);
    candidate.backend=get32(raw+RO_FIELDS_OFFSET+20);
    candidate.revision=ps_get64(raw+8);
    if(!candidate.revision || !ro_valid(&candidate))return RO_CORRUPT;
    *out=candidate;return RO_OK;
}
static void encode(unsigned char raw[RO_RECORD_BYTES],const ap_identity *id,
                   const ro_options *o)
{
    memset(raw,0,RO_RECORD_BYTES);memcpy(raw,"HRO1",4);raw[4]=1;
    ps_put64(raw+8,o->revision);ps_encode_identity(raw+16,id);
    put32(raw+RO_FIELDS_OFFSET,o->input_width);
    put32(raw+RO_FIELDS_OFFSET+4,o->input_height);
    put32(raw+RO_FIELDS_OFFSET+8,o->output_width);
    put32(raw+RO_FIELDS_OFFSET+12,o->output_height);
    put32(raw+RO_FIELDS_OFFSET+16,o->preset);
    put32(raw+RO_FIELDS_OFFSET+20,o->backend);ps_seal(raw,RO_RECORD_BYTES);
}
int ro_load(const char *root,const ap_identity *id,ro_options *out)
{
    ts_store *store;char name[65];ro_options candidate;int result,closed;
    if(!out)return RO_INVALID;
    ro_defaults(out);
    if(!root_valid(root) || !pfa_settings_namespace(id,name))return RO_INVALID;
    store=ts_open(root,name);if(!store)return RO_IO_ERROR;
    result=decode(store,id,&candidate);closed=ts_close(store);
    if(closed)return RO_IO_ERROR;
    if(result>=0)*out=candidate;
    return result;
}
int ro_save(const char *root,const ap_identity *id,ro_options *options,uint64_t expected)
{
    ts_store *store;char name[65];unsigned char raw[RO_RECORD_BYTES];
    ro_options current,candidate;int result,closed;
    if(!ro_valid(options) || expected==UINT64_MAX || !root_valid(root) ||
       !pfa_settings_namespace(id,name))return RO_INVALID;
    candidate=*options;store=ts_open(root,name);if(!store)return RO_IO_ERROR;
    result=decode(store,id,&current);
    if(result>=0) {
        if(current.revision!=expected)result=RO_CONFLICT;
        else {
            candidate.revision=expected+1;encode(raw,id,&candidate);
            result=ts_stage(store,key,raw,sizeof raw)==sizeof raw &&
                !ts_sync_stage(store,key) && !ts_commit(store,key) &&
                !ts_sync_directory(store,key)?RO_OK:RO_IO_ERROR;
        }
    }
    closed=ts_close(store);if(closed)result=RO_IO_ERROR;
    if(result==RO_OK)*options=candidate;
    return result;
}
