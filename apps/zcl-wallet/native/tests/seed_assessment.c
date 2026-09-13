/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include <stdio.h>
static assessment_fixture fixture;
static bool write_seed(const char *name, const uint8_t *wire, size_t length)
{
    FILE *file = fopen(name, "wbx");
    if (file == NULL) return false;
    const bool written = fwrite(wire, 1, length, file) == length;
    const int closed = fclose(file);
    return written && closed == 0;
}
int main(void)
{
    if (!assessment_fixture_init(&fixture)) return 1;
    /* Caller creates a new corpus directory before running this fixture. */
    const char *names[2] = {"previous0", "previous1"};
    for (size_t i = 0; i < 2; ++i) {
        if (!write_seed(names[i], fixture.wire[i], fixture.sources[i].length)) return 1;
    }
    uint8_t draft[ZCL_TX_WIRE_MAX];
    size_t length = 0;
    if (zcl_transaction_serialize(&fixture.spending, draft, sizeof(draft), &length) != ZCL_OK) return 1;
    return write_seed("draft", draft, length) ? 0 : 1;
}
