/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include "mnemonic_words.h"

#include <string.h>

static zcl_status check_fixed_fields(const uint8_t *header, size_t length)
{
    if (header == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (length != ZCL_WALLET_HEADER_BYTES)
        return ZCL_OUT_OF_RANGE;
    if (memcmp(header, "ZCLW", 4) != 0 || header[4] != 1 || header[6] != 1)
        return ZCL_UNSUPPORTED;
    static const uint8_t zero_index[4] = {0};
    if (memcmp(header + 8, zero_index, sizeof(zero_index)) != 0 || header[79] != 0)
        return ZCL_UNSUPPORTED;
    return ZCL_OK;
}

static zcl_status check_identity(const uint8_t *header, size_t length, zcl_wallet_info *info)
{
    if (length != ZCL_WALLET_HEADER_BYTES)
        return ZCL_OUT_OF_RANGE;
    uint8_t genesis[32] = {0};
    zcl_address address = {0};
    zcl_network network = (zcl_network)header[5];
    zcl_status status = zcl_network_genesis(network, genesis, sizeof(genesis));
    if (status != ZCL_OK)
        return status;
    if (memcmp(header + 12, genesis, sizeof(genesis)) != 0)
        return ZCL_INVALID_ENCODING;
    status = zcl_address_parse(header + 44, 35, network, &address);
    if (status != ZCL_OK)
        return status;
    if (address.kind != ZCL_P2PKH)
        return ZCL_UNSUPPORTED;
    info->network = network;
    memcpy(info->address, header + 44, sizeof(info->address));
    return ZCL_OK;
}

zcl_status zcl_wallet_header_parse(const uint8_t *header, size_t header_len, zcl_wallet_info *info)
{
    if (info == NULL)
        return ZCL_INVALID_ARGUMENT;
    zcl_status status = check_fixed_fields(header, header_len);
    if (status != ZCL_OK)
        return status;
    if (!zcl_entropy_length_valid(header[7]))
        return ZCL_INVALID_ENCODING;
    zcl_wallet_info result = {0};
    status = check_identity(header, header_len, &result);
    if (status != ZCL_OK)
        return status;
    result.entropy_len = header[7];
    *info = result;
    return ZCL_OK;
}

zcl_status zcl_wallet_header_create(const uint8_t *entropy, size_t entropy_len,
                                    zcl_network network, const uint8_t *blinding, size_t blinding_len,
                                    uint8_t *header, size_t header_capacity)
{
    if (header == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (header_capacity < ZCL_WALLET_HEADER_BYTES)
        return ZCL_BUFFER_TOO_SMALL;
    uint8_t result[80] = {'Z', 'C', 'L', 'W', 1, 0, 1};
    size_t address_len = 0;
    zcl_status status = zcl_network_genesis(network, result + 12, 32);
    if (status != ZCL_OK)
        return status;
    status = zcl_receive_from_entropy(entropy, entropy_len, network, 0, blinding, blinding_len,
                                      result + 44, 35, &address_len);
    if (status != ZCL_OK)
        return status;
    if (address_len != 35)
        return ZCL_CRYPTO_FAILURE;
    result[5] = (uint8_t)network;
    result[7] = (uint8_t)entropy_len;
    memcpy(header, result, sizeof(result));
    return ZCL_OK;
}

zcl_status zcl_wallet_recovered_address(const uint8_t *header, size_t header_len,
                                       const uint8_t *entropy, size_t entropy_len,
                                       const uint8_t *blinding, size_t blinding_len,
                                       uint8_t *address, size_t capacity)
{
    if (address == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (capacity < 35)
        return ZCL_BUFFER_TOO_SMALL;
    zcl_wallet_info info = {0};
    zcl_status status = zcl_wallet_header_parse(header, header_len, &info);
    if (status != ZCL_OK)
        return status;
    if (entropy_len != info.entropy_len)
        return ZCL_INVALID_ENCODING;
    uint8_t derived[35] = {0};
    size_t length = 0;
    status = zcl_receive_from_entropy(entropy, entropy_len, info.network, 0, blinding, blinding_len,
                                      derived, sizeof(derived), &length);
    if (status != ZCL_OK)
        return status;
    if (length != sizeof(derived) || memcmp(info.address, derived, sizeof(derived)) != 0)
        return ZCL_INVALID_ENCODING;
    memcpy(address, derived, sizeof(derived));
    return ZCL_OK;
}
