/* SPDX-License-Identifier: GPL-3.0-or-later
 * Command lifecycle adapted from the existing ps5-vulkan compute_run.c and
 * compute_commands.c. Shader is the pinned public synthetic PSBC LDS probe. */
#include "psbc_probe.h"
#include "probe_shader.h"
#include <string.h>
enum { ARENA=65536, COMMAND=131072, WORDS=4608, LDS_WORDS=4096,
       DATA_OFFSET=0x4000, CAPACITY=128 };
#define TOKEN UINT64_C(0x4c445331)
static void flush(const void *address,size_t bytes)
{
    uintptr_t begin=(uintptr_t)address&~(uintptr_t)63;
    uintptr_t end=(uintptr_t)address+bytes;
    for(;begin<end;begin+=64) __asm__ volatile("clflush (%0)"::"r"(begin):"memory");
    __asm__ volatile("mfence":::"memory");
}
static void sh(uint32_t *p,size_t *n,uint32_t offset,const uint32_t *v,size_t count)
{
    p[(*n)++]=UINT32_C(0xc0007600)|((uint32_t)count<<16);
    p[(*n)++]=offset;
    memcpy(p+*n,v,count*4); *n+=count;
}
size_t psbc_probe_commands(uint32_t *out,size_t capacity,uint64_t code,
                           uint64_t table,uint64_t fence)
{
    if(!out||capacity<CAPACITY||!code||!table||!fence||(code&255)||
       (table&15)||(fence&7)||(code>>48)||(fence>>48)||
       (table>>32)!=PROBE_ADDRESS_HI||!PROBE_SET_VALID||
       PROBE_USER_COUNT>16||PROBE_SET_SLOT>=PROBE_USER_COUNT||PROBE_SCRATCH||
       (PROBE_UNRESOLVED&~1u)||sizeof(probe_code)>0x1000||
       probe_regs[4]!=256||probe_regs[5]!=1||probe_regs[6]!=1||
       ((probe_regs[3]>>15)&511)!=32) return 0;
    uint32_t p[CAPACITY],n0[4]={0},user[16]={0}; size_t n=0;
    uint32_t pgm[2]={(uint32_t)(code>>8),(uint32_t)(code>>40)};
    uint32_t dest[2]={UINT32_MAX,UINT32_MAX};
    sh(p,&n,0x204,n0,3);                    /* COMPUTE_START_X/Y/Z */
    sh(p,&n,0x207,probe_regs+4,3);          /* compiler local size */
    sh(p,&n,0x20c,pgm,2);
    sh(p,&n,0x212,probe_regs+2,2);          /* compiler RSRC1/2 incl LDS */
    sh(p,&n,0x215,n0,1);                   /* TMPRING_SIZE: no scratch */
    sh(p,&n,0x228,n0,1);                   /* PGM_RSRC3 */
    user[PROBE_SET_SLOT]=(uint32_t)table;   /* compiler slot 2, NOT old slot 1 */
    sh(p,&n,0x240,user,PROBE_USER_COUNT);
    sh(p,&n,0x216,dest,2); sh(p,&n,0x219,n0,2); sh(p,&n,0x224,n0,4);
    p[n++]=0xc0031500; p[n++]=1; p[n++]=1; p[n++]=1;
    p[n++]=0x8041; /* PSBC source fixes PS5 CS wave32, native order mode. */
    p[n++]=0xc0004600; p[n++]=0x407; /* CS_PARTIAL_FLUSH */
    /* Exact completion packet from the working RADV PS5 winsys:
     * event20/index5, GCR0x30c, DATA_SEL1 (32-bit sequence value). */
    const uint32_t release[]={0xc0064900,0x0030c514,0x20000000,
        (uint32_t)fence,(uint32_t)(fence>>32),(uint32_t)TOKEN,(uint32_t)(TOKEN>>32),0};
    memcpy(p+n,release,sizeof(release)); n+=8;
    if(n>CAPACITY)return 0;
    memcpy(out,p,n*4); return n;
}
static uint32_t value(unsigned i) { return UINT32_C(0x18273645)^(i*UINT32_C(0x9e3779b9)); }
static unsigned selected(unsigned lane) { return (lane*67u+19u)&4095u; }
int psbc_probe_run_platform(const struct psbc_probe_platform *p,struct psbc_probe_result *r)
{
    if(!r)return -1;
    memset(r,0,sizeof(*r)); r->rc=-1;
    if(!p||!p->allocate||!p->release||!p->submit||!p->suspend_point||
       !p->monotonic_ns||!p->sleep_us)return -1;
    struct psbc_probe_allocation arena={0},command={0};
    uint64_t start=p->monotonic_ns(p->context);
    r->stage=1;
    r->rc=p->allocate(p->context,ARENA,PROBE_ADDRESS_HI,0,&arena);
    if(r->rc)goto cleanup;
    if(!arena.ptr||arena.bytes<ARENA||((uintptr_t)arena.ptr&65535)||
       ((uintptr_t)arena.ptr>>32)!=PROBE_ADDRESS_HI||
       (((uintptr_t)arena.ptr+ARENA-1)>>32)!=PROBE_ADDRESS_HI){r->rc=-2;goto cleanup;}
    r->stage=2;
    r->rc=p->allocate(p->context,COMMAND,UINT32_MAX,1,&command);
    if(r->rc)goto cleanup;
    if(!command.ptr||command.bytes<COMMAND||((uintptr_t)command.ptr&65535)||
       (uintptr_t)command.ptr>((UINT64_C(1)<<48)-COMMAND)){r->rc=-3;goto cleanup;}
    unsigned char *a=arena.ptr;
    unsigned char *c=command.ptr;
    memset(a,0xa5,ARENA); memset(c,0,COMMAND);
    uint32_t *d=(void *)(a+DATA_OFFSET),*table=(void *)(a+0x2000);
    volatile uint64_t *fence=(void *)(c+0x1100);
    memcpy(a+0x1000,probe_code,sizeof(probe_code));
    for(unsigned i=0;i<LDS_WORDS;i++)d[i]=value(i);
    for(unsigned i=0;i<256;i++){d[LDS_WORDS+i]=selected(i);d[LDS_WORDS+256+i]=~value(selected(i));}
    uint64_t address=(uintptr_t)d;
    table[0]=(uint32_t)address; table[1]=(uint32_t)(address>>32);
    table[2]=WORDS*4; table[3]=0x31016fac; /* same raw B# as reference */
    *fence=0;
    r->shader_address=(uintptr_t)(a+0x1000);r->table_address=(uintptr_t)table;
    r->data_address=address;r->command_address=(uintptr_t)c;
    size_t count=psbc_probe_commands((void *)c,CAPACITY,r->shader_address,r->table_address,(uintptr_t)fence);
    if(!count){r->rc=-4;goto cleanup;}
    flush(a,ARENA);flush(c,COMMAND);
    r->stage=3;
    /* From this point no allocations may be freed without proven completion,
     * including an ambiguous nonzero submit result. */
    r->resources_retained=1;
    r->submit_rc=p->submit(p->context,(void *)c,(uint32_t)count);
    if(r->submit_rc){r->rc=-5;goto done;}
    r->suspend_rc=p->suspend_point(p->context);
    if(r->suspend_rc){r->rc=-6;goto done;}
    uint64_t deadline=p->monotonic_ns(p->context)+UINT64_C(3000000000);
    do {
        flush((const void *)fence,8);
        r->fence_observed=__atomic_load_n(fence,__ATOMIC_ACQUIRE);
        if(r->fence_observed==TOKEN)break;
        if(p->monotonic_ns(p->context)>deadline){r->rc=-7;goto done;}
        p->sleep_us(p->context,1000);
    }while(1);
    r->completion_observed=1;r->resources_retained=0;r->stage=4;
    flush(a,ARENA);
    for(unsigned i=0;i<LDS_WORDS;i++)r->input_errors+=d[i]!=value(i);
    for(unsigned i=0;i<256;i++){
        r->input_errors+=d[LDS_WORDS+i]!=selected(i);
        r->mismatches+=d[LDS_WORDS+256+i]!=value(selected(i));
    }
    for(unsigned i=0;i<DATA_OFFSET;i++){
        if(i>=0x1000&&i<0x1000+sizeof(probe_code))r->guard_errors+=a[i]!=probe_code[i-0x1000];
        else if(i>=0x2000&&i<0x2010)continue;
        else r->guard_errors+=a[i]!=0xa5;
    }
    for(unsigned i=DATA_OFFSET+WORDS*4;i<ARENA;i++)r->guard_errors+=a[i]!=0xa5;
    r->guard_errors+=table[0]!=(uint32_t)address;
    r->guard_errors+=table[1]!=(uint32_t)(address>>32);
    r->guard_errors+=table[2]!=WORDS*4;
    r->guard_errors+=table[3]!=0x31016fac;
    r->output_first=d[4352];r->expected_first=value(selected(0));
    r->output_last=d[4607];r->expected_last=value(selected(255));
    r->rc=(r->mismatches||r->input_errors||r->guard_errors)?-8:0;
cleanup:
    if(command.ptr&&p->release(p->context,&command)){r->resources_retained=1;r->rc=-9;goto done;}
    if(arena.ptr&&p->release(p->context,&arena)){r->resources_retained=1;r->rc=-9;goto done;}
    r->cleanup_complete=1;
done:
    r->elapsed_ns=p->monotonic_ns(p->context)-start;
    return r->rc;
}
