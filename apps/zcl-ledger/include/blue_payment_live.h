/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_LIVE_H
#define ZCL_BLUE_PAYMENT_LIVE_H

#include "blue_payment_simulate.h"
#include "zcl_tx_prevout.h"

typedef struct {
    blue_payment_screen screens[BLUE_PAYMENT_REVIEW_MAX_OUTPUTS];
    uint32_t output_end[BLUE_PAYMENT_REVIEW_MAX_OUTPUTS];
    uint8_t wire_hash[32];
    uint32_t count, inputs, branch_id, wire_length;
} blue_payment_live_plan;

typedef bool (*blue_payment_live_exchange)(void *context,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t capacity, size_t *reply_length);

/* Called while an output is visible. It must return true only after the
 * owner physically taps CONTINUE. The driver queries device state afterward. */
typedef bool (*blue_payment_live_continue)(void *context,
    uint32_t index, const blue_payment_screen *screen);

/* The plan accepts only an unsigned all-transparent v4 transaction. It has no
 * signing authority and never treats host screen text as device evidence. */
bool blue_payment_live_prepare(const uint8_t *wire, size_t length,
    uint32_t branch_id, blue_payment_live_plan *plan);

/* Replays the exact planned bytes to Wallet protocol 10/capability 7. Every
 * output chunk ends on its final byte; a verified touchscreen callback is
 * required before the next chunk. Failure attempts a read-only review abort. */
bool blue_payment_live_run(const uint8_t *wire, size_t length,
    const blue_payment_live_plan *plan,
    blue_payment_live_exchange exchange,
    blue_payment_live_continue continuation, void *context);

/* After a complete read-only output review, streams each supplied previous
 * wire to the Blue and checks its independently derived fee. The caller must
 * first preflight chain provenance; neither this result nor the prior review
 * authorizes signing. */
bool blue_payment_live_run_bound(const uint8_t *wire, size_t length,
    const blue_payment_live_plan *plan,
    const zcl_tx_previous_transaction *previous, size_t previous_count,
    uint64_t expected_fee_zat, blue_payment_live_exchange exchange,
    blue_payment_live_continue continuation, void *context);

#endif
