/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "blue_wallet_signer_device.h"

#include <string.h>

typedef struct {
    uint8_t raw[32], chain[32];
    cx_ecfp_private_key_t key;
} private_material;

static private_material secret;
static cx_ecfp_public_key_t public_point;

static void wipe(void *memory, size_t length) {
    volatile uint8_t *bytes = memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

void blue_wallet_signer_wipe(void) {
    wipe(&secret, sizeof secret);
    wipe(&public_point, sizeof public_point);
}

static bool request_valid(uint8_t path, const uint8_t *digest,
    const uint8_t *public_key, const uint8_t *signature) {
    return digest && public_key && signature &&
        (path == BLUE_PAYMENT_INPUT_EXTERNAL ||
         path == BLUE_PAYMENT_INPUT_INTERNAL) &&
        os_global_pin_is_validated();
}

static bool derive_pair(uint8_t path) {
    const unsigned int bip32_path[5] = {
        0x8000002c, 0x80000093, 0x80000000,
        path == BLUE_PAYMENT_INPUT_EXTERNAL ? 0u : 1u, 0u
    };
    os_perso_derive_node_bip32(CX_CURVE_256K1, bip32_path, 5,
        secret.raw, secret.chain);
    (void)cx_ecfp_init_private_key(CX_CURVE_256K1, secret.raw, 32,
        &secret.key);
    wipe(secret.raw, sizeof secret.raw);
    wipe(secret.chain, sizeof secret.chain);
    return secret.key.curve == CX_CURVE_256K1 &&
        secret.key.d_len == 32 &&
        cx_ecfp_generate_pair(CX_CURVE_256K1, &public_point,
                              &secret.key, 1) == 0 &&
        public_point.W_len == 65 && public_point.W[0] == 4;
}

bool blue_wallet_sign_digest(void *context, uint8_t path,
    const uint8_t digest[32], uint8_t public_key[33],
    uint8_t signature[BLUE_ECDSA_DER_MAX], size_t *signature_length) {
    (void)context;
    blue_wallet_signer_wipe();
    if (public_key) memset(public_key, 0, 33);
    if (signature) memset(signature, 0, BLUE_ECDSA_DER_MAX);
    if (!signature_length) return false;
    *signature_length = 0;
    if (!request_valid(path, digest, public_key, signature)) return false;
    bool valid = derive_pair(path);
    if (valid) {
        public_key[0] = (uint8_t)(2u | (public_point.W[64] & 1u));
        memcpy(public_key + 1, public_point.W + 1, 32);
        unsigned int info = 0;
        int count = (cx_ecdsa_sign)(&secret.key, CX_RND_RFC6979,
            CX_SHA256, digest, 32, signature, BLUE_ECDSA_DER_MAX, &info);
        valid = count >= 8 && count <= BLUE_ECDSA_DER_MAX;
        if (valid) *signature_length = (size_t)count;
    }
    blue_wallet_signer_wipe();
    if (!valid) {
        memset(public_key, 0, 33);
        memset(signature, 0, BLUE_ECDSA_DER_MAX);
    }
    return valid;
}
