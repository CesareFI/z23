/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static zcl_status checked_derivation(const uint8_t *header, size_t header_len,
                                    const uint8_t *entropy, size_t entropy_len, uint32_t index,
                                    const uint8_t *blinding, size_t blind_len, size_t capacity)
{
    uint8_t output[37], before[37];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    const zcl_status status = zcl_wallet_recovered_change(header, header_len, entropy, entropy_len,
        index, blinding, blind_len, output + 1, capacity);
    if (status != ZCL_OK) {
        if (memcmp(output, before, sizeof(output)) != 0) abort();
        return status;
    }
    if (capacity < 35 || output[0] != 0xa5 || output[36] != 0xa5) abort();
    zcl_wallet_info info;
    if (zcl_wallet_header_parse(header, header_len, &info) != ZCL_OK) abort();
    uint8_t expected[35], alternate[32] = {3};
    size_t length = 0;
    if (zcl_change_from_entropy(entropy, entropy_len, info.network, index,
        alternate, sizeof(alternate), expected, sizeof(expected), &length) != ZCL_OK) abort();
    if (length != 35 || memcmp(output + 1, expected, 35) != 0) abort();
    if (memcmp(output + 1, info.address, 35) == 0) abort();
    return ZCL_OK;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 6 || size > 118) return 0;
    uint8_t entropy[32] = {0}, saved[32], header[80], changed[80], blinding[64];
    const size_t entropy_len = 16 + (size_t)(data[0] % 5) * 4;
    for (size_t i = 0; i < entropy_len; ++i) entropy[i] = data[i % size];
    memcpy(saved, entropy, sizeof(saved));
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    uint32_t index = 0;
    for (size_t i = 0; i < 4; ++i) index |= (uint32_t)data[i + 1] << (8 * i);
    const zcl_network network = (data[0] & 1) != 0 ? ZCL_TESTNET : ZCL_MAINNET;
    if (zcl_wallet_header_create(entropy, entropy_len, network, blinding, 32,
        header, sizeof(header)) != ZCL_OK) abort();
    /* Keep complete binding/derivation in every input, so invalid headers
     * cannot dominate the campaign through cheap argument refusal alone. */
    if (checked_derivation(header, sizeof(header), entropy, entropy_len,
        index & UINT32_C(0x7fffffff), blinding, sizeof(blinding), 35) != ZCL_OK) abort();
    memcpy(changed, header, sizeof(changed));
    changed[data[1] % sizeof(changed)] ^= (uint8_t)(1U << (data[2] % 8));
    if (checked_derivation(changed, sizeof(changed), entropy, entropy_len,
        index & UINT32_C(0x7fffffff), blinding, sizeof(blinding), 35) == ZCL_OK) abort();
    (void)checked_derivation(header, data[3] % 82, entropy, data[4] % 34,
        index, blinding, data[5] % 66, data[5] % 36);
    const size_t raw_len = size - 6 < sizeof(changed) ? size - 6 : sizeof(changed);
    memset(changed, 0, sizeof(changed));
    if (raw_len != 0) memcpy(changed, data + 6, raw_len);
    (void)checked_derivation(changed, raw_len, entropy, entropy_len,
        index & UINT32_C(0x7fffffff), blinding, sizeof(blinding), 35);
    if (memcmp(entropy, saved, sizeof(saved)) != 0) abort();
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(saved, sizeof(saved));
    zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}
