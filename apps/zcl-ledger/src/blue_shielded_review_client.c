/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_shielded_review_client.h"

#include "blue_mainnet_branch.h"
#include "zcl_zip243.h"
#include "zcl_zip243_host.h"

#include <string.h>

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

static void expected_facts(uint8_t bytes[44],
    const zcl_tx_review *review) {
    put_u32(bytes, review->transparent_inputs);
    put_u32(bytes + 4, review->transparent_outputs);
    put_u32(bytes + 8, review->sapling_spends);
    put_u32(bytes + 12, review->sapling_outputs);
    put_u32(bytes + 16, review->sprout_joinsplits);
    put_u64(bytes + 20, review->transparent_output_zat);
    put_u64(bytes + 28, (uint64_t)review->value_balance_zat);
    put_u32(bytes + 36, review->lock_time);
    put_u32(bytes + 40, review->expiry_height);
}

static bool checked_exchange(blue_shielded_exchange_fn exchange,
    void *context, const uint8_t *apdu, size_t length,
    uint8_t *body, size_t expected_length) {
    uint8_t reply[78];
    size_t received = 0;
    if (!exchange(context, apdu, length, reply, sizeof reply,
        &received) || received != expected_length + 2 ||
        reply[received - 2] != 0x90 || reply[received - 1] != 0)
        return false;
    if (body && expected_length) memcpy(body, reply, expected_length);
    return true;
}

static void best_effort_erase(blue_shielded_exchange_fn exchange,
    void *context) {
    const uint8_t erase[5] = {0xa5, 0x24, 0, 0, 0};
    uint8_t reply[8];
    size_t length = 0;
    (void)exchange(context, erase, sizeof erase,
        reply, sizeof reply, &length);
}

static bool upload_pass(blue_shielded_exchange_fn exchange, void *context,
    const uint8_t *wire, size_t length, uint8_t pass) {
    uint8_t apdu[225] = {0xa5, 0x21, 0, 0, 0};
    uint8_t progress[5];
    for (size_t offset = 0; offset < length; offset += 220) {
        size_t take = length - offset < 220 ? length - offset : 220;
        apdu[4] = (uint8_t)take;
        memcpy(apdu + 5, wire + offset, take);
        if (!checked_exchange(exchange, context, apdu, take + 5,
            progress, sizeof progress) || progress[0] != pass ||
            read_u32(progress + 1) != offset + take) return false;
    }
    return true;
}

static bool run_wire(blue_shielded_exchange_fn exchange, void *context,
    const uint8_t *wire, size_t length, uint32_t branch,
    const uint8_t expected[76]) {
    const uint8_t identify[5] = {0xa5, 0x01, 0, 0, 0};
    uint8_t body[76];
    if (!checked_exchange(exchange, context, identify,
        sizeof identify, body, 5) ||
        memcmp(body, "ZCL\x07\x40", 5)) return false;
    uint8_t begin[13] = {0xa5, 0x20, 0, 0, 8};
    put_u32(begin + 5, (uint32_t)length);
    put_u32(begin + 9, branch);
    if (!checked_exchange(exchange, context, begin,
        sizeof begin, NULL, 0)) return false;
    for (uint8_t pass = 1; pass <= 6; ++pass) {
        if (!upload_pass(exchange, context, wire, length, pass)) return false;
        if (pass < 6) {
            const uint8_t next[5] = {0xa5, 0x22, 0, 0, 0};
            uint8_t response;
            if (!checked_exchange(exchange, context, next, sizeof next,
                &response, 1) || response != pass + 1) return false;
        }
    }
    const uint8_t finish[5] = {0xa5, 0x23, 0, 0, 0};
    return checked_exchange(exchange, context, finish, sizeof finish,
        body, sizeof body) && memcmp(body, expected, sizeof body) == 0;
}

bool blue_shielded_review_client_run(const uint8_t *wire, size_t length,
    uint32_t branch, blue_shielded_exchange_fn exchange, void *context,
    zcl_tx_review *review, uint8_t digest[32]) {
    if (review) memset(review, 0, sizeof *review);
    if (digest) memset(digest, 0, 32);
    if (!wire || !length || length > ZCL_TX_REVIEW_MAX_BYTES ||
        !blue_mainnet_branch_is_known(branch) || !exchange ||
        !review || !digest) return false;
    zcl_tx_review parsed;
    struct blake2b_ctx hash_context;
    zcl_zip243_hasher hash = zcl_zip243_host_hasher(&hash_context);
    uint8_t candidate[76];
    if (zcl_tx_review_parse(wire, length, &parsed) < 0 ||
        zcl_zip243_shielded_digest(wire, length, branch,
            &hash, candidate + 44) < 0) return false;
    expected_facts(candidate, &parsed);
    if (!run_wire(exchange, context, wire, length,
        branch, candidate)) {
        best_effort_erase(exchange, context);
        return false;
    }
    *review = parsed;
    memcpy(digest, candidate + 44, 32);
    return true;
}
