/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include <string.h>

static zcl_status change_arguments(const uint8_t *header, const uint8_t *entropy,
                                   uint32_t index, const uint8_t *blinding,
                                   size_t blinding_len, const uint8_t *address, size_t capacity)
{
    if (header == NULL || entropy == NULL || blinding == NULL || address == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (blinding_len != ZCL_WALLET_CHANGE_BLINDING_BYTES)
        return ZCL_INVALID_ARGUMENT;
    if (index >= UINT32_C(0x80000000))
        return ZCL_OUT_OF_RANGE;
    return capacity < 35 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
}

zcl_status zcl_wallet_recovered_change(const uint8_t *header, size_t header_len,
                                      const uint8_t *entropy, size_t entropy_len,
                                      uint32_t index, const uint8_t *blinding, size_t blinding_len,
                                      uint8_t *address, size_t capacity)
{
    zcl_status status = change_arguments(header, entropy, index, blinding, blinding_len, address, capacity);
    if (status != ZCL_OK) return status;
    zcl_wallet_info info = {0};
    status = zcl_wallet_header_parse(header, header_len, &info);
    if (status != ZCL_OK) return status;
    uint8_t anchor[35] = {0}, candidate[35] = {0};
    status = zcl_wallet_recovered_address(header, header_len, entropy, entropy_len,
        blinding, 32, anchor, sizeof(anchor));
    if (status != ZCL_OK) return status;
    size_t length = 0;
    /* The exact64-byte blinding span was checked before either derivation. */
    status = zcl_change_from_entropy(entropy, entropy_len, info.network, index,
        blinding + 32, 32, candidate, sizeof(candidate), &length);
    if (status != ZCL_OK) return status;
    if (length != sizeof(candidate)) return ZCL_CRYPTO_FAILURE;
    memcpy(address, candidate, sizeof(candidate));
    return ZCL_OK;
}
