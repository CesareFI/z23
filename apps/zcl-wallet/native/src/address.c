/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <stdbool.h>
#include <string.h>

static bool supported_network(zcl_network network)
{
    return network == ZCL_MAINNET || network == ZCL_TESTNET;
}

static uint16_t p2pkh_prefix(zcl_network network)
{
    return network == ZCL_MAINNET ? UINT16_C(0x1cb8) : UINT16_C(0x1d25);
}

static uint16_t p2sh_prefix(zcl_network network)
{
    return network == ZCL_MAINNET ? UINT16_C(0x1cbd) : UINT16_C(0x1cba);
}

zcl_status zcl_address_parse(const uint8_t *text, size_t text_len,
                             zcl_network network, zcl_address *address)
{
    if (text == NULL || address == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!supported_network(network))
        return ZCL_UNSUPPORTED;
    if (text_len != 35)
        return ZCL_INVALID_ENCODING;
    uint8_t payload[22] = {0};
    size_t length = 0;
    zcl_status status = zcl_base58check_decode(text, text_len, payload, sizeof(payload), &length);
    if (status != ZCL_OK)
        return status;
    if (length != sizeof(payload))
        return ZCL_INVALID_ENCODING;
    uint16_t prefix = (uint16_t)((uint16_t)payload[0] << 8) | payload[1];
    zcl_address temporary = {0};
    temporary.network = network;
    if (prefix == p2pkh_prefix(network))
        temporary.kind = ZCL_P2PKH;
    else if (prefix == p2sh_prefix(network))
        temporary.kind = ZCL_P2SH;
    else
        return ZCL_UNSUPPORTED;
    memcpy(temporary.hash, payload + 2, sizeof(temporary.hash));
    *address = temporary;
    return ZCL_OK;
}

zcl_status zcl_address_from_hash(const uint8_t *hash, size_t hash_len,
                                 zcl_network network, uint8_t *text,
                                 size_t text_capacity, size_t *text_len)
{
    if (hash == NULL || text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!supported_network(network))
        return ZCL_UNSUPPORTED;
    if (hash_len != 20)
        return ZCL_INVALID_ARGUMENT;
    uint16_t prefix = p2pkh_prefix(network);
    uint8_t payload[22] = {0};
    payload[0] = (uint8_t)(prefix >> 8);
    payload[1] = (uint8_t)(prefix & UINT16_C(255));
    memcpy(payload + 2, hash, hash_len);
    return zcl_base58check_encode(payload, sizeof(payload), text, text_capacity, text_len);
}

zcl_status zcl_address_script(const zcl_address *address, uint8_t *script,
                              size_t script_capacity, size_t *script_len)
{
    if (address == NULL || script == NULL || script_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!supported_network(address->network))
        return ZCL_UNSUPPORTED;
    uint8_t temporary[25] = {0};
    size_t length;
    if (address->kind == ZCL_P2PKH) {
        temporary[0] = 0x76;
        temporary[1] = 0xa9;
        temporary[2] = 0x14;
        memcpy(temporary + 3, address->hash, sizeof(address->hash));
        temporary[23] = 0x88;
        temporary[24] = 0xac;
        length = 25;
    } else if (address->kind == ZCL_P2SH) {
        temporary[0] = 0xa9;
        temporary[1] = 0x14;
        memcpy(temporary + 2, address->hash, sizeof(address->hash));
        temporary[22] = 0x87;
        length = 23;
    } else {
        return ZCL_UNSUPPORTED;
    }
    if (script_capacity < length)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(script, temporary, length);
    *script_len = length;
    return ZCL_OK;
}
