/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main inventory_main
#include "helixsr_inventory.c"
#undef main
#include <assert.h>
#include <stdint.h>
static void u32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
int main(void) {
    mkdir(USER_APPMETA,0755);mkdir(SYSTEM_APPMETA,0755);
    mkdir(USER_APPMETA "/PPSA02572",0755);mkdir(USER_APPMETA "/CUSA08809",0755);
    mkdir(SYSTEM_APPMETA "/CUSA08809",0755);
    const char json[]="{\"contentVersion\":\"01.200.007\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Alan Wake 2\"}}}";
    assert(!write_atomic(USER_APPMETA "/PPSA02572/param.json",json,sizeof json-1));
    unsigned char sfo[128]={0};memcpy(sfo,"\0PSF",4);u32(sfo+8,52);u32(sfo+12,68);u32(sfo+16,2);
    sfo[22]=4;sfo[23]=2;u32(sfo+24,6);u32(sfo+28,6);
    sfo[36]=8;sfo[38]=4;sfo[39]=2;u32(sfo+40,22);u32(sfo+44,22);u32(sfo+48,6);
    memcpy(sfo+52,"APP_VER\0TITLE\0",14);memcpy(sfo+68,"01.00\0SHADOW OF THE COLOSSUS\0",28);
    assert(!write_atomic(SYSTEM_APPMETA "/CUSA08809/param.sfo",(const char *)sfo,96));
    char version[32],name[80];assert(!title_info("CUSA08809",version,name));
    assert(!strcmp(version,"01.00")&&!strcmp(name,"SHADOW OF THE COLOSSUS"));
    assert(!title_info("PPSA02572",version,name)&&!strcmp(version,"01.200.007"));
    assert(title_info("../escape",version,name));
    assert(sfo_string(sfo,19,"APP_VER",version,sizeof version));
    unsigned char bad[128];memcpy(bad,sfo,sizeof bad);u32(bad+12,0xffffffff);
    assert(sfo_string(bad,sizeof bad,"APP_VER",version,sizeof version));
    assert(!inventory_main());char text[512];assert(read_bounded(HELIX_ROOT "/installed.tsv",text,sizeof text)>0);
    assert(strstr(text,"PPSA02572\t01.200.007\tAlan Wake 2")&&strstr(text,"CUSA08809\t01.00\tSHADOW OF THE COLOSSUS"));
    char *s=strstr(text,"CUSA08809");assert(s&&!strstr(s+1,"CUSA08809"));
    puts("PS5 JSON / PS4 SFO metadata and deduplicated dual-root inventory: PASS");return 0;
}
