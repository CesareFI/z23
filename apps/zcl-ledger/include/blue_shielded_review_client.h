/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_SHIELDED_REVIEW_CLIENT_H
#define ZCL_BLUE_SHIELDED_REVIEW_CLIENT_H

#include "zcl_tx_review.h"

#include <stdbool.h>

typedef bool (*blue_shielded_exchange_fn)(void *context,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t capacity, size_t *reply_length);

/* Replays the exact Sapling-v4 wire six times and checks every device reply
 * against the independent host parser and ZIP-243 implementation. Failure
 * sends a best-effort erase command and leaves outputs zero. No signing or
 * trusted chain-tip decision is provided by this read-only client. */
bool blue_shielded_review_client_run(const uint8_t *wire, size_t length,
    uint32_t branch, blue_shielded_exchange_fn exchange, void *context,
    zcl_tx_review *review, uint8_t digest[32]);

#endif
