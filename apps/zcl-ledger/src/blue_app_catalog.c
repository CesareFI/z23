/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_app_catalog.h"

#include <stdbool.h>
#include <string.h>

static bool valid_name(const uint8_t *bytes, size_t length) {
    if (!length || length >= ZCL_BLUE_APP_NAME_SIZE) return false;
    for (size_t i = 0; i < length; ++i)
        if (bytes[i] < 0x20 || bytes[i] > 0x7e) return false;
    return true;
}

int blue_app_catalog_parse(const uint8_t *page, size_t length,
                           blue_app_entry *entries, size_t capacity,
                           size_t *count) {
    if (!page || !entries || !count) return -1;
    *count = 0;
    if (!length) return 0;
    if (page[0] != 1) return -1;
    size_t offset = 1;
    while (offset < length) {
        if (*count == capacity || length - offset < 70) return -1;
        ++offset; /* Ledger's entry-size field precedes the fixed fields. */
        uint32_t flags = 0;
        for (unsigned i = 0; i < 4; ++i)
            flags = (flags << 8) | page[offset++];
        offset += 64; /* Code/data hash and app hash. */
        size_t name_length = page[offset++];
        if (name_length > length - offset ||
            !valid_name(page + offset, name_length)) return -1;
        blue_app_entry *entry = &entries[*count];
        memcpy(entry->name, page + offset, name_length);
        entry->name[name_length] = 0;
        entry->flags = flags;
        offset += name_length;
        ++*count;
    }
    return 0;
}
