/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Bound and parse worker receipts before swarm publication. */

#include "config/boot_zcode_work_receipt.h"

#include "vcs/vcs_object.h"
#include "vcs/zcode_dev.h"

#include <stdlib.h>

bool boot_zcode_work_receipt_load(
    const char *workspace, const uint8_t root[32],
    struct vcs_zcode_work_receipt_v1 *out)
{
    if (!workspace || !root || !out)
        return false;
    uint8_t *wire = NULL;
    size_t wire_len = 0;
    int loaded = vcs_object_load_raw_bounded(
        workspace, root, VCS_ZCODE_WORK_RECEIPT_WIRE_BYTES,
        &wire, &wire_len);
    bool parsed = loaded == 0 &&
        vcs_zcode_work_receipt_parse(wire, wire_len, out) ==
            VCS_ZCODE_DEV_OK;
    free(wire);
    return parsed;
}
