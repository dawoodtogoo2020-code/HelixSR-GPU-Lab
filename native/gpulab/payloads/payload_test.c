/* Host test of the launch payload's file logic: param.json parsing, names and
 * catalog rows. Build with -DHELIX_ROOT=<temp dir> (see build.sh); the PS5
 * calls are stubbed and main is renamed. */
#define main launch_main
#define HELIX_HOST_TEST 1
#include "helixsr_launch.c"
#undef main
#include <assert.h>

int sceLncUtilGetAppIdOfRunningBigApp(void) { return 0; }
int sceLncUtilGetAppTitleId(uint32_t app,char *title) { (void)app;title[0]=0;return 0; }
int sceUserServiceInitialize(void *p) { (void)p;return 0; }
int sceUserServiceGetForegroundUser(uint32_t *user) { *user=1;return 0; }
int sceSystemServiceLaunchApp(const char *t,char **a,struct launch_context *c) { (void)t;(void)a;(void)c;return 0; }
int sceKernelSendNotificationRequest(int a,notify_request_t *r,size_t n,int b) { (void)a;(void)r;(void)n;(void)b;return 0; }

int main(int argc,char **argv)
{
    /* param.json: the SAROS manifest captured on 16 Oct. */
    static char json[65536];char version[32],name[80],raw[256];
    assert(argc==2&&read_bounded(argv[1],json,sizeof json)>0);
    assert(json_string(json,"contentVersion",version,sizeof version));
    const char *section=strstr(strstr(json,"\"localizedParameters\""),"\"en-US\"");
    assert(section&&json_string(section,"titleName",raw,sizeof raw));
    plain_name(raw,name);
    printf("param: version=%s name=%s\n",version,name);
    plain_name("  SHADOW OF THE COLOSSUS\xe2\x84\xa2  \tEU ",name);
    assert(!strcmp(name,"SHADOW OF THE COLOSSUS EU"));
    assert(valid_title("PPSA28000")&&valid_title("CUSA08809")&&!valid_title("PPSA2800")&&!valid_title("NPXS40000"));
    /* Catalog: first row sets the device label; same title+version is not added twice. */
    const char *h1="8feec3e2e0c7d266f4e2c7007ef24cbe8009a734f17dca7d44546b3501661786";
    const char *h2="636c47d29e81a82ab2f39b826948355e83d8c1c22e27a5a5f8fd50ecdd546b43";
    char seed[256];snprintf(seed,sizeof seed,"ps5-regular\tPPSA34547\t01.300.300\t%s\tCONTROL\n",h1);
    assert(!write_atomic(HELIX_ROOT "/catalog.tsv",seed,strlen(seed)));
    assert(!strcmp(catalog_add("PPSA07631","01.000.000",h2,"SAROS"),"added"));
    assert(!strcmp(catalog_add("PPSA07631","01.000.000",h2,"SAROS"),"present"));
    assert(!strcmp(catalog_add("PPSA34547","01.300.300",h2,"CONTROL"),"present_other_hash"));
    static char catalog[4096];read_bounded(HELIX_ROOT "/catalog.tsv",catalog,sizeof catalog);
    printf("catalog:\n%s",catalog);
    char expect[256];snprintf(expect,sizeof expect,"ps5-regular\tPPSA07631\t01.000.000\t%s\tSAROS\n",h2);
    assert(strstr(catalog,expect)&&strstr(catalog,seed)==catalog);
    puts("payload logic: PASS");
    return 0;
}
