/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "source_assessment_fixture.h"
#include <stdio.h>
static source_assessment_fixture fixture;
static bool write_fixture(const char *name, const uint8_t *wire, size_t length)
{
    FILE *file = fopen(name, "wbx");
    if (file == NULL) return false;
    const bool written = fwrite(wire, 1, length, file) == length;
    const int closed = fclose(file);
    return written && closed == 0;
}
int main(void)
{
    /* Run only in a new fixture directory; refuses existing output paths.
     * Opaque proof/signature bytes are synthetic and consensus-invalid. */
    if (!source_assessment_init(&fixture, 7)) return 1;
    if (!write_fixture("previous0", fixture.base.sources[0].wire, fixture.base.sources[0].length)) return 1;
    if (!write_fixture("previous1", fixture.base.sources[1].wire, fixture.base.sources[1].length)) return 1;
    uint8_t draft[ZCL_TX_WIRE_MAX];
    size_t length = 0;
    if (zcl_transaction_serialize(&fixture.base.spending, draft, sizeof(draft), &length) != ZCL_OK) return 1;
    return write_fixture("draft", draft, length) ? 0 : 1;
}
