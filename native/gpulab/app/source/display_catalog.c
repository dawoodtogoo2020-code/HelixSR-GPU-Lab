/* Display-only metadata catalog. Values are fabricated display data and never
 * confer device, backend, trust or activation authority. */
#include "display_catalog.h"

#include <string.h>

/* Returns the length of a bounded NUL-terminated string, or cap if no NUL is
 * present within cap bytes. Never reads beyond cap bytes. */
static size_t dc_bounded_len(const char *s, size_t cap)
{
    size_t i;
    for (i = 0; i < cap; ++i) {
        if (s[i] == '\0') {
            return i;
        }
    }
    return cap;
}

static int dc_key_char_ok(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' ||
           c == '-';
}

static int dc_key_valid(const char key[DC_KEY_BYTES])
{
    size_t len = dc_bounded_len(key, DC_KEY_BYTES);
    size_t i;
    if (len < 1u || len >= DC_KEY_BYTES) {
        return 0;
    }
    for (i = 0; i < len; ++i) {
        if (!dc_key_char_ok((unsigned char)key[i])) {
            return 0;
        }
    }
    return 1;
}

static int dc_digest_valid(const char digest[DC_DIGEST_BYTES])
{
    size_t i;
    for (i = 0; i < DC_DIGEST_BYTES - 1u; ++i) {
        unsigned char c = (unsigned char)digest[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return digest[DC_DIGEST_BYTES - 1u] == '\0';
}

static int dc_label_valid(const char label[DC_LABEL_BYTES])
{
    size_t len = dc_bounded_len(label, DC_LABEL_BYTES);
    size_t i;
    if (len < 1u || len >= DC_LABEL_BYTES) {
        return 0;
    }
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)label[i];
        if (c < 32u || c > 126u) {
            return 0;
        }
    }
    return 1;
}

static int dc_entry_valid(const dc_entry *e)
{
    return dc_identity_valid(&e->identity) && dc_label_valid(e->label);
}

/* Compares two keys already known to be valid; bytes after NUL are ignored. */
static int dc_key_equal(const char *a, const char *b)
{
    size_t la = dc_bounded_len(a, DC_KEY_BYTES);
    size_t lb = dc_bounded_len(b, DC_KEY_BYTES);
    return la == lb && memcmp(a, b, la) == 0;
}

/* Exact four-field match of identities already known to be valid. */
static int dc_identity_equal_valid(const dc_identity *a, const dc_identity *b)
{
    return dc_key_equal(a->device, b->device) &&
           dc_key_equal(a->title, b->title) &&
           dc_key_equal(a->build, b->build) &&
           memcmp(a->digest, b->digest, DC_DIGEST_BYTES - 1u) == 0;
}

/* Validates every entry and rejects duplicate full identities. */
static dc_result dc_entries_check(const dc_entry *entries, size_t count)
{
    size_t i;
    size_t j;
    for (i = 0; i < count; ++i) {
        if (!dc_entry_valid(&entries[i])) {
            return DC_INVALID;
        }
    }
    for (i = 0; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            if (dc_identity_equal_valid(&entries[i].identity,
                                        &entries[j].identity)) {
                return DC_DUPLICATE;
            }
        }
    }
    return DC_OK;
}

static void dc_copy_key(char dst[DC_KEY_BYTES], const char src[DC_KEY_BYTES])
{
    memcpy(dst, src, dc_bounded_len(src, DC_KEY_BYTES));
}

int dc_identity_valid(const dc_identity *id)
{
    if (id == NULL) {
        return 0;
    }
    return dc_key_valid(id->device) && dc_key_valid(id->title) &&
           dc_key_valid(id->build) && dc_digest_valid(id->digest);
}

int dc_identity_equal(const dc_identity *a, const dc_identity *b)
{
    if (!dc_identity_valid(a) || !dc_identity_valid(b)) {
        return 0;
    }
    return dc_identity_equal_valid(a, b);
}

dc_result dc_replace(dc_catalog *catalog, const dc_entry *entries, size_t count)
{
    dc_catalog staged;
    dc_result rc;
    size_t i;

    if (catalog == NULL || count > DC_MAX_ENTRIES) {
        return DC_INVALID;
    }
    if (count > 0u && entries == NULL) {
        return DC_INVALID;
    }
    if (count > 0u) {
        rc = dc_entries_check(entries, count);
        if (rc != DC_OK) {
            return rc;
        }
    }

    /* Stage first so entries may alias catalog->entries, including subranges. */
    memset(&staged, 0, sizeof staged);
    for (i = 0; i < count; ++i) {
        const dc_entry *src = &entries[i];
        dc_entry *dst = &staged.entries[i];
        dc_copy_key(dst->identity.device, src->identity.device);
        dc_copy_key(dst->identity.title, src->identity.title);
        dc_copy_key(dst->identity.build, src->identity.build);
        memcpy(dst->identity.digest, src->identity.digest,
               DC_DIGEST_BYTES - 1u);
        memcpy(dst->label, src->label,
               dc_bounded_len(src->label, DC_LABEL_BYTES));
    }
    staged.count = count;

    memcpy(catalog, &staged, sizeof staged);
    return DC_OK;
}

dc_result dc_find(const dc_catalog *catalog, const dc_identity *identity,
                  size_t *index)
{
    size_t i;

    if (catalog == NULL || identity == NULL || index == NULL) {
        return DC_INVALID;
    }
    if (catalog->count > DC_MAX_ENTRIES) {
        return DC_INVALID;
    }
    if (!dc_identity_valid(identity)) {
        return DC_INVALID;
    }
    if (dc_entries_check(catalog->entries, catalog->count) != DC_OK) {
        return DC_INVALID;
    }
    for (i = 0; i < catalog->count; ++i) {
        if (dc_identity_equal_valid(&catalog->entries[i].identity, identity)) {
            *index = i;
            return DC_OK;
        }
    }
    return DC_NOT_FOUND;
}
