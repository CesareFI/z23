/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Address encoding check failed at %d\n", __LINE__); abort(); } } while (0)

/* Public original base58_keys_valid.json rows, also used by the JVM fixtures. */
static const struct {
    const char *text;
    const char *hash;
    zcl_network network;
    zcl_address_kind kind;
} vectors[] = {
    {"t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF", "65a16059864a2fdbc7c99a4723a8395bc6f188eb", ZCL_MAINNET, ZCL_P2PKH},
    {"t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ", "74f209f6ea907e2ea48f74fae05782ae8a665257", ZCL_MAINNET, ZCL_P2SH},
    {"tmHMBeeYRuc2eVicLNfP15YLxbQsooCA6jb", "53c0307d6851aa0ce7825ba883c6bd9ad242b486", ZCL_TESTNET, ZCL_P2PKH},
    {"t2Fbo6DBKKVYw1SfrY8bEgz56hYEhywhEN6", "6349a418fc4578d10a372b54b45c280cc8c4382f", ZCL_TESTNET, ZCL_P2SH}
};

static uint8_t nibble(char value)
{
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    CHECK(value >= 'a' && value <= 'f');
    return (uint8_t)(value - 'a' + 10);
}

static void refused(const zcl_address *address, size_t capacity, zcl_status expected)
{
    uint8_t box[37], original[37];
    memset(box, 0xa5, sizeof(box));
    memcpy(original, box, sizeof(original));
    size_t length = SIZE_MAX;
    CHECK(zcl_address_encode(address, box + 1, capacity, &length) == expected);
    CHECK(length == SIZE_MAX && memcmp(box, original, sizeof(box)) == 0);
}

static void original_vectors(void)
{
    for (size_t row = 0; row < sizeof(vectors) / sizeof(vectors[0]); ++row) {
        zcl_address address = {vectors[row].network, vectors[row].kind, {0}};
        for (size_t i = 0; i < sizeof(address.hash); ++i)
            address.hash[i] = (uint8_t)(16 * nibble(vectors[row].hash[2 * i]) + nibble(vectors[row].hash[2 * i + 1]));
        uint8_t text[37];
        memset(text, 0xa5, sizeof(text));
        size_t length = 0;
        CHECK(zcl_address_encode(&address, text + 1, 35, &length) == ZCL_OK);
        CHECK(length == 35 && memcmp(text + 1, vectors[row].text, length) == 0);
        CHECK(text[0] == 0xa5 && text[36] == 0xa5);
        zcl_address parsed;
        CHECK(zcl_address_parse(text + 1, length, vectors[row].network, &parsed) == ZCL_OK);
        CHECK(parsed.kind == address.kind && parsed.network == address.network);
        CHECK(memcmp(parsed.hash, address.hash, sizeof(address.hash)) == 0);
        for (size_t capacity = 0; capacity < 35; ++capacity)
            refused(&address, capacity, ZCL_BUFFER_TOO_SMALL);
        if (address.kind == ZCL_P2PKH) {
            uint8_t legacy[35];
            size_t legacy_length = 0;
            CHECK(zcl_address_from_hash(address.hash, sizeof(address.hash), address.network,
                legacy, sizeof(legacy), &legacy_length) == ZCL_OK);
            CHECK(legacy_length == length && memcmp(legacy, text + 1, length) == 0);
        }
    }
}

static void arbitrary_hashes(void)
{
    for (int network = 0; network < 2; ++network) {
        for (int kind = 1; kind <= 2; ++kind) {
            for (unsigned value = 0; value <= UINT8_MAX; ++value) {
                zcl_address address = {(zcl_network)network, (zcl_address_kind)kind, {0}};
                for (size_t i = 0; i < sizeof(address.hash); ++i)
                    address.hash[i] = (uint8_t)((value + i) & UINT8_MAX);
                uint8_t text[35];
                size_t length = 0;
                CHECK(zcl_address_encode(&address, text, sizeof(text), &length) == ZCL_OK && length == 35);
                zcl_address parsed;
                CHECK(zcl_address_parse(text, length, address.network, &parsed) == ZCL_OK);
                CHECK(parsed.kind == address.kind && memcmp(parsed.hash, address.hash, 20) == 0);
                const zcl_network wrong = network == 0 ? ZCL_TESTNET : ZCL_MAINNET;
                CHECK(zcl_address_parse(text, length, wrong, &parsed) == ZCL_UNSUPPORTED);
            }
        }
    }
}

static void arguments(void)
{
    zcl_address address = {ZCL_MAINNET, ZCL_P2PKH, {0}};
    refused(NULL, 35, ZCL_INVALID_ARGUMENT);
    address.network = (zcl_network)2;
    refused(&address, 35, ZCL_UNSUPPORTED);
    address.network = (zcl_network)INT_MAX;
    refused(&address, 35, ZCL_UNSUPPORTED);
    address.network = ZCL_MAINNET;
    const int kinds[] = {0, 3, INT_MAX};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        address.kind = (zcl_address_kind)kinds[i];
        refused(&address, 35, ZCL_UNSUPPORTED);
    }
    address.kind = ZCL_P2SH;
    uint8_t text[35], original[35];
    memset(text, 0xa5, sizeof(text));
    memcpy(original, text, sizeof(original));
    size_t length = SIZE_MAX;
    CHECK(zcl_address_encode(&address, NULL, 35, &length) == ZCL_INVALID_ARGUMENT && length == SIZE_MAX);
    CHECK(zcl_address_encode(&address, text, 35, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(text, original, sizeof(text)) == 0);
}

int main(void)
{
    original_vectors(); arbitrary_hashes(); arguments();
    puts("Canonical P2PKH/P2SH address encoding checks passed");
    return 0;
}
