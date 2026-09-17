/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef mbedtls_sha256
#undef zcl_secure_zero
#include "header_fixture.h"
#include "electrum_internal.h"
#include "electrum_genesis_fixture.h"
#include "zcl_keys.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Header retirement at %d\n", __LINE__); abort(); } } while (0)
static uint8_t wire[1487];
static char frame[3000];
static zcl_rpc_json doc;
static uintptr_t outputs[2], hashed_wire;
static unsigned calls, failure, clears;
int zcl_header_test_sha(const unsigned char *input, size_t length, unsigned char output[32], int sha224);
void zcl_header_test_zero(void *buffer, size_t length);

int zcl_header_test_sha(const unsigned char *input, size_t length, unsigned char output[32], int sha224)
{
    CHECK(calls < 2 && clears == 0 && sha224 == 0);
    CHECK(length == (calls == 0 ? 1487U : 32U));
    if (calls == 0) hashed_wire = (uintptr_t)input;
    outputs[calls++] = (uintptr_t)output;
    if (calls == failure) { memset(output, 0xa5, 32); return -1; }
    return mbedtls_sha256(input, length, output, sha224);
}

void zcl_header_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && buffer != wire);
    if (length == sizeof(zcl_header_view) + 64) {
        CHECK(clears == 0);
        for (unsigned i = 0; i < calls; ++i)
            CHECK(outputs[i] >= (uintptr_t)buffer && outputs[i] - (uintptr_t)buffer <= length - 32);
    } else if (length == sizeof(wire) && calls != 0) CHECK((uintptr_t)buffer == hashed_wire);
    else CHECK(length == sizeof(wire) || length == sizeof(zcl_header_view));
    ++clears;
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}

static void raw(void)
{
    zcl_header_view view, unchanged;
    header_fixture_decode(main_genesis, wire, sizeof(wire));
    for (unsigned point = 0; point <= 2; ++point) {
        failure = point; calls = clears = 0; memset(&view, 0xa5, sizeof(view));
        memcpy(&unchanged, &view, sizeof(view));
        CHECK(zcl_header_inspect(wire, sizeof(wire), ZCL_MAINNET, 0, &view) == (point == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
        CHECK(calls == (point == 0 ? 2 : point) && clears == 1);
        if (point != 0) CHECK(memcmp(&view, &unchanged, sizeof(view)) == 0);
    }
    calls = clears = 0; wire[140] ^= 1;
    CHECK(zcl_header_inspect(wire, sizeof(wire), ZCL_MAINNET, 0, &view) == ZCL_INVALID_ENCODING);
    CHECK(calls == 0 && clears == 0);
}

static void rpc(unsigned point, bool malformed)
{
    const int length = snprintf(frame, sizeof(frame), "{\"hex\":\"%s\"}", test_genesis);
    CHECK(length > 0 && (size_t)length < sizeof(frame));
    if (malformed) frame[1800] = 'g'; /* Partial decoded header must still retire. */
    CHECK(zcl_rpc_json_parse((const uint8_t *)frame, (size_t)length, &doc) == ZCL_OK);
    uint8_t hash[32], unchanged[32]; memset(hash, 0xa5, sizeof(hash)); memcpy(unchanged, hash, 32);
    failure = point; calls = clears = 0;
    const zcl_status expected = malformed ? ZCL_INVALID_ENCODING : point == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
    CHECK(zcl_rpc_header_hash(&doc, &doc.tokens[2], ZCL_TESTNET, 0, hash) == expected);
    CHECK(clears == (malformed ? 2U : 3U));
    CHECK(calls == (malformed ? 0U : point == 0 ? 2U : point));
    if (expected != ZCL_OK) CHECK(memcmp(hash, unchanged, 32) == 0);
}

int main(void)
{
    raw(); for (unsigned point = 0; point <= 2; ++point) rpc(point, false);
    rpc(0, true);
    puts("Header raw/RPC dirty-provider failure and retirement checks passed"); return 0;
}
