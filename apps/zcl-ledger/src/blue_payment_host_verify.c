/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_host_verify.h"

#include <string.h>

static bool inputs_valid(const uint8_t *reply, size_t reply_length,
    uint8_t expected_index, uint8_t expected_path,
    const uint8_t expected_hash160[20], const uint8_t digest[32],
    blue_payment_pubkey_hash_fn hash,
    blue_payment_verify_signature_fn verify) {
    return reply && expected_hash160 && digest && hash && verify &&
        expected_index < ZCL_TX_STREAM_MAX_INPUTS &&
        (expected_path == BLUE_PAYMENT_INPUT_EXTERNAL ||
         expected_path == BLUE_PAYMENT_INPUT_INTERNAL) &&
        reply_length >= 46 &&
        reply_length <= BLUE_PAYMENT_SIGN_REPLY_MAX + 2;
}

static bool frame_valid(const uint8_t *reply, size_t reply_length,
    uint8_t expected_index, uint8_t expected_path) {
    size_t der_length = reply[35];
    return reply[0] == expected_index && reply[1] == expected_path &&
        (reply[2] == 2 || reply[2] == 3) &&
        der_length >= 8 && der_length <= BLUE_ECDSA_DER_MAX &&
        reply_length == 36 + der_length + 2 &&
        reply[reply_length - 2] == 0x90 && reply[reply_length - 1] == 0;
}

static bool canonical_der(const uint8_t *der, size_t der_length) {
    uint8_t normalized[BLUE_ECDSA_DER_MAX];
    size_t normalized_length = 0;
    return blue_ecdsa_der_low_s(der, der_length,
        normalized, &normalized_length) &&
        normalized_length == der_length &&
        memcmp(normalized, der, der_length) == 0;
}

bool blue_payment_host_verify(const uint8_t *reply, size_t reply_length,
    uint8_t expected_index, uint8_t expected_path,
    const uint8_t expected_hash160[20], const uint8_t digest[32],
    blue_payment_pubkey_hash_fn hash,
    blue_payment_verify_signature_fn verify, void *verify_context,
    blue_payment_verified_signature *result) {
    if (result) memset(result, 0, sizeof *result);
    if (!result || !inputs_valid(reply, reply_length, expected_index,
            expected_path, expected_hash160, digest, hash, verify) ||
        !frame_valid(reply, reply_length, expected_index, expected_path) ||
        !canonical_der(reply + 36, reply[35])) return false;
    uint8_t hash160[20];
    if (!hash(reply + 2, hash160) ||
        memcmp(hash160, expected_hash160, sizeof hash160) != 0 ||
        !verify(verify_context, reply + 2, digest,
            reply + 36, reply[35])) return false;
    size_t der_length = reply[35];
    result->index = expected_index;
    result->path = expected_path;
    memcpy(result->digest, digest, sizeof result->digest);
    memcpy(result->public_key, reply + 2, sizeof result->public_key);
    memcpy(result->der, reply + 36, der_length);
    result->der_length = (uint8_t)der_length;
    return true;
}
