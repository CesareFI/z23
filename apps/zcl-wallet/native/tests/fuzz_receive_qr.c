/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_qr.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void exercise(const uint8_t *data, size_t size, zcl_network network, size_t capacity)
{
    uint8_t modules[ZCL_RECEIVE_QR_MODULES_MAX + 2];
    memset(modules, 0xa5, sizeof(modules));
    size_t side = SIZE_MAX;
    const zcl_status status = zcl_receive_qr(data, size, network, modules + 1, capacity, &side);
    if (modules[0] != 0xa5 || modules[sizeof(modules) - 1] != 0xa5)
        abort();
    if (status != ZCL_OK) {
        if (side != SIZE_MAX) abort();
        for (size_t i = 0; i < sizeof(modules); ++i)
            if (modules[i] != 0xa5) abort();
        return;
    }
    if (side != 41 || side * side > capacity) abort();
    for (size_t i = 1; i <= side * side; ++i)
        if (modules[i] > 1) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    exercise(data, size, ZCL_MAINNET, ZCL_RECEIVE_QR_MODULES_MAX);
    if (size < 20) return 0;
    uint8_t text[35] = {0};
    size_t length = 0;
    const zcl_network network = (data[0] & 1u) != 0 ? ZCL_MAINNET : ZCL_TESTNET;
    if (zcl_address_from_hash(data, 20, network, text, sizeof(text), &length) != ZCL_OK)
        abort();
    exercise(text, length, network, ZCL_RECEIVE_QR_MODULES_MAX);
    exercise(text, length, network, (size_t)data[1] * 5);
    return 0;
}
