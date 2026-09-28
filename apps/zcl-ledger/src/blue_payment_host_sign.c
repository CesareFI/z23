/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_host_sign.h"

#include <string.h>

static bool signing_identity(blue_payment_live_exchange exchange,
    void *context) {
    static const uint8_t request[5] = {0xa5, 0x01, 0, 0, 0};
    static const uint8_t expected[7] = {'Z', 'C', 'L', 12, 31, 0x90, 0};
    uint8_t reply[7];
    size_t length = 0;
    return exchange(context, request, sizeof request, reply, sizeof reply,
        &length) && length == sizeof expected &&
        memcmp(reply, expected, sizeof expected) == 0;
}

static void abort_review(blue_payment_live_exchange exchange,
    void *context) {
    static const uint8_t request[5] = {0xa5, 0x24, 0, 0, 0};
    uint8_t reply[2];
    size_t length = 0;
    (void)exchange(context, request, sizeof request, reply, sizeof reply,
        &length);
}

static bool request_signature(size_t index, const uint8_t *paths,
    const uint8_t (*hashes)[20], const uint8_t (*digests)[32],
    blue_payment_live_exchange exchange, void *device_context,
    blue_payment_pubkey_hash_fn hash,
    blue_payment_verify_signature_fn verify, void *verify_context,
    blue_payment_verified_signature *result) {
    uint8_t request[6] = {0xa5, 0x29, 0, 0, 1, (uint8_t)index};
    uint8_t reply[BLUE_PAYMENT_SIGN_REPLY_MAX + 2];
    size_t length = 0;
    return exchange(device_context, request, sizeof request,
            reply, sizeof reply, &length) &&
        blue_payment_host_verify(reply, length, (uint8_t)index,
            paths[index], hashes[index], digests[index], hash, verify,
            verify_context, result);
}

static bool signing_arguments_valid(size_t count, const uint8_t *paths,
    const uint8_t (*hashes)[20], const uint8_t (*digests)[32],
    blue_payment_live_exchange exchange,
    blue_payment_host_approval approval,
    blue_payment_pubkey_hash_fn hash,
    blue_payment_verify_signature_fn verify,
    blue_payment_verified_signature *signatures) {
    return signatures && count && count <= ZCL_TX_STREAM_MAX_INPUTS &&
        paths && hashes && digests && exchange && approval && hash && verify;
}

bool blue_payment_host_sign(size_t count, const uint8_t *paths,
    const uint8_t (*hashes)[20], const uint8_t (*digests)[32],
    blue_payment_live_exchange exchange,
    blue_payment_host_approval approval, void *device_context,
    blue_payment_pubkey_hash_fn hash,
    blue_payment_verify_signature_fn verify, void *verify_context,
    blue_payment_verified_signature *signatures) {
    bool bounded = signatures && count && count <= ZCL_TX_STREAM_MAX_INPUTS;
    if (bounded) memset(signatures, 0, count * sizeof *signatures);
    if (!signing_arguments_valid(count, paths, hashes, digests, exchange,
            approval, hash, verify, signatures)) {
        if (exchange) abort_review(exchange, device_context);
        return false;
    }
    bool valid = signing_identity(exchange, device_context) &&
        approval(device_context);
    for (size_t i = 0; valid && i < count; ++i)
        valid = request_signature(i, paths, hashes, digests, exchange,
            device_context, hash, verify, verify_context, &signatures[i]);
    if (valid) return true;
    memset(signatures, 0, count * sizeof *signatures);
    abort_review(exchange, device_context);
    return false;
}
