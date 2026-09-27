/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_apdu.h"

#include <string.h>

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

void blue_payment_apdu_abort(blue_payment_apdu *state) {
    if (!state) return;
    memset(state, 0, sizeof *state);
    blue_payment_review_abort(&state->review);
}

bool blue_payment_apdu_touch_continue(blue_payment_apdu *state) {
    if (!state || !state->active ||
        !blue_payment_review_acknowledge(&state->review)) return false;
    memset(&state->screen, 0, sizeof state->screen);
    return true;
}

static uint16_t begin(blue_payment_apdu *state, const uint8_t *body,
    uint8_t length, const zcl_zip243_hasher *blake,
    const zcl_tx_replay_sha256 *sha) {
    if (length != 12) return 0x6700;
    blue_payment_apdu_abort(state);
    if (!blue_payment_review_begin(&state->review, read_u32(body),
            read_u32(body + 4), read_u32(body + 8), blake, sha))
        return 0x6a80;
    state->active = true;
    return 0x9000;
}

static uint16_t feed(blue_payment_apdu *state, const uint8_t *body,
    uint8_t length, uint8_t *reply, size_t capacity, size_t *reply_length,
    blue_payment_hash_fn hash) {
    if (!state->active) return 0x6985;
    if (!length || capacity < 2) return 0x6700;
    if (!blue_payment_review_feed(&state->review, body, length)) return 0x6a80;
    const blue_payment_output *pending =
        blue_payment_review_pending(&state->review);
    if (pending && !blue_payment_screen_format(pending,
            state->review.total_outputs, hash, &state->screen)) return 0x6a80;
    reply[0] = state->review.replay.pass;
    reply[1] = pending ? 1 : 0;
    *reply_length = 2;
    return 0x9000;
}

static uint16_t next(blue_payment_apdu *state,
    uint8_t length, uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (!state->active) return 0x6985;
    if (length || capacity < 2) return 0x6700;
    if (!blue_payment_review_next_pass(&state->review)) return 0x6a80;
    reply[0] = state->review.replay.pass;
    reply[1] = (uint8_t)state->review.total_outputs;
    *reply_length = 2;
    return 0x9000;
}

static uint16_t finish(blue_payment_apdu *state, uint8_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (!state->active) return 0x6985;
    if (length || capacity < 1) return 0x6700;
    zcl_tx_stream_facts facts;
    uint8_t unused_digest[32];
    if (!blue_payment_review_finish(&state->review, NULL, 0, 0,
            &facts, unused_digest)) return 0x6a80;
    state->active = false;
    reply[0] = (uint8_t)facts.outputs;
    *reply_length = 1;
    return 0x9000;
}

static uint16_t status(const blue_payment_apdu *state, uint8_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (length || capacity < 6) return 0x6700;
    reply[0] = state->active;
    reply[1] = state->review.replay.pass;
    reply[2] = state->review.pending;
    reply[3] = state->review.verified;
    reply[4] = (uint8_t)state->review.total_outputs;
    reply[5] = (uint8_t)state->review.acknowledged;
    *reply_length = 6;
    return 0x9000;
}

static uint16_t dispatch(blue_payment_apdu *state, const uint8_t *apdu,
    uint8_t *reply, size_t capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha,
    blue_payment_hash_fn hash) {
    const uint8_t *body = apdu + 5;
    uint8_t length = apdu[4];
    switch (apdu[1]) {
    case 0x20: return begin(state, body, length, blake, sha);
    case 0x21: return feed(state, body, length, reply, capacity,
                           reply_length, hash);
    case 0x22: return next(state, length, reply, capacity, reply_length);
    case 0x23: return finish(state, length, reply, capacity, reply_length);
    case 0x24:
        if (length) return 0x6700;
        blue_payment_apdu_abort(state);
        return 0x9000;
    case 0x25: return status(state, length, reply, capacity, reply_length);
    default: return 0x6d00;
    }
}

uint16_t blue_payment_apdu_handle(blue_payment_apdu *state,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha,
    blue_payment_hash_fn hash) {
    if (!state || !reply_length) return 0x6f00;
    *reply_length = 0;
    if (!apdu || !reply || !hash) {
        blue_payment_apdu_abort(state);
        return 0x6f00;
    }
    uint16_t result = 0x9000;
    if (apdu_length < 5 || apdu_length != (size_t)apdu[4] + 5)
        result = 0x6700;
    else if (apdu[0] != 0xa5) result = 0x6e00;
    else if (apdu[2] || apdu[3]) result = 0x6b00;
    else result = dispatch(state, apdu, reply, reply_capacity,
                           reply_length, blake, sha, hash);
    if (result != 0x9000) {
        blue_payment_apdu_abort(state);
        *reply_length = 0;
    }
    return result;
}
