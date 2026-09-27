/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_app_catalog.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

int main(void) {
    uint8_t page[74] = {1};
    page[1] = 72;
    page[2] = 0x12;
    page[3] = 0x34;
    page[4] = 0x56;
    page[5] = 0x78;
    page[70] = 3;
    memcpy(page + 71, "ZCL", 3);
    blue_app_entry entries[1] = {0};
    size_t count = 99;
    assert(blue_app_catalog_parse(page, sizeof page, entries, 1,
                                  &count) == 0);
    assert(count == 1 && strcmp(entries[0].name, "ZCL") == 0);
    assert(entries[0].flags == 0x12345678);
    assert(blue_app_catalog_parse(page, 0, entries, 1, &count) == 0);
    assert(count == 0);
    assert(blue_app_catalog_parse(page, sizeof page - 1,
                                  entries, 1, &count) < 0);
    assert(blue_app_catalog_parse(page, sizeof page, entries, 0,
                                  &count) < 0);
    page[70] = 32;
    assert(blue_app_catalog_parse(page, sizeof page, entries, 1,
                                  &count) < 0);
    page[70] = 3;
    page[71] = '\n';
    assert(blue_app_catalog_parse(page, sizeof page, entries, 1,
                                  &count) < 0);
    page[0] = 2;
    assert(blue_app_catalog_parse(page, sizeof page, entries, 1,
                                  &count) < 0);
    return 0;
}
