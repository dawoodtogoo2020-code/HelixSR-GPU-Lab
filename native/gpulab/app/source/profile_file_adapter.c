#include "profile_file_adapter.h"
#include "profile_namespace.h"
#include <string.h>

enum { PFA_UNUSED=0, PFA_OPEN, PFA_CLOSED, PFA_FAILED };

static pfa_store *active(void *context)
{
    pfa_store *s=context;
    return s && s->lifecycle==PFA_OPEN && s->store && !s->faulted ? s : NULL;
}

static const char *key_name(const char *key)
{
    if (!key) return NULL;
    if (!strcmp(key,"profile.v1")) return "settings";
    if (!strcmp(key,"recovery.v1")) return "recovery";
    return NULL;
}

static int file_read(void *context,const char *key,unsigned char *data,
                     size_t capacity,size_t *used)
{
    pfa_store *s=active(context);
    const char *name=key_name(key);
    int result;
    if (used) *used=0;
    if (!s || !name || !used) return -1;
    result=ts_read(s->store,name,data,capacity,used);
    if (result<0) { s->faulted=1; *used=0; }
    return result;
}

static size_t file_stage(void *context,const char *key,
                         const unsigned char *data,size_t size)
{
    pfa_store *s=active(context);
    const char *name=key_name(key);
    size_t result;
    if (!s || !name) return 0;
    if (!data || !size || size>PS_MAX_RECORD) { s->faulted=1; return 0; }
    result=ts_stage(s->store,name,data,size);
    if (result!=size) s->faulted=1;
    return result;
}

static int sync_callback(void *context,const char *key,
                         int (*call)(void *,const char *))
{
    pfa_store *s=active(context);
    const char *name=key_name(key);
    int result;
    if (!s || !name) return -1;
    result=call(s->store,name);
    if (result) s->faulted=1;
    return result;
}

static int file_sync(void *context,const char *key)
{ return sync_callback(context,key,ts_sync_stage); }
static int file_commit(void *context,const char *key)
{ return sync_callback(context,key,ts_commit); }
static int directory_sync(void *context,const char *key)
{ return sync_callback(context,key,ts_sync_directory); }

ps_result pfa_open(pfa_store *s,const char *root,const ap_identity *identity)
{
    char settings_namespace[65];
    if (!s || !root || !ap_identity_valid(identity) ||
        s->lifecycle!=PFA_UNUSED || s->store) return PS_INVALID;
    s->lifecycle=PFA_FAILED;
    if (!pfa_settings_namespace(identity,settings_namespace)) {
        s->faulted=1; return PS_INVALID;
    }
    s->store=ts_open(root,settings_namespace);
    if (!s->store) { s->faulted=1; return PS_IO_ERROR; }
    s->io=(ps_io){s,file_read,file_stage,file_sync,file_commit,directory_sync};
    s->lifecycle=PFA_OPEN;
    s->close_result=PS_OK;
    return PS_OK;
}

const ps_io *pfa_get_io(pfa_store *s)
{ return active(s) ? &s->io : NULL; }

ps_result pfa_close(pfa_store *s)
{
    int result;
    if (!s) return PS_INVALID;
    if (s->lifecycle==PFA_CLOSED) return s->close_result;
    if (s->lifecycle!=PFA_OPEN || !s->store) return PS_INVALID;
    /* Disable all borrowed callbacks before releasing their underlying store. */
    s->lifecycle=PFA_CLOSED;
    result=ts_close(s->store);
    s->store=NULL;
    s->close_result=(result || s->faulted) ? PS_IO_ERROR : PS_OK;
    return s->close_result;
}
