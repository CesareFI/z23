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
