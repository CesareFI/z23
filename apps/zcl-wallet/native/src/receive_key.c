/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "bip32_internal.h"
#include "mnemonic_words.h"

#include <mbedtls/ripemd160.h>
#include <mbedtls/sha256.h>

static zcl_status validate_receive(const uint8_t *entropy, size_t entropy_len,
                                   zcl_network network, uint32_t index,
                                   const uint8_t *address, size_t capacity, const size_t *length)
{
    if (entropy == NULL || address == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!zcl_entropy_length_valid(entropy_len) || index >= UINT32_C(0x80000000))
        return ZCL_OUT_OF_RANGE;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET)
        return ZCL_UNSUPPORTED;
    return capacity < 35 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
}

zcl_status zcl_entropy_seed(const uint8_t *entropy, size_t entropy_len,
                             uint8_t *seed, size_t seed_capacity)
{
    uint8_t text[215] = {0};
    static const uint8_t empty[] = {0};
    size_t length = 0;
    zcl_status status = zcl_mnemonic_encode(entropy, entropy_len, text, sizeof(text), &length);
    if (status == ZCL_OK)
        status = zcl_mnemonic_seed(text, length, empty, 0, seed, seed_capacity);
    zcl_secure_zero(text, sizeof(text));
    return status;
}

static zcl_status address_path(const uint8_t *seed, size_t seed_len,
                               zcl_network network, uint32_t chain, uint32_t index,
                               const secp256k1_context *context, zcl_extended_private *output)
{
    uint32_t coin = network == ZCL_MAINNET ? 147 : 1;
    uint32_t path[5] = {UINT32_C(0x8000002c), UINT32_C(0x80000000) | coin,
                        UINT32_C(0x80000000), chain, index};
    zcl_extended_private parent = {0}, child = {0};
    zcl_status status = zcl_bip32_master(seed, seed_len, &parent);
    if (status != ZCL_OK)
        goto cleanup;
    for (size_t level = 0; level < sizeof(path) / sizeof(path[0]); ++level) {
        status = zcl_bip32_step(&parent, path[level], context, &child);
        if (status != ZCL_OK)
            goto cleanup;
        parent = child;
        zcl_secure_zero(&child, sizeof(child));
    }
    *output = parent;
cleanup:
    zcl_secure_zero(&parent, sizeof(parent));
    zcl_secure_zero(&child, sizeof(child));
    return status;
}

static zcl_status encode_public_address(const uint8_t *key, size_t key_len,
                                        zcl_network network, uint8_t *address,
                                        size_t capacity, size_t *length)
{
    if (key_len != 33)
        return ZCL_INVALID_ARGUMENT;
    uint8_t hash[32] = {0}, identifier[20] = {0};
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(key, key_len, hash, 0) != 0)
        goto cleanup;
    if (mbedtls_ripemd160(hash, sizeof(hash), identifier) != 0)
        goto cleanup;
    status = zcl_address_from_hash(identifier, sizeof(identifier), network, address, capacity, length);
cleanup:
    zcl_secure_zero(hash, sizeof(hash));
    zcl_secure_zero(identifier, sizeof(identifier));
    return status;
}

static zcl_status seed_address_bounds(const uint8_t *seed, size_t seed_len,
    zcl_network network, uint32_t chain, uint32_t index)
{
    if (seed == NULL) return ZCL_INVALID_ARGUMENT;
    if (seed_len != ZCL_SEED_BYTES || chain > 1 || index >= UINT32_C(0x80000000)) return ZCL_OUT_OF_RANGE;
    return network == ZCL_MAINNET || network == ZCL_TESTNET ? ZCL_OK : ZCL_UNSUPPORTED;
}

zcl_status zcl_seed_private(const uint8_t *seed, size_t seed_len, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context,
    zcl_extended_private *output)
{
    if (context == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = seed_address_bounds(seed, seed_len, network, chain, index);
    if (status != ZCL_OK) return status;
    return address_path(seed, seed_len, network, chain, index, context, output);
}

/* Consumes this invocation's derived key before public hashing/encoding. */
static zcl_status encode_owned_key(zcl_extended_private *key,
    const secp256k1_context *context, zcl_network network,
    uint8_t *address, size_t capacity, size_t *length)
{
    uint8_t public_key[33] = {0};
    zcl_status status = zcl_ec_public(context, key->secret, sizeof(key->secret), public_key, sizeof(public_key));
    /* Hashing and address encoding consume only the public key. Retire both
     * the private scalar and chain code before entering those providers. */
    zcl_secure_zero(key, sizeof(*key));
    if (status == ZCL_OK)
        status = encode_public_address(public_key, sizeof(public_key), network, address, capacity, length);
    zcl_secure_zero(public_key, sizeof(public_key));
    return status;
}

zcl_status zcl_seed_address(const uint8_t *seed, size_t seed_len, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context,
    uint8_t *address, size_t capacity, size_t *length)
{
    if (context == NULL || address == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_status status = seed_address_bounds(seed, seed_len, network, chain, index);
    if (status != ZCL_OK) return status;
    if (capacity < 35) return ZCL_BUFFER_TOO_SMALL;
    zcl_extended_private key = {0};
    status = zcl_seed_private(seed, seed_len, network, chain, index, context, &key);
    if (status == ZCL_OK)
        status = encode_owned_key(&key, context, network, address, capacity, length);
    zcl_secure_zero(&key, sizeof(key));
    return status;
}

/* Only the two public wrappers select chain: fixed external0 or internal1. */
static zcl_status address_from_entropy(const uint8_t *entropy, size_t entropy_len,
                                    zcl_network network, uint32_t chain, uint32_t index,
                                    const uint8_t *blinding, size_t blinding_len,
                                    uint8_t *address, size_t address_capacity, size_t *address_len)
{
    zcl_status status = validate_receive(entropy, entropy_len, network, index,
                                         address, address_capacity, address_len);
    if (status != ZCL_OK)
        return status;
    uint8_t seed[64] = {0};
    zcl_extended_private key = {0};
    zcl_ec_context context = {0};
    status = zcl_ec_begin(&context, blinding, blinding_len);
    if (status == ZCL_OK)
        status = zcl_entropy_seed(entropy, entropy_len, seed, sizeof(seed));
    if (status == ZCL_OK)
        status = zcl_seed_private(seed, sizeof(seed), network, chain, index, context.handle, &key);
    /* This seed is owned here and no longer needed by the final public-key
     * provider, hashes or encoding. Borrowed seed APIs retain their ownership. */
    zcl_secure_zero(seed, sizeof(seed));
    if (status == ZCL_OK)
        status = encode_owned_key(&key, context.handle, network, address, address_capacity, address_len);
    zcl_secure_zero(&key, sizeof(key));
    zcl_ec_end(&context);
    return status;
}

zcl_status zcl_receive_from_entropy(const uint8_t *entropy, size_t entropy_len,
                                    zcl_network network, uint32_t index,
                                    const uint8_t *blinding, size_t blinding_len,
                                    uint8_t *address, size_t address_capacity, size_t *address_len)
{
    return address_from_entropy(entropy, entropy_len, network, 0, index, blinding,
        blinding_len, address, address_capacity, address_len);
}

zcl_status zcl_change_from_entropy(const uint8_t *entropy, size_t entropy_len,
                                   zcl_network network, uint32_t index,
                                   const uint8_t *blinding, size_t blinding_len,
                                   uint8_t *address, size_t address_capacity, size_t *address_len)
{
    return address_from_entropy(entropy, entropy_len, network, 1, index, blinding,
        blinding_len, address, address_capacity, address_len);
}
