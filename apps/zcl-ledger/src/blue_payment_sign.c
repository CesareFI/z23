/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_sign.h"

#include <string.h>

static void wipe(void *memory, size_t length) {
    volatile uint8_t *bytes = memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static bool account_matches(uint8_t path, const uint8_t public_key[33],
    const uint8_t hash160[20], const blue_payment_owned_hashes *owned) {
    if (public_key[0] != 2 && public_key[0] != 3) return false;
    if (path == BLUE_PAYMENT_INPUT_EXTERNAL)
        return memcmp(hash160, owned->external, 20) == 0;
    if (path == BLUE_PAYMENT_INPUT_INTERNAL)
        return memcmp(hash160, owned->internal, 20) == 0;
    return false;
}

bool blue_payment_sign_next(blue_payment_apdu *state, uint8_t index,
    const blue_payment_owned_hashes *owned,
    blue_payment_sign_digest_fn signer, void *signer_context,
    blue_payment_pubkey_hash_fn hash,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (!reply_length) {
        blue_payment_apdu_abort(state);
        return false;
    }
    *reply_length = 0;
    if (reply) memset(reply, 0, capacity);
    if (!state || !owned || !signer || !hash || !reply ||
        capacity < BLUE_PAYMENT_SIGN_REPLY_MAX) {
        blue_payment_apdu_abort(state);
        return false;
    }
    uint8_t digest[32] = {0}, public_key[33] = {0},
        signature[BLUE_ECDSA_DER_MAX] = {0},
        normalized[BLUE_ECDSA_DER_MAX] = {0}, hash160[20] = {0};
    uint8_t path = 0;
    size_t signature_length = 0, normalized_length = 0;
    bool valid = blue_payment_apdu_take_digest(state, index, digest, &path) &&
        signer(signer_context, path, digest, public_key,
               signature, &signature_length) &&
        hash(public_key, hash160) &&
        account_matches(path, public_key, hash160, owned) &&
        blue_ecdsa_der_low_s(signature, signature_length,
                             normalized, &normalized_length);
    if (valid) {
        reply[0] = index;
        reply[1] = path;
        memcpy(reply + 2, public_key, 33);
        reply[35] = (uint8_t)normalized_length;
        memcpy(reply + 36, normalized, normalized_length);
        *reply_length = 36 + normalized_length;
    } else blue_payment_apdu_abort(state);
    wipe(digest, sizeof digest);
    wipe(signature, sizeof signature);
    wipe(normalized, sizeof normalized);
    wipe(public_key, sizeof public_key);
    wipe(hash160, sizeof hash160);
    return valid;
}

static uint16_t command_frame_status(const uint8_t *apdu,
    size_t apdu_length, size_t reply_capacity) {
    if (apdu_length != 6 || apdu[4] != 1 ||
        reply_capacity < BLUE_PAYMENT_SIGN_REPLY_MAX) return 0x6700;
    if (apdu[0] != 0xa5) return 0x6e00;
    if (apdu[2] || apdu[3]) return 0x6b00;
    if (apdu[1] != 0x29) return 0x6d00;
    return 0x9000;
}

uint16_t blue_payment_sign_command(blue_payment_apdu *state,
    const uint8_t *apdu, size_t apdu_length,
    const blue_payment_owned_hashes *owned,
    blue_payment_sign_digest_fn signer, void *signer_context,
    blue_payment_pubkey_hash_fn hash,
    uint8_t *reply, size_t capacity, size_t *reply_length) {
    if (reply_length) *reply_length = 0;
    if (!state || !apdu || !owned || !signer || !hash ||
        !reply || !reply_length) {
        if (reply) memset(reply, 0, capacity);
        blue_payment_apdu_abort(state);
        return 0x6f00;
    }
    uint16_t status = command_frame_status(apdu, apdu_length, capacity);
    uint8_t index = status == 0x9000 ? apdu[5] : 0;
    if (status == 0x9000 && !blue_payment_sign_next(state, index, owned,
            signer, signer_context, hash, reply, capacity, reply_length))
        status = 0x6985;
    if (status != 0x9000) {
        memset(reply, 0, capacity);
        blue_payment_apdu_abort(state);
        *reply_length = 0;
    }
    return status;
}
