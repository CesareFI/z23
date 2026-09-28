/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_redjubjub_response.h"

#include "blue_fs_ct.h"
#include "blue_mod256.h"

#include <stddef.h>

static bool nonzero(const uint8_t bytes[32]) {
    uint8_t combined = 0;
    for (unsigned i = 0; i < 32; ++i) combined |= bytes[i];
    return combined != 0;
}

bool blue_redjubjub_response(uint8_t response[32],
    const uint8_t nonce[32], const uint8_t challenge[32],
    const uint8_t secret[32]) {
    if (!response || !nonce || !challenge || !secret) return false;
    struct fs r = {0}, c = {0}, sk = {0}, product = {0}, sum = {0};
    bool valid = blue_fs_from_bytes_canonical(&r, nonce) &&
        nonzero(nonce) &&
        blue_fs_from_bytes_canonical(&c, challenge) &&
        blue_fs_from_bytes_canonical(&sk, secret) && nonzero(secret);
    if (valid) {
        blue_fs_mul_ct(&product, &c, &sk);
        blue_fs_add_ct(&sum, &r, &product);
        blue_fs_to_bytes(response, &sum);
    }
    blue_mod256_wipe(&r, sizeof r);
    blue_mod256_wipe(&c, sizeof c);
    blue_mod256_wipe(&sk, sizeof sk);
    blue_mod256_wipe(&product, sizeof product);
    blue_mod256_wipe(&sum, sizeof sum);
    return valid;
}
