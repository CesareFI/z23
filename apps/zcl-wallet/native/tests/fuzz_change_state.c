/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_change_state.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void checked_decode(const uint8_t *header, const uint8_t *entropy, size_t entropy_len,
                            const uint8_t *blinding, const uint8_t *record, size_t record_len)
{
    uint32_t result = UINT32_MAX;
    const zcl_status status = zcl_change_state_decode(header, 80, entropy, entropy_len,
        blinding, 32, record, record_len, &result);
    if (status != ZCL_OK) { if (result != UINT32_MAX) abort(); return; }
    if (result > ZCL_CHANGE_INDEX_EXHAUSTED || record_len != 80) abort();
    uint8_t repeated[80];
    if (zcl_change_state_encode(header, 80, entropy, entropy_len, blinding, 32,
        result, repeated, sizeof(repeated)) != ZCL_OK) abort();
    if (memcmp(repeated, record, sizeof(repeated)) != 0) abort();
}

static void check_capacity(const uint8_t *header, const uint8_t *entropy,
    const uint8_t *blinding, uint8_t entropy_selector, uint8_t capacity_selector, uint32_t value)
{
    uint8_t output[82], before[82];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    const size_t capacity = capacity_selector % 81;
    const zcl_status status = zcl_change_state_encode(header, 80, entropy, entropy_selector % 34,
        blinding, 32, value, output + 1, capacity);
    if (status == ZCL_OK) {
        if (capacity != 80 || output[0] != 0xa5 || output[81] != 0xa5) abort();
        checked_decode(header, entropy, entropy_selector % 34, blinding, output + 1, 80);
    } else if (memcmp(output, before, sizeof(output)) != 0) abort();
}

static uint32_t read_index(const uint8_t bytes[static 4])
{
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) value |= (uint32_t)bytes[i] << (8 * i);
    return value;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 6 || size > 118) return 0;
    uint8_t entropy[32] = {0}, saved[32], header[80], record[80], changed[80], blinding[32] = {1};
    const size_t entropy_len = 16 + (size_t)(data[0] % 5) * 4;
    for (size_t i = 0; i < entropy_len; ++i) entropy[i] = data[i % size];
    memcpy(saved, entropy, sizeof(saved));
    const zcl_network network = (data[0] & 1) != 0 ? ZCL_TESTNET : ZCL_MAINNET;
    const uint32_t value = read_index(data + 1);
    const uint32_t valid = (data[0] & 0x80) != 0 ? ZCL_CHANGE_INDEX_EXHAUSTED : value & UINT32_C(0x7fffffff);
    if (zcl_wallet_header_create(entropy, entropy_len, network, blinding, 32, header, 80) != ZCL_OK) abort();
    if (zcl_change_state_encode(header, 80, entropy, entropy_len, blinding, 32, valid, record, 80) != ZCL_OK) abort();
    uint32_t index = UINT32_MAX;
    if (zcl_change_state_decode(header, 80, entropy, entropy_len, blinding, 32, record, 80, &index) != ZCL_OK || index != valid) abort();
    memcpy(changed, record, sizeof(changed));
    changed[data[1] % 80] ^= (uint8_t)(1U << (data[2] % 8));
    index = UINT32_MAX;
    if (zcl_change_state_decode(header, 80, entropy, entropy_len, blinding, 32, changed, 80, &index) == ZCL_OK) abort();
    if (index != UINT32_MAX) abort();
    checked_decode(header, entropy, entropy_len, blinding, record, data[3] % 82);
    const size_t raw_len = size - 6 < 80 ? size - 6 : 80;
    memset(changed, 0, sizeof(changed));
    if (raw_len != 0) memcpy(changed, data + 6, raw_len);
    checked_decode(header, entropy, entropy_len, blinding, changed, raw_len);
    check_capacity(header, entropy, blinding, data[4], data[5], value);
    if (memcmp(entropy, saved, sizeof(saved)) != 0) abort();
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(saved, sizeof(saved));
    return 0;
}
