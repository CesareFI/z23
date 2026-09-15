/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_change_state.h"
#include "change_state_oracle.h"
#include <stdio.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Change state oracle failed at %d\n", __LINE__); return 1; } } while (0)

static int oracle_bounds(void)
{
    uint8_t entropy[32] = {0}, header[80] = {0}, output[82], before[82];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    CHECK(!change_state_oracle_record(NULL, 16, header, 80, 0, output + 1, 80));
    CHECK(!change_state_oracle_record(entropy, 16, NULL, 80, 0, output + 1, 80));
    CHECK(!change_state_oracle_record(entropy, 16, header, 80, 0, NULL, 80));
    for (size_t length = 0; length <= 33; ++length) {
        if (length >= 16 && length <= 32 && length % 4 == 0) continue;
        CHECK(!change_state_oracle_record(entropy, length, header, 80, 0, output + 1, 80));
    }
    CHECK(!change_state_oracle_record(entropy, SIZE_MAX, header, 80, 0, output + 1, 80));
    const size_t lengths[] = {0, 1, 79, 81, SIZE_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        CHECK(!change_state_oracle_record(entropy, 16, header, lengths[i], 0, output + 1, 80));
    for (size_t capacity = 0; capacity < 80; ++capacity)
        CHECK(!change_state_oracle_record(entropy, 16, header, 80, 0, output + 1, capacity));
    CHECK(!change_state_oracle_record(entropy, 16, header, 80, UINT32_C(0x80000001), output + 1, 80));
    CHECK(!change_state_oracle_record(entropy, 16, header, 80, UINT32_MAX, output + 1, 80));
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    return 0;
}

int main(void)
{
    CHECK(oracle_bounds() == 0);
    uint8_t entropy[32], blinding[32] = {1};
    const uint32_t values[] = {0, 1, UINT32_C(0x7fffffff), ZCL_CHANGE_INDEX_EXHAUSTED,
        UINT32_C(0x100), UINT32_C(0x10000), UINT32_C(0x1000000), UINT32_C(0x01020304),
        UINT32_C(0x10203040), UINT32_C(0x12345678), UINT32_C(0x55555555), UINT32_C(0x2aaaaaaa)};
    for (size_t size = 16; size <= 32; size += 4) for (int network = 0; network < 2; ++network) {
        uint8_t header[80];
        for (size_t i = 0; i < sizeof(entropy); ++i) entropy[i] = (uint8_t)(i + size);
        CHECK(zcl_wallet_header_create(entropy, size, (zcl_network)network, blinding, sizeof(blinding),
            header, sizeof(header)) == ZCL_OK);
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
            uint8_t expected[82], actual[82];
            memset(expected, 0xa5, sizeof(expected)); memset(actual, 0xa5, sizeof(actual));
            uint32_t decoded = UINT32_MAX;
            CHECK(change_state_oracle_record(entropy, size, header, sizeof(header), values[i], expected + 1, 80));
            CHECK(zcl_change_state_encode(header, sizeof(header), entropy, size, blinding, sizeof(blinding),
                values[i], actual + 1, 80) == ZCL_OK);
            CHECK(expected[0] == 0xa5 && expected[81] == 0xa5 && actual[0] == 0xa5 && actual[81] == 0xa5);
            CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
            CHECK(zcl_change_state_decode(header, sizeof(header), entropy, size, blinding, sizeof(blinding),
                expected + 1, 80, &decoded) == ZCL_OK && decoded == values[i]);
        }
    }
    zcl_secure_zero(entropy, sizeof(entropy));
    puts("Change state: 120 OpenSSL HKDF-SHA512/HMAC record comparisons passed");
    return 0;
}
