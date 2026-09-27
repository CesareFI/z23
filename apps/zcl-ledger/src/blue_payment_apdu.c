/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_apdu.h"

#include <string.h>

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void put_u64(uint8_t bytes[8], uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
}

static bool capture_input(void *context, uint32_t index,
    const uint8_t outpoint[36], uint32_t sequence) {
    blue_payment_apdu *state = context;
    if (index != state->input_count ||
        index >= ZCL_TX_STREAM_MAX_INPUTS) return false;
    for (uint32_t i = 0; i < index; ++i)
        if (memcmp(state->outpoints[i], outpoint, 36) == 0) return false;
    memcpy(state->outpoints[index], outpoint, 36);
    state->sequences[index] = sequence;
    ++state->input_count;
    return true;
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
            read_u32(body + 4), read_u32(body + 8), blake, sha) ||
        !zcl_tx_replay_zip243_observe_inputs(&state->review.replay,
            capture_input, state))
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
            &facts, unused_digest) || facts.inputs != state->input_count)
        return 0x6a80;
    state->active = false;
    state->output_zat = facts.output_zat;
    reply[0] = (uint8_t)facts.outputs;
    *reply_length = 1;
    return 0x9000;
}

static uint16_t previous_begin(blue_payment_apdu *state,
    const uint8_t *body, uint8_t length,
    const zcl_tx_replay_sha256 *sha) {
    if (length != 4) return 0x6700;
    if (!sha || !sha->context || !sha->init || !sha->update || !sha->final ||
        !state->review.verified || state->active ||
        state->previous_active || state->fee_ready ||
        state->bound_inputs >= state->input_count) return 0x6985;
    zcl_tx_previous_sha256 hash = {.context = sha->context,
        .init = sha->init, .update = sha->update, .final = sha->final};
    if (!zcl_tx_previous_stream_begin(&state->previous,
            read_u32(body), read_u32(state->outpoints[state->bound_inputs] + 32),
            hash)) return 0x6a80;
    state->previous_active = true;
    return 0x9000;
}

static uint16_t previous_feed(blue_payment_apdu *state,
    const uint8_t *body, uint8_t length) {
    if (!state->previous_active) return 0x6985;
    if (!length) return 0x6700;
    return zcl_tx_previous_stream_feed(&state->previous, body, length)
        ? 0x9000 : 0x6a80;
}

static uint16_t previous_finish(blue_payment_apdu *state, uint8_t length,
    uint8_t *reply, size_t capacity, size_t *reply_length,
    const blue_payment_owned_hashes *owned) {
    if (!state->previous_active) return 0x6985;
    if (length || capacity < 43) return 0x6700;
    zcl_tx_previous_p2pkh output;
    const uint8_t *txid = state->outpoints[state->bound_inputs];
    uint8_t digest[32];
    if (!zcl_tx_previous_stream_finish(&state->previous, txid, &output) ||
        !owned ||
        (memcmp(output.script + 3, owned->external, 20) != 0 &&
         memcmp(output.script + 3, owned->internal, 20) != 0) ||
        output.value_zat > 2100000000000000ULL - state->input_zat ||
        !zcl_tx_replay_zip243_bound_digest(&state->review.replay,
            txid, state->sequences[state->bound_inputs], output.script,
            output.value_zat, digest))
        return 0x6a80;
    state->input_zat += output.value_zat;
    ++state->bound_inputs;
    state->previous_active = false;
    if (state->bound_inputs == state->input_count) {
        if (state->input_zat < state->output_zat) return 0x6a80;
        state->fee_zat = state->input_zat - state->output_zat;
        state->fee_ready = true;
    }
    reply[0] = (uint8_t)state->bound_inputs;
    reply[1] = (uint8_t)state->input_count;
    reply[2] = state->fee_ready;
    put_u64(reply + 3, state->fee_zat);
    memcpy(reply + 11, digest, sizeof digest);
    *reply_length = 43;
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
    blue_payment_hash_fn hash, const blue_payment_owned_hashes *owned) {
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
    case 0x26: return previous_begin(state, body, length, sha);
    case 0x27: return previous_feed(state, body, length);
    case 0x28: return previous_finish(state, length, reply, capacity,
                                      reply_length, owned);
    default: return 0x6d00;
    }
}

uint16_t blue_payment_apdu_handle(blue_payment_apdu *state,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t reply_capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha,
    blue_payment_hash_fn hash, const blue_payment_owned_hashes *owned) {
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
                           reply_length, blake, sha, hash, owned);
    if (result != 0x9000) {
        blue_payment_apdu_abort(state);
        *reply_length = 0;
    }
    return result;
}
