/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_recovery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    for (size_t i = 0; i < length; ++i) assert(bytes[i] == value);
}

static void equivalent(size_t entropy_len, zcl_network network, uint32_t chain,
    uint32_t first, size_t count)
{
    uint8_t entropy[32], blinding[32], output[563], expected[35];
    memset(entropy, 0x42, sizeof(entropy)); memset(blinding, 0x17, sizeof(blinding));
    memset(output, 0xa5, sizeof(output));
    assert(zcl_recovery_address_batch(entropy, entropy_len, network, chain, first, count,
        blinding, sizeof(blinding), output + 1, sizeof(output) - 2) == ZCL_OK);
    for (size_t i = 0; i < count; ++i) {
        size_t length = 0;
        const uint32_t index = first + (uint32_t)i;
        zcl_status status = chain == 0
            ? zcl_receive_from_entropy(entropy, entropy_len, network, index, blinding, 32,
                expected, sizeof(expected), &length)
            : zcl_change_from_entropy(entropy, entropy_len, network, index, blinding, 32,
                expected, sizeof(expected), &length);
        assert(status == ZCL_OK && length == sizeof(expected));
        assert(memcmp(output + 1 + i * sizeof(expected), expected, sizeof(expected)) == 0);
    }
    assert(output[0] == 0xa5);
    const size_t used = 1 + count * sizeof(expected);
    filled(output + used, sizeof(output) - used, 0xa5);
    filled(entropy, sizeof(entropy), 0x42); filled(blinding, sizeof(blinding), 0x17);
}

static void success_matrix(void)
{
    static const struct { uint32_t first; size_t count; } ranges[] = {
        {0, 16}, {19, 2}, {UINT32_C(0x7ffffff0), 16}, {UINT32_C(0x7fffffff), 1}
    };
    for (size_t length = 16; length <= 32; length += 4)
        for (uint32_t chain = 0; chain < 2; ++chain)
            for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i) {
                equivalent(length, ZCL_MAINNET, chain, ranges[i].first, ranges[i].count);
                equivalent(length, ZCL_TESTNET, chain, ranges[i].first, ranges[i].count);
            }
}

static void rejected_ranges(void)
{
    static const struct {
        size_t entropy_len; zcl_network network; uint32_t chain, first;
        size_t count, capacity; zcl_status expected;
    } cases[] = {
        {0, ZCL_MAINNET, 0, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {15, ZCL_MAINNET, 0, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {17, ZCL_MAINNET, 0, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {33, ZCL_MAINNET, 0, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {SIZE_MAX, ZCL_MAINNET, 0, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {16, (zcl_network)99, 0, 0, 1, 560, ZCL_UNSUPPORTED},
        {16, ZCL_MAINNET, 2, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, UINT32_MAX, 0, 1, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, UINT32_C(0x80000000), 1, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, UINT32_MAX, 1, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, UINT32_C(0x7fffffff), 2, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, UINT32_C(0x7ffffff1), 16, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, 0, 0, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, 0, 17, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, 0, SIZE_MAX, 560, ZCL_OUT_OF_RANGE},
        {16, ZCL_MAINNET, 0, 0, 1, 0, ZCL_BUFFER_TOO_SMALL},
        {16, ZCL_MAINNET, 0, 0, 1, 34, ZCL_BUFFER_TOO_SMALL},
        {16, ZCL_MAINNET, 0, 0, 16, 559, ZCL_BUFFER_TOO_SMALL}
    };
    uint8_t entropy[32] = {0}, blinding[32] = {1}, output[562];
    memset(output, 0xa5, sizeof(output));
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        assert(zcl_recovery_address_batch(entropy, cases[i].entropy_len, cases[i].network,
            cases[i].chain, cases[i].first, cases[i].count, blinding, sizeof(blinding),
            output + 1, cases[i].capacity) == cases[i].expected);
        filled(output, sizeof(output), 0xa5);
    }
}

static void rejected_spans(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, output[35];
    memset(output, 0xa5, sizeof(output));
    assert(zcl_recovery_address_batch(NULL, 16, ZCL_MAINNET, 0, 0, 1,
        blinding, 32, output, 35) == ZCL_INVALID_ARGUMENT);
    assert(zcl_recovery_address_batch(entropy, 16, ZCL_MAINNET, 0, 0, 1,
        blinding, 32, NULL, 35) == ZCL_INVALID_ARGUMENT);
    assert(zcl_recovery_address_batch(entropy, 16, ZCL_MAINNET, 0, 0, 1,
        NULL, 32, output, 35) == ZCL_INVALID_ARGUMENT);
    static const size_t lengths[] = {0, 31, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        assert(zcl_recovery_address_batch(entropy, 16, ZCL_MAINNET, 0, 0, 1,
            blinding, lengths[i], output, 35) == ZCL_INVALID_ARGUMENT);
        filled(output, sizeof(output), 0xa5);
    }
}

int main(void)
{
    rejected_ranges(); rejected_spans(); success_matrix();
    puts("recovery batch: 80 batches / 700 address comparisons, bounds and unchanged-output refusals passed");
    return 0;
}
