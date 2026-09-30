/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Load one canonical worker receipt for swarm result publication. */

#ifndef ZCL_CONFIG_BOOT_ZCODE_WORK_RECEIPT_H
#define ZCL_CONFIG_BOOT_ZCODE_WORK_RECEIPT_H

#include <stdbool.h>
#include <stdint.h>

struct vcs_zcode_work_receipt_v1;

/* Missing, oversized, and malformed objects are all non-publishable. */
bool boot_zcode_work_receipt_load(
    const char *workspace, const uint8_t root[32],
    struct vcs_zcode_work_receipt_v1 *out);

#endif /* ZCL_CONFIG_BOOT_ZCODE_WORK_RECEIPT_H */
