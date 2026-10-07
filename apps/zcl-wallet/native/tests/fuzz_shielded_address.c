/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_shielded_address.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void check_success(const zcl_shielded_address *address, const uint8_t *text, size_t length)
{
    if (address->kind == ZCL_SPROUT_ADDRESS) {
        uint8_t payload[66] = {0x16, 0}, encoded[96] = {0};
        payload[1] = address->network == ZCL_MAINNET ? 0x9a : 0xb6;
        memcpy(payload + 2, address->bytes, sizeof(address->bytes));
        size_t written = 0;
        if (zcl_base58check_encode(payload, sizeof(payload), encoded, sizeof(encoded), &written) != ZCL_OK) abort();
        if (written != length || memcmp(encoded, text, length) != 0) abort();
    } else {
        if (address->kind != ZCL_SAPLING_ADDRESS) abort();
        for (size_t index = 43; index < sizeof(address->bytes); ++index) {
            if (address->bytes[index] != 0) abort();
        }
    }
}

static bool inspect(const uint8_t *data, size_t size, zcl_network network)
{
    struct { uint64_t before; zcl_shielded_address value; uint64_t after; } guarded, sentinel;
    memset(&sentinel, 0xa5, sizeof(sentinel));
    memcpy(&guarded, &sentinel, sizeof(guarded));
    const zcl_status status = zcl_shielded_address_parse(data, size, network, &guarded.value);
    if (guarded.before != sentinel.before || guarded.after != sentinel.after) abort();
    if (status != ZCL_OK) {
        if (memcmp(&guarded, &sentinel, sizeof(guarded)) != 0) abort();
        return false;
    }
    if (guarded.value.network != network) abort();
    check_success(&guarded.value, data, size);
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const bool mainnet = inspect(data, size, ZCL_MAINNET);
    const bool testnet = inspect(data, size, ZCL_TESTNET);
    if (mainnet && testnet) abort();
    if (inspect(data, size, (zcl_network)2)) abort();
    return 0;
}
