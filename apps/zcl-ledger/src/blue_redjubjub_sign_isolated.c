/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_redjubjub_sign_isolated.h"

#include "blue_jubjub_encode.h"
#include "blue_jubjub_lowmem.h"
#include "blue_mod256.h"
#include "blue_redjubjub_challenge.h"
#include "blue_redjubjub_nonce.h"
#include "blue_redjubjub_response.h"
#include "blue_sapling_generators.h"

#include <string.h>

static bool point_from_scalar(uint8_t encoded[32],
    const uint8_t scalar[32]) {
    struct jub_point point = {0};
    bool success = blue_jubjub_scalar_mul_lowmem(&point,
        &blue_spending_key_generator, scalar);
    if (success) success = blue_jubjub_encode(encoded, &point);
    blue_mod256_wipe(&point, sizeof point);
    return success;
}

bool blue_redjubjub_sign_isolated(uint8_t signature[64],
    const uint8_t secret[32], const uint8_t entropy[80],
    const uint8_t transaction_digest[32]) {
    if (!signature) return false;
    if (!secret || !entropy || !transaction_digest) {
        blue_mod256_wipe(signature, 64);
        return false;
    }
    uint8_t vkbar[32] = {0}, nonce[32] = {0};
    uint8_t challenge[32] = {0}, candidate[64] = {0};
    bool success = point_from_scalar(vkbar, secret);
    if (success) success = blue_redjubjub_nonce_from_entropy(nonce,
        entropy, vkbar, transaction_digest);
    if (success) success = point_from_scalar(candidate, nonce);
    if (success) success = blue_redjubjub_challenge(challenge,
        candidate, vkbar, transaction_digest);
    if (success) success = blue_redjubjub_response(candidate + 32,
        nonce, challenge, secret);
    if (success) memcpy(signature, candidate, sizeof candidate);
    else blue_mod256_wipe(signature, sizeof candidate);
    blue_mod256_wipe(vkbar, sizeof vkbar);
    blue_mod256_wipe(nonce, sizeof nonce);
    blue_mod256_wipe(challenge, sizeof challenge);
    blue_mod256_wipe(candidate, sizeof candidate);
    return success;
}
