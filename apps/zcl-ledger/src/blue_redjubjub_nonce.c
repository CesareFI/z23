/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_redjubjub_nonce.h"

#include "crypto/blake2b.h"
#include "sapling/jubjub.h"

#include <stddef.h>
#include <string.h>

static void wipe(void *memory, size_t length) {
    volatile uint8_t *bytes = memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

bool blue_redjubjub_nonce_from_entropy(uint8_t scalar[32],
    const uint8_t entropy[80], const uint8_t vkbar[32],
    const uint8_t transaction_digest[32]) {
    static const uint8_t personal[16] = {
        'Z','c','a','s','h','_','R','e','d','J','u','b','j','u','b','H'
    };
    if (!scalar || !entropy || !vkbar || !transaction_digest) return false;
    uint8_t entropy_nonzero = 0;
    for (unsigned i = 0; i < 80; ++i) entropy_nonzero |= entropy[i];
    if (!entropy_nonzero) return false;
    struct blake2b_ctx hash = {0};
    uint8_t digest[64] = {0}, candidate[32] = {0};
    bool success = blake2b_init_salt_personal(&hash, sizeof digest,
        NULL, 0, NULL, personal) == 0;
    if (success) success = blake2b_update(&hash, entropy, 80) == 0;
    if (success) success = blake2b_update(&hash, vkbar, 32) == 0;
    if (success) success = blake2b_update(&hash,
        transaction_digest, 32) == 0;
    if (success) success = blake2b_final(&hash, digest,
        sizeof digest) == 0;
    if (success) {
        jubjub_to_scalar(digest, candidate);
        uint8_t scalar_nonzero = 0;
        for (unsigned i = 0; i < sizeof candidate; ++i)
            scalar_nonzero |= candidate[i];
        success = scalar_nonzero != 0;
    }
    if (success) memcpy(scalar, candidate, sizeof candidate);
    wipe(candidate, sizeof candidate);
    wipe(digest, sizeof digest);
    wipe(&hash, sizeof hash);
    return success;
}
