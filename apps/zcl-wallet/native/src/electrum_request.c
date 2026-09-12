/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "mbedtls/sha256.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

zcl_status zcl_electrum_script_hash(const uint8_t *text, size_t text_len,
                                     zcl_network network, uint8_t *output, size_t capacity)
{
    if (output == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity < 64) return ZCL_BUFFER_TOO_SMALL;
    zcl_address address = {0};
    zcl_status status = zcl_address_parse(text, text_len, network, &address);
    if (status != ZCL_OK) return status;
    uint8_t script[25] = {0}, hash[32] = {0}, encoded[64];
    size_t script_len = 0;
    status = zcl_address_script(&address, script, sizeof(script), &script_len);
    if (status != ZCL_OK) return status;
    if (mbedtls_sha256(script, script_len, hash, 0) != 0) return ZCL_CRYPTO_FAILURE;
    static const uint8_t hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        const uint8_t byte = hash[31 - i];
        encoded[2 * i] = hex[byte >> 4];
        encoded[2 * i + 1] = hex[byte & 15];
    }
    memcpy(output, encoded, sizeof(encoded));
    return ZCL_OK;
}

static int public_request(zcl_electrum_method method, uint32_t id, char *output, size_t capacity)
{
    switch (method) {
    case ZCL_ELECTRUM_VERSION:
        return snprintf(output, capacity, "{\"id\":%" PRIu32 ",\"method\":\"server.version\",\"params\":[\"ZclassicAndroidDev\",\"1.2\"]}\n", id);
    case ZCL_ELECTRUM_FEATURES:
        return snprintf(output, capacity, "{\"id\":%" PRIu32 ",\"method\":\"server.features\",\"params\":[]}\n", id);
    case ZCL_ELECTRUM_GENESIS:
        return snprintf(output, capacity, "{\"id\":%" PRIu32 ",\"method\":\"blockchain.block.headers\",\"params\":[0,1]}\n", id);
    case ZCL_ELECTRUM_TIP:
        return snprintf(output, capacity, "{\"id\":%" PRIu32 ",\"method\":\"blockchain.headers.subscribe\",\"params\":[true]}\n", id);
    default:
        return -1;
    }
}

static bool request_arguments(uint32_t id, const uint8_t *text, const size_t *length)
{
    return text != NULL && length != NULL && id != 0;
}

zcl_status zcl_electrum_request(zcl_electrum_method method, uint32_t id,
                                const uint8_t *address, size_t address_len, zcl_network network,
                                uint8_t *text, size_t capacity, size_t *length)
{
    if (!request_arguments(id, text, length)) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    char encoded[ZCL_ELECTRUM_REQUEST_MAX];
    int written;
    if (method == ZCL_ELECTRUM_BALANCE) {
        uint8_t hash[64];
        const zcl_status status = zcl_electrum_script_hash(address, address_len, network, hash, sizeof(hash));
        if (status != ZCL_OK) return status;
        written = snprintf(encoded, sizeof(encoded), "{\"id\":%" PRIu32 ",\"method\":\"blockchain.scripthash.get_balance\",\"params\":[\"%.*s\"]}\n", id, 64, (const char *)hash);
    } else {
        written = public_request(method, id, encoded, sizeof(encoded));
    }
    if (written < 0) return ZCL_UNSUPPORTED;
    if ((size_t)written >= sizeof(encoded)) return ZCL_OUT_OF_RANGE;
    if ((size_t)written > capacity) return ZCL_BUFFER_TOO_SMALL;
    memcpy(text, encoded, (size_t)written);
    *length = (size_t)written;
    return ZCL_OK;
}
