#ifndef HX_TRIPLET_ADMISSION_H
#define HX_TRIPLET_ADMISSION_H
#include "triplet.h"
typedef struct {
    hvptr magic;unsigned version,size,seq,calls;hvptr mask,tick;
    unsigned aa,primary,explicit_cut,cf48,history,phase;
    hv_point active_min,active,display_min,display;
    unsigned state_valid,uniform_valid,map_valid,blocks[4];
    struct {unsigned valid,vtable;hv_point alloc,lo,hi;hvptr metadata,name_rva;} tex[3];
} ht_admission;
_Static_assert(sizeof(ht_admission)==272,"admission schema");
#endif
