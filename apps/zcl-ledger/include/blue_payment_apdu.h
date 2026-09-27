/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_APDU_H
#define ZCL_BLUE_PAYMENT_APDU_H

#include "blue_payment_screen.h"

typedef struct {
    blue_payment_review review;
    blue_payment_screen screen;
    bool active;
} blue_payment_apdu;

/* Read-only CLA A5 commands 20-25. No command acknowledges an output,
 * accesses a key, or produces a signature. A nonpayment command must cancel
 * an active review before the caller dispatches it elsewhere. */
uint16_t blue_payment_apdu_handle(blue_payment_apdu *state,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha,
    blue_payment_hash_fn hash);

/* Only a physical touchscreen callback may call this function on device. */
bool blue_payment_apdu_touch_continue(blue_payment_apdu *state);
void blue_payment_apdu_abort(blue_payment_apdu *state);

#endif
