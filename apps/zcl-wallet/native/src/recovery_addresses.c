/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_recovery.h"
#include "bip32_internal.h"
#include "mnemonic_words.h"
#include <string.h>

static zcl_status range_valid(size_t entropy_len, uint32_t chain, uint32_t first, size_t count)
{
    if (!zcl_entropy_length_valid(entropy_len) || chain > 1) return ZCL_OUT_OF_RANGE;
    if (count == 0 || count > ZCL_RECOVERY_BATCH_MAX) return ZCL_OUT_OF_RANGE;
    const uint32_t limit = UINT32_C(0x80000000);
    if (first >= limit || count > (size_t)(limit - first)) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status derive_addresses(const uint8_t seed[64], const secp256k1_context *context,
    zcl_network network, uint32_t chain, uint32_t first, size_t count, uint8_t *candidate)
{
    for (size_t i = 0; i < count; ++i) {
        size_t length = 0;
        /* Admission bounds count<=16 and first+count<=2^31 before this loop. */
        zcl_status status = zcl_seed_address(seed, 64, network, chain, first + (uint32_t)i,
            context, candidate + i * ZCL_RECOVERY_ADDRESS_BYTES, ZCL_RECOVERY_ADDRESS_BYTES, &length);
        if (status != ZCL_OK) return status;
        if (length != ZCL_RECOVERY_ADDRESS_BYTES) return ZCL_CRYPTO_FAILURE;
    }
    return ZCL_OK;
}

zcl_status zcl_recovery_address_batch(const uint8_t *entropy, size_t entropy_len,
    zcl_network network, uint32_t chain, uint32_t first, size_t count,
    const uint8_t *blinding, size_t blinding_len, uint8_t *output, size_t capacity)
{
    if (entropy == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    zcl_status status = range_valid(entropy_len, chain, first, count);
    if (status != ZCL_OK) return status;
    const size_t bytes = count * ZCL_RECOVERY_ADDRESS_BYTES; /* count<=16. */
    if (capacity < bytes) return ZCL_BUFFER_TOO_SMALL;
    uint8_t seed[64] = {0}, candidate[ZCL_RECOVERY_BATCH_MAX * ZCL_RECOVERY_ADDRESS_BYTES] = {0};
    zcl_ec_context context = {0};
    status = zcl_ec_begin(&context, blinding, blinding_len);
    if (status == ZCL_OK) status = zcl_entropy_seed(entropy, entropy_len, seed, sizeof(seed));
    if (status == ZCL_OK)
        status = derive_addresses(seed, context.handle, network, chain, first, count, candidate);
    zcl_secure_zero(seed, sizeof(seed));
    zcl_ec_end(&context);
    if (status == ZCL_OK) memcpy(output, candidate, bytes);
    zcl_secure_zero(candidate, sizeof(candidate));
    return status;
}
