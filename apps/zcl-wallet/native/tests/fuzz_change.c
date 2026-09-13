/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void arbitrary_arguments(const uint8_t *entropy, size_t entropy_len, uint32_t index,
                                 zcl_network network, size_t blind_len, size_t capacity)
{
    uint8_t blinding[32] = {1}, output[37], before[37];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    size_t length = SIZE_MAX;
    const zcl_status status = zcl_change_from_entropy(entropy, entropy_len, network, index,
        blinding, blind_len, output + 1, capacity, &length);
    if (status != ZCL_OK) {
        if (length != SIZE_MAX || memcmp(output, before, sizeof(output)) != 0) abort();
        return;
    }
    if (length != 35 || capacity < length || output[0] != 0xa5 || output[36] != 0xa5) abort();
    zcl_address address;
    if (zcl_address_parse(output + 1, length, network, &address) != ZCL_OK || address.kind != ZCL_P2PKH) abort();
}

static void valid_derivation(const uint8_t *entropy, size_t entropy_len, zcl_network network, uint32_t index)
{
    uint8_t blind1[32] = {1}, blind2[32] = {2}, first[35], second[35], receive[35];
    size_t first_len = 0, second_len = 0, receive_len = 0;
    if (zcl_change_from_entropy(entropy, entropy_len, network, index, blind1, 32,
        first, sizeof(first), &first_len) != ZCL_OK) abort();
    if (zcl_change_from_entropy(entropy, entropy_len, network, index, blind2, 32,
        second, sizeof(second), &second_len) != ZCL_OK) abort();
    if (zcl_receive_from_entropy(entropy, entropy_len, network, index, blind1, 32,
        receive, sizeof(receive), &receive_len) != ZCL_OK) abort();
    if (first_len != 35 || second_len != 35 || receive_len != 35) abort();
    if (memcmp(first, second, 35) != 0 || memcmp(first, receive, 35) == 0) abort();
    zcl_address address;
    if (zcl_address_parse(first, first_len, network, &address) != ZCL_OK) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 64) return 0;
    uint8_t entropy[32] = {0}, saved[32];
    memcpy(entropy, data, size < sizeof(entropy) ? size : sizeof(entropy));
    memcpy(saved, entropy, sizeof(saved));
    uint32_t index = 0;
    for (size_t i = 1; i < size && i <= 4; ++i) index |= (uint32_t)data[i] << (8 * (i - 1));
    arbitrary_arguments(entropy, data[0] % 34, index, (zcl_network)(data[0] % 3),
        data[size - 1] % 34, data[size - 1] % 36);
    /* All entropy/blinding is public test input. Always exercise the full
     * derivation too, so the campaign cannot optimize into argument refusal. */
    valid_derivation(entropy, 16 + (size_t)(data[0] % 5) * 4,
        data[0] % 2 == 0 ? ZCL_MAINNET : ZCL_TESTNET, index & UINT32_C(0x7fffffff));
    if (memcmp(entropy, saved, sizeof(entropy)) != 0) abort();
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(saved, sizeof(saved));
    return 0;
}
