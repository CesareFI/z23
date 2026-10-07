/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_qr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "QR check failed at line %d\n", __LINE__); abort(); } } while (0)
static const uint8_t main_address[] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";

static void check_modules(const uint8_t *modules, size_t side)
{
    CHECK(side == 41);
    CHECK(modules[4 * side + 4] == 1);
    for (size_t y = 0; y < side; ++y) {
        for (size_t x = 0; x < side; ++x) {
            CHECK(modules[y * side + x] <= 1);
            if (x < 4 || y < 4 || x >= side - 4 || y >= side - 4)
                CHECK(modules[y * side + x] == 0);
        }
    }
}

static void accepted_addresses(void)
{
    uint8_t modules[ZCL_RECEIVE_QR_MODULES_MAX + 2];
    uint8_t address[35], hash[20] = {0};
    size_t side = 0, length = 0;
    for (unsigned int i = 0; i < 16; ++i) {
        memset(hash, (int)(i * 17), sizeof(hash));
        const zcl_network network = (i % 2 == 0) ? ZCL_MAINNET : ZCL_TESTNET;
        CHECK(zcl_address_from_hash(hash, sizeof(hash), network, address, sizeof(address), &length) == ZCL_OK);
        memset(modules, 0xa5, sizeof(modules));
        CHECK(zcl_receive_qr(address, length, network, modules + 1, sizeof(modules) - 2, &side) == ZCL_OK);
        CHECK(modules[0] == 0xa5 && modules[sizeof(modules) - 1] == 0xa5);
        check_modules(modules + 1, side);
    }
    static const uint8_t script_address[] = "t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ";
    CHECK(zcl_receive_qr(script_address, sizeof(script_address) - 1, ZCL_MAINNET,
                         modules, sizeof(modules), &side) == ZCL_OK);
    check_modules(modules, side);
}

static void rejected_inputs(void)
{
    uint8_t modules[ZCL_RECEIVE_QR_MODULES_MAX], before[ZCL_RECEIVE_QR_MODULES_MAX];
    memset(modules, 0xa5, sizeof(modules));
    memcpy(before, modules, sizeof(before));
    size_t side = SIZE_MAX;
    CHECK(zcl_receive_qr(NULL, 35, ZCL_MAINNET, modules, sizeof(modules), &side) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_receive_qr(main_address, 35, ZCL_MAINNET, NULL, sizeof(modules), &side) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_receive_qr(main_address, 35, ZCL_MAINNET, modules, sizeof(modules), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_receive_qr(main_address, SIZE_MAX, ZCL_MAINNET, modules, sizeof(modules), &side) == ZCL_INVALID_ENCODING);
    CHECK(zcl_receive_qr(main_address, 0, ZCL_MAINNET, modules, sizeof(modules), &side) == ZCL_INVALID_ENCODING);
    CHECK(zcl_receive_qr(main_address, 35, ZCL_TESTNET, modules, sizeof(modules), &side) != ZCL_OK);
    CHECK(zcl_receive_qr(main_address, 35, (zcl_network)2, modules, sizeof(modules), &side) == ZCL_UNSUPPORTED);
    for (size_t capacity = 0; capacity < sizeof(modules); ++capacity) {
        CHECK(zcl_receive_qr(main_address, 35, ZCL_MAINNET, modules, capacity, &side) == ZCL_BUFFER_TOO_SMALL);
        CHECK(side == SIZE_MAX && memcmp(modules, before, sizeof(modules)) == 0);
    }
    uint8_t malformed[35];
    memcpy(malformed, main_address, sizeof(malformed));
    malformed[17] = 0;
    CHECK(zcl_receive_qr(malformed, sizeof(malformed), ZCL_MAINNET, modules, sizeof(modules), &side) != ZCL_OK);
    CHECK(side == SIZE_MAX && memcmp(modules, before, sizeof(modules)) == 0);
}

static void exact_output_span(void)
{
    uint8_t expected[ZCL_RECEIVE_QR_MODULES_MAX];
    size_t expected_side = 0;
    CHECK(zcl_receive_qr(main_address, 35, ZCL_MAINNET, expected, sizeof(expected), &expected_side) == ZCL_OK);
    CHECK(expected_side == ZCL_RECEIVE_QR_SIDE_MAX);
    uint8_t guarded[ZCL_RECEIVE_QR_MODULES_MAX + 66];
    const size_t capacities[] = { sizeof(expected), sizeof(expected) + 1, sizeof(expected) + 64 };
    for (size_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i) {
        memset(guarded, 0xa5, sizeof(guarded));
        size_t side = SIZE_MAX;
        CHECK(zcl_receive_qr(main_address, 35, ZCL_MAINNET, guarded + 1, capacities[i], &side) == ZCL_OK);
        CHECK(side == expected_side && memcmp(guarded + 1, expected, side * side) == 0);
        CHECK(guarded[0] == 0xa5);
        for (size_t j = 1 + side * side; j < sizeof(guarded); ++j)
            CHECK(guarded[j] == 0xa5);
    }
}

int main(void)
{
    accepted_addresses();
    rejected_inputs();
    exact_output_span();
    puts("Receiving QR bounds, network validation, modules and unchanged failures passed");
    return 0;
}
