/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_change_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Change state check failed at %d\n", __LINE__); abort(); } } while (0)
static uint8_t entropy[32], header[80], record[80], blinding[32] = {1};

static void prepare(void)
{
    memset(entropy, 0, sizeof(entropy));
    CHECK(zcl_wallet_header_create(entropy, 16, ZCL_MAINNET, blinding, sizeof(blinding),
        header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_change_state_encode(header, sizeof(header), entropy, 16, blinding, sizeof(blinding),
        7, record, sizeof(record)) == ZCL_OK);
}

static void decode_refused(const uint8_t *wallet, size_t wallet_len, const uint8_t *secret, size_t secret_len,
                            const uint8_t *blind, size_t blind_len, const uint8_t *bytes, size_t length)
{
    uint32_t index = UINT32_MAX;
    CHECK(zcl_change_state_decode(wallet, wallet_len, secret, secret_len, blind, blind_len,
        bytes, length, &index) != ZCL_OK);
    CHECK(index == UINT32_MAX);
}

static void encode_refused(const uint8_t *wallet, size_t wallet_len, const uint8_t *secret, size_t secret_len,
                            const uint8_t *blind, size_t blind_len, uint32_t index, size_t capacity)
{
    uint8_t output[82], before[82];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    CHECK(zcl_change_state_encode(wallet, wallet_len, secret, secret_len, blind, blind_len,
        index, output + 1, capacity) != ZCL_OK);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
}

static void exact_records_and_scope(void)
{
    const uint32_t values[] = {0, 1, UINT32_C(0x7fffffff), ZCL_CHANGE_INDEX_EXHAUSTED};
    uint8_t previous[80] = {0};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        uint8_t output[82], alternate[80], blind2[32] = {2};
        memset(output, 0xa5, sizeof(output));
        CHECK(zcl_change_state_encode(header, 80, entropy, 16, blinding, 32, values[i], output + 1, 80) == ZCL_OK);
        CHECK(output[0] == 0xa5 && output[81] == 0xa5);
        CHECK(zcl_change_state_encode(header, 80, entropy, 16, blind2, 32, values[i], alternate, 80) == ZCL_OK);
        CHECK(memcmp(output + 1, alternate, sizeof(alternate)) == 0);
        uint32_t index = UINT32_MAX;
        CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, alternate, 80, &index) == ZCL_OK);
        CHECK(index == values[i]);
        if (i == 0) memcpy(previous, alternate, sizeof(previous));
        else CHECK(memcmp(previous + 16, alternate + 16, 64) != 0);
    }
    /* A codec has no durable state: old authenticated content still verifies.
     * A future storage owner MUST enforce which counter may be consumed next. */
    uint32_t old = UINT32_MAX;
    CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, previous, 80, &old) == ZCL_OK && old == 0);
}

static void record_mutations(void)
{
    for (size_t offset = 0; offset < sizeof(record); ++offset) for (unsigned bit = 0; bit < 8; ++bit) {
        uint8_t changed[80]; memcpy(changed, record, sizeof(changed));
        changed[offset] ^= (uint8_t)(1U << bit);
        decode_refused(header, 80, entropy, 16, blinding, 32, changed, sizeof(changed));
    }
    for (size_t length = 0; length < sizeof(record); ++length)
        decode_refused(header, 80, entropy, 16, blinding, 32, record, length);
    decode_refused(header, 80, entropy, 16, blinding, 32, record, 81);
    decode_refused(header, 80, entropy, 16, blinding, 32, record, SIZE_MAX);
}

static void wallet_binding(void)
{
    uint8_t other_entropy[16] = {1}, other_header[80], test_header[80];
    CHECK(zcl_wallet_header_create(other_entropy, 16, ZCL_MAINNET, blinding, 32, other_header, 80) == ZCL_OK);
    CHECK(zcl_wallet_header_create(entropy, 16, ZCL_TESTNET, blinding, 32, test_header, 80) == ZCL_OK);
    decode_refused(other_header, 80, other_entropy, 16, blinding, 32, record, 80);
    decode_refused(test_header, 80, entropy, 16, blinding, 32, record, 80);
    decode_refused(header, 80, other_entropy, 16, blinding, 32, record, 80);
    encode_refused(header, 80, other_entropy, 16, blinding, 32, 0, 80);
    for (size_t i = 0; i < sizeof(header); ++i) {
        uint8_t changed[80]; memcpy(changed, header, sizeof(changed)); changed[i] ^= 1;
        decode_refused(changed, 80, entropy, 16, blinding, 32, record, 80);
        encode_refused(changed, 80, entropy, 16, blinding, 32, 0, 80);
    }
    zcl_secure_zero(other_entropy, sizeof(other_entropy));
}

static void argument_bounds(void)
{
    for (size_t capacity = 0; capacity < 80; ++capacity)
        encode_refused(header, 80, entropy, 16, blinding, 32, 0, capacity);
    const size_t invalid[] = {0, 15, 17, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        encode_refused(header, 80, entropy, invalid[i], blinding, 32, 0, 80);
        decode_refused(header, 80, entropy, invalid[i], blinding, 32, record, 80);
        encode_refused(header, invalid[i], entropy, 16, blinding, 32, 0, 80);
        decode_refused(header, invalid[i], entropy, 16, blinding, 32, record, 80);
        encode_refused(header, 80, entropy, 16, blinding, invalid[i], 0, 80);
        decode_refused(header, 80, entropy, 16, blinding, invalid[i], record, 80);
    }
    encode_refused(header, 80, entropy, 16, blinding, 32, UINT32_MAX, 80);
    encode_refused(header, 80, entropy, 16, blinding, 32, UINT32_C(0x80000001), 80);
    encode_refused(NULL, 80, entropy, 16, blinding, 32, 0, 80);
    encode_refused(header, 80, NULL, 16, blinding, 32, 0, 80);
    encode_refused(header, 80, entropy, 16, NULL, 32, 0, 80);
    decode_refused(NULL, 80, entropy, 16, blinding, 32, record, 80);
    decode_refused(header, 80, NULL, 16, blinding, 32, record, 80);
    decode_refused(header, 80, entropy, 16, NULL, 32, record, 80);
    decode_refused(header, 80, entropy, 16, blinding, 32, NULL, 80);
    CHECK(zcl_change_state_encode(header, 80, entropy, 16, blinding, 32, 0, NULL, 80) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, record, 80, NULL) == ZCL_INVALID_ARGUMENT);
}

int main(void)
{
    prepare(); exact_records_and_scope(); record_mutations(); wallet_binding(); argument_bounds();
    zcl_secure_zero(entropy, sizeof(entropy));
    puts("Change state content authentication and bounds checks passed");
    return 0;
}
