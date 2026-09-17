/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "bip32_internal.h"
#include "secret_hash.h"

#include <string.h>

zcl_status zcl_bip32_master(const uint8_t *seed, size_t seed_len, zcl_extended_private *output)
{
    if (seed == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (seed_len < 16 || seed_len > 64)
        return ZCL_OUT_OF_RANGE;
    static const uint8_t key[] = "Bitcoin seed";
    uint8_t digest[64] = {0};
    zcl_extended_private result = {0};
    zcl_status status = zcl_hmac_sha512(key, sizeof(key) - 1, seed, seed_len, digest, sizeof(digest));
    if (status != ZCL_OK)
        goto cleanup;
    secp256k1_selftest();
    if (secp256k1_ec_seckey_verify(secp256k1_context_static, digest) != 1) {
        status = ZCL_CRYPTO_FAILURE;
        goto cleanup;
    }
    memcpy(result.secret, digest, sizeof(result.secret));
    memcpy(result.chain_code, digest + 32, sizeof(result.chain_code));
    *output = result;
cleanup:
    zcl_secure_zero(digest, sizeof(digest));
    zcl_secure_zero(&result, sizeof(result));
    return status;
}

static zcl_status child_data(const zcl_extended_private *parent, uint32_t index,
                             const secp256k1_context *context,
                             uint8_t *data, size_t capacity)
{
    if (capacity < 37)
        return ZCL_BUFFER_TOO_SMALL;
    if (index >= UINT32_C(0x80000000)) {
        data[0] = 0;
        memcpy(data + 1, parent->secret, sizeof(parent->secret));
    } else {
        zcl_status status = zcl_ec_public(context, parent->secret, sizeof(parent->secret), data, capacity);
        if (status != ZCL_OK)
            return status;
    }
    for (size_t i = 0; i < 4; ++i)
        data[33 + i] = (uint8_t)(index >> (24U - (unsigned)i * 8U));
    return ZCL_OK;
}

static zcl_status derive_child(const zcl_extended_private *parent, uint32_t index,
                               const secp256k1_context *context, zcl_extended_private *output)
{
    uint8_t data[37] = {0}, digest[64] = {0};
    zcl_extended_private result = {0};
    zcl_status status = child_data(parent, index, context, data, sizeof(data));
    if (status == ZCL_OK)
        status = zcl_hmac_sha512(parent->chain_code, sizeof(parent->chain_code),
                                 data, sizeof(data), digest, sizeof(digest));
    /* Hardened input includes a private-key copy. HMAC is its last consumer;
     * scalar tweaking and publication need only the digest and parent. */
    zcl_secure_zero(data, sizeof(data));
    if (status != ZCL_OK)
        goto cleanup;
    memcpy(result.secret, parent->secret, sizeof(result.secret));
    if (secp256k1_ec_seckey_tweak_add(context, result.secret, digest) != 1) {
        status = ZCL_INVALID_CHILD;
        goto cleanup;
    }
    memcpy(result.chain_code, digest + 32, sizeof(result.chain_code));
    *output = result;
cleanup:
    zcl_secure_zero(digest, sizeof(digest));
    zcl_secure_zero(&result, sizeof(result));
    return status;
}

zcl_status zcl_bip32_child(const zcl_extended_private *parent, uint32_t index,
                          const uint8_t *blinding, size_t blinding_len,
                          zcl_extended_private *output)
{
    if (parent == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    zcl_ec_context context = {0};
    zcl_status status = zcl_ec_begin(&context, blinding, blinding_len);
    if (status != ZCL_OK)
        goto cleanup;
    status = zcl_bip32_step(parent, index, context.handle, output);
cleanup:
    zcl_ec_end(&context);
    return status;
}

zcl_status zcl_bip32_step(const zcl_extended_private *parent, uint32_t index,
                          const secp256k1_context *context, zcl_extended_private *output)
{
    if (parent == NULL || context == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (secp256k1_ec_seckey_verify(context, parent->secret) != 1)
        return ZCL_INVALID_ARGUMENT;
    return derive_child(parent, index, context, output);
}
