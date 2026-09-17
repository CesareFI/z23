/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "header_fixture.h"
#include "electrum_genesis_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Header inspection at %d\n", __LINE__); abort(); } } while (0)
static uint8_t wire[1488], before[1488];
static struct { uint8_t before[8]; zcl_header_view view; uint8_t after[8]; } result, unchanged;

static void run(size_t length, zcl_network network, uint32_t height, zcl_status status)
{
    memset(&result, 0xa5, sizeof(result)); memcpy(&unchanged, &result, sizeof(result));
    memcpy(before, wire, sizeof(wire));
    CHECK(zcl_header_inspect(wire, length, network, height, &result.view) == status);
    CHECK(memcmp(wire, before, sizeof(wire)) == 0);
    CHECK(memcmp(result.before, unchanged.before, sizeof(result.before)) == 0);
    CHECK(memcmp(result.after, unchanged.after, sizeof(result.after)) == 0);
    if (status == ZCL_OK) header_fixture_check(wire, length, &result.view);
    else CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
}

static void genesis(zcl_network network, const char *hex, const char *hash)
{
    uint8_t expected[32], merkle[32], zero[32] = {0};
    header_fixture_decode(hex, wire, 1487); header_fixture_decode(hash, expected, 32);
    header_fixture_decode("19612bcf00ea7611d315d7f43554fa983c6e8c30cba17e52c679e0e80abf7d42", merkle, 32);
    run(1487, network, 0, ZCL_OK);
    CHECK(memcmp(result.view.hash, expected, 32) == 0 && memcmp(result.view.merkle, merkle, 32) == 0);
    CHECK(memcmp(result.view.previous, zero, 32) == 0 && memcmp(result.view.sapling, zero, 32) == 0);
    CHECK(result.view.version_bits == 4 && result.view.solution_length == 1344);
    const bool main = network == ZCL_MAINNET;
    CHECK(result.view.timestamp == (main ? UINT32_C(1478403829) : UINT32_C(1479443947)));
    CHECK(result.view.bits == (main ? UINT32_C(0x1f07ffff) : UINT32_C(0x2007ffff)));
    CHECK(result.view.nonce[30] == (main ? 2 : 0) && result.view.nonce[31] == (main ? 29 : 19));
    CHECK(memcmp(result.view.nonce, zero, 30) == 0);
    memset(wire, 0, sizeof(wire)); CHECK(memcmp(result.view.hash, expected, 32) == 0);
}

static void encoding(void)
{
    header_fixture_decode(main_genesis, wire, 1487);
    for (size_t length = 0; length < 1487; ++length) run(length, ZCL_MAINNET, 0, ZCL_INVALID_ENCODING);
    run(1488, ZCL_MAINNET, 0, ZCL_INVALID_ENCODING);
    run(SIZE_MAX, ZCL_MAINNET, 0, ZCL_INVALID_ENCODING);
    for (size_t offset = 140; offset < 143; ++offset) {
        wire[offset] ^= 1; run(1487, ZCL_MAINNET, 0, ZCL_INVALID_ENCODING); wire[offset] ^= 1;
    }
    run(1487, (zcl_network)99, 0, ZCL_UNSUPPORTED);
    CHECK(zcl_header_inspect(NULL, 1487, ZCL_MAINNET, 0, &result.view) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_header_inspect(wire, 1487, ZCL_MAINNET, 0, NULL) == ZCL_INVALID_ARGUMENT);
}

static void epochs(zcl_network network, uint32_t fork)
{
    header_fixture_decode(main_genesis, wire, 1487);
    run(1487, network, fork - 1, ZCL_OK); run(1487, network, fork, ZCL_INVALID_ENCODING);
    for (size_t i = 0; i < 140; ++i) wire[i] = (uint8_t)((i * 71) % 256);
    wire[140] = 253; wire[141] = 144; wire[142] = 1;
    run(543, network, fork - 1, ZCL_INVALID_ENCODING); run(543, network, fork, ZCL_OK);
    run(543, network, fork + 1, ZCL_OK); run(543, network, INT32_MAX, ZCL_OK);
    run(543, network, UINT32_MAX, ZCL_OK);
    memset(wire, 255, 140); run(543, network, UINT32_MAX, ZCL_OK);
    CHECK(result.view.version_bits == UINT32_MAX && result.view.timestamp == UINT32_MAX && result.view.bits == UINT32_MAX);
    for (size_t length = 0; length < 543; ++length) run(length, network, fork, ZCL_INVALID_ENCODING);
    run(544, network, fork, ZCL_INVALID_ENCODING);
}

int main(void)
{
    genesis(ZCL_MAINNET, main_genesis, "0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602");
    genesis(ZCL_TESTNET, test_genesis, "03e1c4bb705c871bf9bfda3e74b7f8f86bff267993c215a89d5795e3708e5e1f");
    encoding(); epochs(ZCL_MAINNET, 585318); epochs(ZCL_TESTNET, 6350);
    puts("Owned header genesis, fields, bounds and epoch checks passed"); return 0;
}
