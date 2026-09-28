/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_sapling_spend_auth.h"

#include "blue_fs_ct.h"
#include "blue_jubjub_encode.h"
#include "blue_jubjub_lowmem.h"
#include "blue_mod256.h"
#include "blue_redjubjub_sign_isolated.h"
#include "blue_sapling_generators.h"

#include <stddef.h>

static bool scalar_nonzero(const uint8_t scalar[32]) {
    uint8_t combined = 0;
    for (unsigned i = 0; i < 32; ++i) combined |= scalar[i];
    return combined != 0;
}

static bool matches_rk(const uint8_t actual[32],
    const uint8_t expected[32]) {
    uint8_t difference = 0;
    for (unsigned i = 0; i < 32; ++i)
        difference |= actual[i] ^ expected[i];
    return difference == 0;
}

static bool derive_rsk(uint8_t rsk[32],
    blue_sapling_spend_workspace *workspace, const uint8_t ask[32],
    const uint8_t ar[32]) {
    bool valid = blue_fs_from_bytes_canonical(&workspace->scalars.ask, ask) &&
        blue_fs_from_bytes_canonical(&workspace->scalars.ar, ar);
    if (valid) {
        blue_fs_add_ct(&workspace->scalars.rsk,
            &workspace->scalars.ask, &workspace->scalars.ar);
        blue_fs_to_bytes(rsk, &workspace->scalars.rsk);
        valid = scalar_nonzero(rsk);
    }
    return valid;
}

bool blue_sapling_spend_auth_sign(uint8_t signature[64],
    blue_sapling_spend_workspace *workspace, const uint8_t ask[32],
    const uint8_t ar[32], const uint8_t expected_rk[32],
    const uint8_t entropy[80], const uint8_t digest[32]) {
    if (workspace) blue_mod256_wipe(workspace, sizeof *workspace);
    if (!signature) return false;
    blue_mod256_wipe(signature, 64);
    if (!workspace) return false;
    if (!ask || !ar || !expected_rk || !entropy || !digest) return false;
    uint8_t rsk[32] = {0};
    bool valid = derive_rsk(rsk, workspace, ask, ar);
    blue_mod256_wipe(workspace, sizeof *workspace);
    if (valid) valid = blue_jubjub_scalar_mul_lowmem(&workspace->point,
        &blue_spending_key_generator, rsk) &&
        blue_jubjub_encode(signature, &workspace->point) &&
        matches_rk(signature, expected_rk);
    if (valid) valid = blue_redjubjub_sign_isolated(signature,
        rsk, entropy, digest);
    if (!valid) blue_mod256_wipe(signature, 64);
    blue_mod256_wipe(workspace, sizeof *workspace);
    blue_mod256_wipe(rsk, sizeof rsk);
    return valid;
}
