/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_APDU_H
#define ZCL_BLUE_PAYMENT_APDU_H

#include "blue_payment_screen.h"
#include "zcl_tx_previous_stream.h"

typedef struct {
    blue_payment_review review;
    blue_payment_screen screen;
    zcl_tx_previous_stream previous;
    uint8_t outpoints[ZCL_TX_STREAM_MAX_INPUTS][36];
    uint32_t sequences[ZCL_TX_STREAM_MAX_INPUTS];
    uint64_t input_zat, output_zat, fee_zat;
    uint32_t input_count, bound_inputs;
    bool active, previous_active, fee_ready;
    uint8_t input_paths;
} blue_payment_apdu;

typedef struct {
    uint8_t external[20];
    uint8_t internal[20];
} blue_payment_owned_hashes;

/* Read-only CLA A5 commands 20-28. A physical touch acknowledges each
 * spending output. Previous wires can be uploaded only after a verified
 * three-pass review; each one must match the next captured input outpoint.
 * A finished previous wire must pay to one of the device-derived hashes and
 * yields that input's device-derived ZIP-243 digest.
 * No command accesses a key, approves a payment, or signs. */
uint16_t blue_payment_apdu_handle(blue_payment_apdu *state,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha,
    blue_payment_hash_fn hash, const blue_payment_owned_hashes *owned);

/* Only a physical touchscreen callback may call this function on device. */
bool blue_payment_apdu_touch_continue(blue_payment_apdu *state);
void blue_payment_apdu_abort(blue_payment_apdu *state);

#endif
