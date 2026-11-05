/* Host test: scans a file in odd-sized chunks and checks the result against a
 * whole-file pass, so boundary handling and the streaming hash are exercised.
 *   scan_test FILE [EXPECTED_SHA256] */
#include "scan_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int scan(const char *path,size_t chunk,se_state *s,char hex[65])
{
    FILE *f=fopen(path,"rb");if(!f)return 1;
    unsigned char *buffer=malloc(chunk);se_init(s);size_t n;
    while((n=fread(buffer,1,chunk,f))>0)se_update(s,buffer,n);
    fclose(f);free(buffer);se_finish(s,hex);return 0;
}
int main(int argc,char **argv)
{
    if(argc<2)return 2;
    static se_state a,b;char ha[65],hb[65];
    if(scan(argv[1],(size_t)1<<26,&a,ha)||scan(argv[1],4093,&b,hb))return 1;
    int same=!strcmp(ha,hb)&&!memcmp(a.hits,b.hits,sizeof a.hits);
    printf("sha256=%s bytes=%llu primary=%s engine=%s chunked_equal=%d\n",ha,(unsigned long long)a.bytes,se_primary(&a),se_engine(&a),same);
    for(unsigned t=0;t<SE_TECH_COUNT;++t)if(a.hits[t])printf("  %s: %u\n",se_tech_name(t),a.hits[t]);
    if(argc>2&&strcmp(argv[2],ha)){puts("sha256 mismatch");return 1;}
    return !same;
}
