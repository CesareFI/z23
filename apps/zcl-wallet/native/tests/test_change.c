/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"
#include "../src/change_custody_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Change check failed at %d\n", __LINE__); abort(); } } while (0)
typedef zcl_status (*address_function)(const uint8_t *, size_t, zcl_network, uint32_t,
    const uint8_t *, size_t, uint8_t *, size_t, size_t *);
static const address_function functions[] = {zcl_receive_from_entropy, zcl_change_from_entropy};

static void refused(const uint8_t *entropy, size_t length, zcl_network network, uint32_t index,
                     const uint8_t *blinding, size_t blinding_len, size_t capacity, zcl_status expected)
{
    for (size_t chain = 0; chain < 2; ++chain) {
        uint8_t output[37], before[37];
        memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
        size_t written = SIZE_MAX;
        CHECK(functions[chain](entropy, length, network, index, blinding, blinding_len,
            output + 1, capacity, &written) == expected);
        CHECK(written == SIZE_MAX && memcmp(output, before, sizeof(output)) == 0);
    }
}

static void bounds(void)
{
    uint8_t entropy[33] = {0}, blinding[33] = {1}, output[35];
    for (size_t size = 0; size <= sizeof(entropy); ++size) {
        if (size >= 16 && size <= 32 && size % 4 == 0) continue;
        refused(entropy, size, ZCL_MAINNET, 0, blinding, 32, 35, ZCL_OUT_OF_RANGE);
    }
    refused(entropy, SIZE_MAX, ZCL_MAINNET, 0, blinding, 32, 35, ZCL_OUT_OF_RANGE);
    refused(entropy, 16, (zcl_network)2, 0, blinding, 32, 35, ZCL_UNSUPPORTED);
    refused(entropy, 16, ZCL_TESTNET, UINT32_C(0x80000000), blinding, 32, 35, ZCL_OUT_OF_RANGE);
    refused(entropy, 16, ZCL_TESTNET, UINT32_MAX, blinding, 32, 35, ZCL_OUT_OF_RANGE);
    for (size_t cap = 0; cap < 35; ++cap)
        refused(entropy, 16, ZCL_MAINNET, 0, blinding, 32, cap, ZCL_BUFFER_TOO_SMALL);
    const size_t blind_lengths[] = {0, 1, 31, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(blind_lengths) / sizeof(blind_lengths[0]); ++i)
        refused(entropy, 16, ZCL_MAINNET, 0, blinding, blind_lengths[i], 35, ZCL_INVALID_ARGUMENT);
    refused(NULL, 16, ZCL_MAINNET, 0, blinding, 32, 35, ZCL_INVALID_ARGUMENT);
    refused(entropy, 16, ZCL_MAINNET, 0, NULL, 32, 35, ZCL_INVALID_ARGUMENT);
    for (size_t chain = 0; chain < 2; ++chain) {
        size_t length = SIZE_MAX;
        CHECK(functions[chain](entropy, 16, ZCL_MAINNET, 0, blinding, 32, NULL, 35, &length) == ZCL_INVALID_ARGUMENT);
        CHECK(length == SIZE_MAX);
        CHECK(functions[chain](entropy, 16, ZCL_MAINNET, 0, blinding, 32, output, 35, NULL) == ZCL_INVALID_ARGUMENT);
    }
}

static void distinct_and_blinded(void)
{
    uint8_t entropy[32] = {0}, blinding[32] = {1};
    for (int network = 0; network < 2; ++network) {
        uint8_t addresses[2][37], repeated[35];
        memset(addresses, 0xa5, sizeof(addresses));
        for (size_t chain = 0; chain < 2; ++chain) {
            size_t length = 0;
            CHECK(functions[chain](entropy, 32, (zcl_network)network, 0, blinding, 32,
                addresses[chain] + 1, 35, &length) == ZCL_OK);
            CHECK(length == 35 && addresses[chain][0] == 0xa5 && addresses[chain][36] == 0xa5);
            uint8_t other_blinding[32] = {2};
            CHECK(functions[chain](entropy, 32, (zcl_network)network, 0, other_blinding, 32,
                repeated, sizeof(repeated), &length) == ZCL_OK);
            CHECK(length == 35 && memcmp(repeated, addresses[chain] + 1, 35) == 0);
            zcl_address address;
            CHECK(zcl_address_parse(repeated, length, (zcl_network)network, &address) == ZCL_OK);
            CHECK(address.kind == ZCL_P2PKH && address.network == (zcl_network)network);
        }
        CHECK(memcmp(addresses[0] + 1, addresses[1] + 1, 35) != 0);
    }
    for (size_t i = 0; i < sizeof(entropy); ++i) CHECK(entropy[i] == 0);
    zcl_secure_zero(entropy, sizeof(entropy));
}

static void null_custody_arguments(void)
{
    uint8_t record[140] = {0}, entropy[16] = {0}, state[80] = {0}, address[35] = {0};
    zcl_change_custody wallet = {0};
    uint32_t index = 0;
    CHECK(zcl_change_custody_prepare(record, sizeof(record), entropy, sizeof(entropy), NULL)
        == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_encode(NULL, 0, state) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_encode(&wallet, 0, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_decode(NULL, state, sizeof(state), &index) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_decode(&wallet, NULL, sizeof(state), &index) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_decode(&wallet, state, sizeof(state), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_address(NULL, 0, address, sizeof(address)) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_change_custody_address(&wallet, 0, NULL, sizeof(address)) == ZCL_INVALID_ARGUMENT);
}

int main(void)
{
    bounds(); distinct_and_blinded(); null_custody_arguments();
    puts("Receive/change bounds, separation and blinding checks passed");
    return 0;
}
