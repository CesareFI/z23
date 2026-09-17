/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_HEADER_INTERNAL_H
#define ZCL_HEADER_INTERNAL_H
#include "zcl_wallet.h"

typedef struct {
    uint8_t hash[32], previous[32], merkle[32], sapling[32], nonce[32];
    uint32_t version_bits, timestamp, bits;
    size_t solution_length;
} zcl_header_view;

/* Owned public fields, all uint256 values in display order. Version preserves
 * the unsigned wire bits. Network/height select only the existing serialized
 * solution shape; neither is authenticated or retained. This is NOT Equihash,
 * difficulty, ancestry, transaction inclusion or consensus validation.
 * Stable input/output spans must not overlap. No pointers survive; output is
 * unchanged on failure. No allocation, I/O or signing authority. */
zcl_status zcl_header_inspect(const uint8_t *wire, size_t length,
    zcl_network network, uint32_t height, zcl_header_view *view);
#endif
