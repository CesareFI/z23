/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_shielded_review_apdu.h"

#include "blue_mainnet_branch.h"

#include <string.h>

static_assert(sizeof(blue_shielded_review_state) <= 672,
    "Shielded review controller exceeds its memory budget");

void blue_shielded_review_abort(blue_shielded_review_state *state) {
    if (!state) return;
    volatile uint8_t *bytes = (volatile uint8_t *)state;
    for (size_t i = 0; i < sizeof *state; ++i) bytes[i] = 0;
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void put_u32(uint8_t *bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (8 * i));
}

static void put_u64(uint8_t *bytes, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(value >> (8 * i));
}

static uint16_t request_status(const uint8_t *apdu, size_t length) {
    if (length < 5 || length != 5u + apdu[4]) return 0x6700;
    if (apdu[0] != 0xa5) return 0x6e00;
    if (apdu[2] || apdu[3]) return 0x6b00;
    return 0x9000;
}

static uint16_t identify(size_t length, uint8_t *reply,
    size_t capacity, size_t *reply_length) {
    if (length || capacity < 5) return 0x6700;
    memcpy(reply, "ZCL\x07\x40", 5);
    *reply_length = 5;
    return 0x9000;
}

static uint16_t begin(blue_shielded_review_state *state,
    const uint8_t *body, size_t length,
    const zcl_zip243_hasher *blake) {
    if (length != 8) return 0x6700;
    uint32_t branch = read_u32(body + 4);
    if (!blue_mainnet_branch_is_known(branch)) return 0x6a80;
    blue_shielded_review_abort(state);
    if (!zcl_tx_shielded_replay_begin(&state->replay,
        read_u32(body), branch, blake)) return 0x6a80;
    state->active = true;
    return 0x9000;
}

static uint16_t feed(blue_shielded_review_state *state,
    const uint8_t *body, size_t length, uint8_t *reply,
    size_t capacity, size_t *reply_length) {
    if (!state->active) return 0x6985;
    if (!length || length > BLUE_SHIELDED_REVIEW_CHUNK_MAX ||
        capacity < 5) return 0x6700;
    if (!zcl_tx_shielded_replay_feed(&state->replay, body, length))
        return 0x6a80;
    reply[0] = state->replay.pass;
    put_u32(reply + 1, state->replay.wire.received);
    *reply_length = 5;
    return 0x9000;
}

static uint16_t next(blue_shielded_review_state *state,
    size_t length, uint8_t *reply, size_t capacity,
    size_t *reply_length) {
    if (!state->active) return 0x6985;
    if (length || capacity < 1) return 0x6700;
    if (!zcl_tx_shielded_replay_next(&state->replay)) return 0x6a80;
    reply[0] = state->replay.pass;
    *reply_length = 1;
    return 0x9000;
}

static void encode_facts(uint8_t reply[44],
    const zcl_tx_shielded_facts *facts) {
    put_u32(reply, facts->transparent_inputs);
    put_u32(reply + 4, facts->transparent_outputs);
    put_u32(reply + 8, facts->sapling_spends);
    put_u32(reply + 12, facts->sapling_outputs);
    put_u32(reply + 16, facts->sprout_joinsplits);
    put_u64(reply + 20, facts->transparent_output_zat);
    put_u64(reply + 28, (uint64_t)facts->value_balance_zat);
    put_u32(reply + 36, facts->lock_time);
    put_u32(reply + 40, facts->expiry_height);
}

static uint16_t finish(blue_shielded_review_state *state,
    size_t length, uint8_t *reply, size_t capacity,
    size_t *reply_length) {
    if (!state->active) return 0x6985;
    if (length || capacity < 76) return 0x6700;
    uint8_t digest[32];
    if (!zcl_tx_shielded_replay_finish(&state->replay,
        &state->facts, digest)) return 0x6a80;
    memcpy(state->digest, digest, sizeof digest);
    encode_facts(reply, &state->facts);
    memcpy(reply + 44, digest, sizeof digest);
    memset(&state->replay, 0, sizeof state->replay);
    state->active = false;
    state->complete = true;
    *reply_length = 76;
    return 0x9000;
}

static uint16_t dispatch(blue_shielded_review_state *state,
    const uint8_t *apdu, uint8_t *reply, size_t capacity,
    size_t *reply_length, const zcl_zip243_hasher *blake) {
    const uint8_t *body = apdu + 5;
    size_t length = apdu[4];
    switch (apdu[1]) {
    case 0x01: return identify(length, reply, capacity, reply_length);
    case 0x20: return begin(state, body, length, blake);
    case 0x21: return feed(state, body, length,
        reply, capacity, reply_length);
    case 0x22: return next(state, length, reply,
        capacity, reply_length);
    case 0x23: return finish(state, length, reply,
        capacity, reply_length);
    case 0x24:
        if (length) return 0x6700;
        blue_shielded_review_abort(state);
        return 0x9000;
    default: return 0x6d00;
    }
}

uint16_t blue_shielded_review_handle(blue_shielded_review_state *state,
    const uint8_t *apdu, size_t apdu_length,
    uint8_t *reply, size_t capacity, size_t *reply_length,
    const zcl_zip243_hasher *blake) {
    if (!state || !apdu || !reply || !reply_length) {
        blue_shielded_review_abort(state);
        if (reply_length) *reply_length = 0;
        return 0x6a80;
    }
    *reply_length = 0;
    uint16_t status = request_status(apdu, apdu_length);
    if (status == 0x9000)
        status = dispatch(state, apdu, reply, capacity,
            reply_length, blake);
    if (status != 0x9000) {
        blue_shielded_review_abort(state);
        *reply_length = 0;
    }
    return status;
}
