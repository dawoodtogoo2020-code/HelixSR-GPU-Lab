/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "launch_policy.h"
#include "../../common/package_support.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    /* Even a valid old shared row cannot substitute for a missing/corrupt
     * package-owned identity, regardless of READY or the saved ON/OFF choice. */
    assert(!gp_accept_catalog("ps5-regular","ps5-regular","PPSA02572","01.200.007",0));
    assert(gp_accept_catalog("ps5-regular","ps5-regular","PPSA02572","01.200.007",1));
    assert(!gp_accept_catalog("ps5-regular","ps5-pro","PPSA02572","01.200.007",1));
    assert(!gp_accept_catalog("ps5-regular","ps5-regular","PPSA02572","01.200.008",1));
    assert(gp_accept_catalog("ps5-regular","ps5-regular","PPSA12345","01.00",0));
    gl_game inventory[]={{"PPSA02572","01.200.008","Updated Alan Wake 2"},{"CUSA08809","01.00","SOTC"}};
    assert(!gp_inventory_match(inventory,2,"PPSA02572","01.200.007"));
    assert(!gp_inventory_match(inventory,2,"PPSA34547","01.300.300"));
    assert(!gp_inventory_match(inventory,0,"CUSA08809","01.00"));
    assert(gp_inventory_match(inventory,2,"CUSA08809","01.00"));
    const char *titles[]={"PPSA02572","PPSA34547"},*versions[]={"01.200.007","01.300.300"};
    for(unsigned i=0;i<2;i++)for(int on=0;on<2;on++) {
        assert(gp_choose_launch("ps5-regular",titles[i],versions[i],1,1,0,on)==(on?'H':'O'));
        assert(gp_choose_launch("ps5-regular",titles[i],versions[i],1,0,0,on)==-1);
        assert(gp_choose_launch("ps5-regular",titles[i],versions[i],0,1,0,on)==-1);
        assert(gp_choose_launch("ps5-pro",titles[i],versions[i],1,1,0,on)==-1);
        assert(!gp_request_allowed("ps5-regular",titles[i],versions[i],0));
        assert(gp_request_allowed("ps5-regular",titles[i],versions[i],on?'H':'O'));
    }
    assert(gp_choose_launch("ps5-pro","CUSA08809","01.00",1,0,1,1)==0);
    assert(gp_choose_launch("ps5-pro","CUSA08809","01.00",1,0,0,1)==-1);
    assert(!gp_request_allowed("ps5-pro","CUSA08809","01.01",0));
    assert(!gp_request_allowed("ps5-regular","CUSA08809","01.00",0));
    assert(gp_request_allowed("ps5-pro","CUSA08809","01.00",0));
    assert(gp_choose_launch("ps5-regular","PPSA12345","01.00",0,0,0,0)==0);
    assert(gp_request_allowed(NULL,"PPSA12345","01.00",0));
    assert(!gp_request_allowed(NULL,"PPSA02572","01.200.007",'H'));
    /* SAROS requires the exact tested build and package-owned catalog record. */
    assert(!gp_accept_catalog("ps5-regular","ps5-regular","PPSA07631","01.000.000",0));
    assert(gp_accept_catalog("ps5-regular","ps5-regular","PPSA07631","01.000.000",1));
    assert(!gp_accept_catalog("ps5-regular","ps5-regular","PPSA07631","01.000.001",1));
    assert(!gp_accept_catalog("ps5-pro","ps5-pro","PPSA07631","01.000.000",1));
    for(int on=0;on<2;on++){
     assert(gp_choose_launch("ps5-regular","PPSA07631","01.000.000",1,1,0,on)==(on?'H':'O'));
     assert(gp_choose_launch("ps5-regular","PPSA07631","01.000.000",1,0,0,on)==-1);
     assert(gp_choose_launch("ps5-regular","PPSA07631","01.000.000",0,1,0,on)==-1);
     assert(gp_request_allowed("ps5-regular","PPSA07631","01.000.000",on?'H':'O'));
    }
    assert(!gp_request_allowed("ps5-regular","PPSA07631","01.000.000",0));
    puts("inventory/version reconciliation and fail-closed managed launch policy: PASS");return 0;
}
