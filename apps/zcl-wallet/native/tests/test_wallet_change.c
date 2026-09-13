/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Wallet change check failed at %d\n", __LINE__); abort(); } } while (0)
static uint8_t entropy[32], header[80], blinding[64];

static void prepare(zcl_network network, size_t length)
{
    for (size_t i = 0; i < sizeof(entropy); ++i) entropy[i] = (uint8_t)(i + length);
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    CHECK(zcl_wallet_header_create(entropy, length, network, blinding, 32, header, sizeof(header)) == ZCL_OK);
}

static void refused(const uint8_t *source, size_t header_len, const uint8_t *secret, size_t entropy_len,
                    uint32_t index, const uint8_t *blind, size_t blind_len, size_t capacity)
{
    uint8_t output[37], before[37];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    CHECK(zcl_wallet_recovered_change(source, header_len, secret, entropy_len, index,
        blind, blind_len, output + 1, capacity) != ZCL_OK);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
}

static void exact_binding(void)
{
    const uint32_t indexes[] = {0, 1, UINT32_C(0x7fffffff)};
    for (int network = 0; network < 2; ++network) for (size_t size = 16; size <= 32; size += 4) {
        prepare((zcl_network)network, size);
        uint8_t saved_entropy[32], saved_header[80], saved_blinding[64];
        memcpy(saved_entropy, entropy, sizeof(entropy));
        memcpy(saved_header, header, sizeof(header));
        memcpy(saved_blinding, blinding, sizeof(blinding));
        for (size_t i = 0; i < sizeof(indexes) / sizeof(indexes[0]); ++i) {
            uint8_t output[37], expected[35], repeated[35], alternate[64];
            memset(output, 0xa5, sizeof(output)); memset(alternate, 3, sizeof(alternate));
            memset(alternate + 32, 4, 32);
            size_t length = 0;
            CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, size, indexes[i],
                blinding, sizeof(blinding), output + 1, 35) == ZCL_OK);
            CHECK(output[0] == 0xa5 && output[36] == 0xa5);
            CHECK(zcl_change_from_entropy(entropy, size, (zcl_network)network, indexes[i],
                alternate + 32, 32, expected, sizeof(expected), &length) == ZCL_OK);
            CHECK(length == 35 && memcmp(output + 1, expected, 35) == 0);
            CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, size, indexes[i],
                alternate, sizeof(alternate), repeated, sizeof(repeated)) == ZCL_OK);
            CHECK(memcmp(repeated, expected, sizeof(repeated)) == 0);
            CHECK(memcmp(repeated, header + 44, sizeof(repeated)) != 0);
            zcl_address address;
            CHECK(zcl_address_parse(repeated, sizeof(repeated), (zcl_network)network, &address) == ZCL_OK);
            CHECK(address.kind == ZCL_P2PKH);
        }
        CHECK(memcmp(saved_entropy, entropy, sizeof(entropy)) == 0);
        CHECK(memcmp(saved_header, header, sizeof(header)) == 0);
        CHECK(memcmp(saved_blinding, blinding, sizeof(blinding)) == 0);
        zcl_secure_zero(saved_entropy, sizeof(saved_entropy));
    }
}

static void header_and_wallet_refusals(void)
{
    prepare(ZCL_MAINNET, 16);
    for (size_t length = 0; length < sizeof(header); ++length)
        refused(header, length, entropy, 16, 0, blinding, sizeof(blinding), 35);
    refused(header, sizeof(header) + 1, entropy, 16, 0, blinding, sizeof(blinding), 35);
    refused(header, SIZE_MAX, entropy, 16, 0, blinding, sizeof(blinding), 35);
    for (size_t i = 0; i < sizeof(header); ++i) for (unsigned bit = 0; bit < 8; ++bit) {
        uint8_t changed[80]; memcpy(changed, header, sizeof(changed));
        changed[i] ^= (uint8_t)(1U << bit);
        refused(changed, sizeof(changed), entropy, 16, 0, blinding, sizeof(blinding), 35);
    }
    uint8_t other[32], other_header[80]; memcpy(other, entropy, sizeof(other));
    other[0] ^= 1;
    CHECK(zcl_wallet_header_create(other, 16, ZCL_MAINNET, blinding, 32,
        other_header, sizeof(other_header)) == ZCL_OK);
    refused(other_header, sizeof(other_header), entropy, 16, 0, blinding, sizeof(blinding), 35);
    refused(header, sizeof(header), other, 16, 0, blinding, sizeof(blinding), 35);
    zcl_secure_zero(other, sizeof(other));
}

static void argument_refusals(void)
{
    prepare(ZCL_TESTNET, 16);
    for (size_t length = 0; length <= 33; ++length) {
        if (length != 16) refused(header, 80, entropy, length, 0, blinding, 64, 35);
    }
    refused(header, 80, entropy, SIZE_MAX, 0, blinding, 64, 35);
    for (size_t length = 0; length <= 65; ++length) {
        if (length != 64) refused(header, 80, entropy, 16, 0, blinding, length, 35);
    }
    refused(header, 80, entropy, 16, 0, blinding, SIZE_MAX, 35);
    refused(header, 80, entropy, 16, UINT32_C(0x80000000), blinding, 64, 35);
    refused(header, 80, entropy, 16, UINT32_MAX, blinding, 64, 35);
    for (size_t capacity = 0; capacity < 35; ++capacity)
        refused(header, 80, entropy, 16, 0, blinding, 64, capacity);
    refused(NULL, 80, entropy, 16, 0, blinding, 64, 35);
    refused(header, 80, NULL, 16, 0, blinding, 64, 35);
    refused(header, 80, entropy, 16, 0, NULL, 64, 35);
    CHECK(zcl_wallet_recovered_change(header, 80, entropy, 16, 0, blinding, 64, NULL, 35) == ZCL_INVALID_ARGUMENT);
}

int main(void)
{
    exact_binding(); header_and_wallet_refusals(); argument_refusals();
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(blinding, sizeof(blinding));
    puts("Recovered wallet change binding checks passed");
    return 0;
}
