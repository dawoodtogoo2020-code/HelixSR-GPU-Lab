#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "reset_all.h"
#include "render_options.h"
#include "profile_file_adapter.h"
#include "profile_namespace.h"
#include "profile_store.h"
#include "tiny_store.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifndef HELIX_RESET_HOST_TEST
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

enum { MAX_IDENTITIES=256, MAX_DIRECTORY_ENTRIES=4096 };
typedef int (*name_visitor)(void *,const char *);
struct collected {ap_identity identities[MAX_IDENTITIES];unsigned count,failed;const char *root;int render;};
static int suffix(const char *root,const char *tail)
{
    size_t n,m;if(!root||root[0]!='/')return 0;n=strlen(root);m=strlen(tail);
    return n<=4096&&n>=m&&!strcmp(root+n-m,tail);
}
static int record_name(const char *name,char ns[65])
{
    unsigned j;if(strlen(name)!=77||memcmp(name,"settings.",9)||strcmp(name+73,".bin"))return 0;
    for(j=0;j<64;++j){char c=name[9+j];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;ns[j]=c;}
    ns[64]=0;return 1;
}
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int valid_record(const unsigned char *raw,size_t n,int render,ap_identity *id)
{
    ro_options o;
    if(n!=(render?301u:277u)||memcmp(raw,render?"HRO1":"HSP1",4)||raw[4]!=1||
       !ps_sealed(raw,n)||!ps_get64(raw+8)||!ps_decode_identity(id,raw+16))return 0;
    if(!render)return raw[5]<=1&&raw[6]<=4&&raw[7]==0;
    if(raw[5]||raw[6]||raw[7])return 0;
    o.input_width=get32(raw+273);o.input_height=get32(raw+277);
    o.output_width=get32(raw+281);o.output_height=get32(raw+285);
    o.preset=get32(raw+289);o.backend=get32(raw+293);o.revision=ps_get64(raw+8);
    return ro_valid(&o);
}
static int collect_name(void *context,const char *name)
{
    struct collected *c=context;char ns[65],expected[65];unsigned j;
    unsigned char raw[TS_MAX_RECORD];size_t n=0;ts_store *store;ap_identity id;int read_result,closed;
    if(!record_name(name,ns))return 0;
    store=ts_open(c->root,ns);if(!store){++c->failed;return 0;}
    read_result=ts_read(store,"settings",raw,sizeof raw,&n);closed=ts_close(store);
    if(read_result!=1||closed||!valid_record(raw,n,c->render,&id)||
       !pfa_settings_namespace(&id,expected)||strcmp(expected,ns)){++c->failed;return 0;}
    for(j=0;j<c->count;++j)if(ps_identity_equal(c->identities+j,&id))return 0;
    if(c->count==MAX_IDENTITIES){++c->failed;return -1;}
    c->identities[c->count++]=id;return 0;
}
#ifdef HELIX_RESET_HOST_TEST
/* Host tests inject names only. All production record validation and store
 * operations remain the same, using a deterministic tiny_store stub. */
extern int helix_reset_test_enumerate(const char *,name_visitor,void *);
static int enumerate(const char *root,name_visitor visit,void *context)
{return helix_reset_test_enumerate(root,visit,context);}
#else
static int enumerate(const char *root,name_visitor visit,void *context)
{
    DIR *directory;struct dirent *entry;int fd,result=0;unsigned seen=0;
    fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;
    directory=fdopendir(fd);if(!directory){(void)close(fd);return -1;}
    for(;;){errno=0;entry=readdir(directory);if(!entry){if(errno)result=-1;break;}
        if(++seen>MAX_DIRECTORY_ENTRIES||visit(context,entry->d_name)){result=-1;break;}}
    if(closedir(directory))result=-1;return result;
}
#endif
static int root_check(const char *root)
{
    static const char ns[]="0000000000000000000000000000000000000000000000000000000000000000";
    ts_store *store=ts_open(root,ns);if(!store)return -1;return ts_close(store)?-1:0;
}
static int reset_identity(const char *settings_root,const char *render_root,const ap_identity *id)
{
    pfa_store store=PFA_INITIALIZER;ps_profile current,saved;ps_result rc,closed;
    const ps_io *io;ro_options old,normal;int render_result;
    rc=pfa_open(&store,settings_root,id);if(rc!=PS_OK)return -1;
    io=pfa_get_io(&store);rc=ps_load(io,id,&current);
    if(rc==PS_OK||rc==PS_DEFAULT){current.desired=AP_OFF;rc=ps_save(io,&current,current.revision,&saved);}
    closed=pfa_close(&store);if(rc!=PS_OK||closed!=PS_OK)return -1;
    render_result=ro_load(render_root,id,&old);if(render_result<0)return -1;
    ro_defaults(&normal);if(ro_apply_preset(&normal,RO_NATIVE))return -1;
    return ro_save(render_root,id,&normal,old.revision)?-1:0;
}
int helix_reset_all(const char *settings_root,const char *render_root,unsigned *reset,unsigned *failed)
{
    struct collected *c;unsigned j;
    if(reset)*reset=0;if(failed)*failed=0;
    if(!reset||!failed)return -1;
    if(!suffix(settings_root,"/settings")||!suffix(render_root,"/render-settings")||
       root_check(settings_root)||root_check(render_root)){*failed=1;return -1;}
    c=calloc(1,sizeof *c);if(!c){*failed=1;return -1;}
    c->root=settings_root;c->render=0;if(enumerate(c->root,collect_name,c))++c->failed;
    c->root=render_root;c->render=1;if(enumerate(c->root,collect_name,c))++c->failed;
    for(j=0;j<c->count;++j){if(reset_identity(settings_root,render_root,c->identities+j))++c->failed;else++*reset;}
    *failed=c->failed;free(c);return *failed?-1:0;
}
