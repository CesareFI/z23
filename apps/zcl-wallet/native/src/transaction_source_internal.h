/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_SOURCE_INTERNAL_H
#define ZCL_TRANSACTION_SOURCE_INTERNAL_H
#include "zcl_transaction.h"

/* Pinned original Zclassic v4 serialized transaction limit, NOT a Zcash SDK
 * limit. This reader inspects public bytes only and never authorizes funding.
 * The existing spend codec, prevout admission and JNI limits are unchanged. */
#define ZCL_V4_SOURCE_MAX ((size_t)102000)
typedef struct {
    uint8_t transaction_id[32]; /* Full raw wire SHA256d, displayed order. */
    zcl_tx_output output;       /* Selected transparent row, owned script. */
    size_t input_count, output_count, spend_count, shielded_count, joinsplit_count;
    uint32_t lock_time, expiry_height;
    int64_t value_balance;     /* Raw signed field, not verified accounting. */
} zcl_v4_source;

/* Parse the complete canonical v4 layout, including opaque Sapling descriptions,
 * Groth JoinSplits and their conditional signatures. Transparent script lengths
 * and vector counts are bounded by remaining bytes, not the small spend profile.
 * Only the selected output script must fit zcl_tx_output (<=25 bytes). All
 * transparent output values and their sum must be within MAX_MONEY.
 * Full byte/hash/index consistency only: NO proof, signature, script, duplicate
 * outpoint/nullifier, maturity, finality, inclusion or unspentness validation.
 * Legacy/v3/NU5/Orchard refuse. No global state, heap, retained pointer or I/O.
 * Caller owns stable nonoverlapping spans. Entire output unchanged on failure.
 * This is not a replacement for zcl_transaction_prevout admission. */
zcl_status zcl_v4_source_inspect(const uint8_t *wire, size_t length,
    uint32_t output_index, zcl_v4_source *output);
#endif
