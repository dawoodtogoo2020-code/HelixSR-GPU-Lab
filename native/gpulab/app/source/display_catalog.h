#ifndef DISPLAY_CATALOG_H
#define DISPLAY_CATALOG_H
#include <stddef.h>
#define DC_MAX_ENTRIES 32u
#define DC_KEY_BYTES 64u
#define DC_DIGEST_BYTES 65u
#define DC_LABEL_BYTES 96u
typedef struct { char device[DC_KEY_BYTES], title[DC_KEY_BYTES], build[DC_KEY_BYTES], digest[DC_DIGEST_BYTES]; } dc_identity;
typedef struct { dc_identity identity; char label[DC_LABEL_BYTES]; } dc_entry;
typedef struct { size_t count; dc_entry entries[DC_MAX_ENTRIES]; } dc_catalog;
typedef enum { DC_OK=0, DC_INVALID=1, DC_DUPLICATE=2, DC_NOT_FOUND=3 } dc_result;
/* Caller initializes catalog with {0}. This is display metadata only, never trust or activation authority. */
int dc_identity_valid(const dc_identity *id);
int dc_identity_equal(const dc_identity *a,const dc_identity *b);
dc_result dc_replace(dc_catalog *catalog,const dc_entry *entries,size_t count);
dc_result dc_find(const dc_catalog *catalog,const dc_identity *identity,size_t *index);
#endif
