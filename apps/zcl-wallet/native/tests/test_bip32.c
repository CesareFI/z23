/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"
#ifdef ZCL_BIP32_ORACLE
#include "bip32_oracle.h"
#endif

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "BIP32 check failed at line %d\n", __LINE__); return 1; } } while (0)

typedef struct {
    uint8_t seed_hex[129];
    size_t seed_len;
    size_t depth;
    uint32_t path[5];
    uint8_t public_text[112];
    uint8_t private_text[112];
} bip32_fixture;
static const bip32_fixture fixtures[] = {
#include "bip32_vectors.inc"
};

static unsigned nibble(uint8_t value)
{
    return value <= '9' ? (unsigned)(value - '0') : (unsigned)(value - 'a') + 10U;
}

static int fixture_seed(const bip32_fixture *fixture, uint8_t *seed, size_t capacity)
{
    if (fixture->seed_len > capacity || fixture->seed_len > 64)
        return 1;
    for (size_t i = 0; i < fixture->seed_len; ++i) {
        unsigned high = nibble(fixture->seed_hex[i * 2]), low = nibble(fixture->seed_hex[i * 2 + 1]);
        if (high > 15 || low > 15)
            return 1;
        seed[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

#ifdef ZCL_BIP32_ORACLE
static int independent_node(const bip32_fixture *fixture, const uint8_t *seed,
    const zcl_extended_private *expected)
{
    zcl_extended_private node = {0}, child = {0};
    uint8_t public_key[33] = {0}, decoded[78] = {0};
    size_t length = 0;
    CHECK(zcl_test_bip32_master(seed, fixture->seed_len, &node) == ZCL_OK);
    CHECK(fixture->depth <= 5);
    for (size_t i = 0; i < fixture->depth; ++i) {
        CHECK(zcl_test_bip32_child(&node, fixture->path[i], &child) == ZCL_OK);
        node = child;
        zcl_secure_zero(&child, sizeof(child));
    }
    CHECK(memcmp(&node, expected, sizeof(node)) == 0);
    CHECK(zcl_test_bip32_public(node.secret, 32, public_key, sizeof(public_key)));
    CHECK(zcl_base58check_decode(fixture->public_text, 111, decoded, sizeof(decoded), &length) == ZCL_OK);
    CHECK(length == 78 && memcmp(public_key, decoded + 45, sizeof(public_key)) == 0);
    zcl_secure_zero(&node, sizeof(node));
    return 0;
}
#endif

static int known_vectors(void)
{
    uint8_t blinding[32] = {1};
    CHECK(sizeof(fixtures) / sizeof(fixtures[0]) == 17);
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        const bip32_fixture *fixture = &fixtures[i];
        uint8_t seed[64] = {0}, public_key[33] = {0};
        uint8_t expected_private[78] = {0}, expected_public[78] = {0};
        size_t private_len = 0, public_len = 0;
        zcl_extended_private node = {0}, child = {0};
        CHECK(fixture_seed(fixture, seed, sizeof(seed)) == 0);
        CHECK(zcl_bip32_master(seed, fixture->seed_len, &node) == ZCL_OK);
        CHECK(fixture->depth <= 5);
        for (size_t level = 0; level < fixture->depth; ++level) {
            CHECK(zcl_bip32_child(&node, fixture->path[level], blinding, sizeof(blinding), &child) == ZCL_OK);
            node = child;
        }
        /* These xprv values are published fixtures, not private wallet input.
         * Production secret imports never use the public Base58 adapter. */
        CHECK(zcl_base58check_decode(fixture->private_text, 111, expected_private,
                                     sizeof(expected_private), &private_len) == ZCL_OK);
        CHECK(zcl_base58check_decode(fixture->public_text, 111, expected_public,
                                     sizeof(expected_public), &public_len) == ZCL_OK);
        CHECK(private_len == 78 && public_len == 78);
        CHECK(expected_private[45] == 0);
        CHECK(memcmp(node.secret, expected_private + 46, 32) == 0);
        CHECK(memcmp(node.chain_code, expected_private + 13, 32) == 0);
        CHECK(memcmp(node.chain_code, expected_public + 13, 32) == 0);
        CHECK(zcl_public_key(node.secret, sizeof(node.secret), blinding, sizeof(blinding),
                             public_key, sizeof(public_key)) == ZCL_OK);
        CHECK(memcmp(public_key, expected_public + 45, 33) == 0);
#ifdef ZCL_BIP32_ORACLE
        CHECK(independent_node(fixture, seed, &node) == 0);
#endif
        zcl_secure_zero(seed, sizeof(seed));
        zcl_secure_zero(&node, sizeof(node));
        zcl_secure_zero(&child, sizeof(child));
    }
    return 0;
}

static int failures_preserve_output(void)
{
    uint8_t seed[64] = {0}, blinding[32] = {1}, public_key[34] = {0};
    zcl_extended_private node = {0}, output = {0}, before = {0};
    memset(&output, 0xa5, sizeof(output));
    before = output;
    CHECK(zcl_bip32_master(NULL, 16, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_bip32_master(seed, 15, &output) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_bip32_master(seed, SIZE_MAX, &output) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_bip32_child(NULL, 0, blinding, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_bip32_child(&node, 0, blinding, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_bip32_master(seed, 16, &node) == ZCL_OK);
    CHECK(zcl_bip32_child(&node, 0, NULL, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_bip32_child(&node, UINT32_MAX, blinding, 31, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    memset(public_key, 0x5a, sizeof(public_key));
    for (size_t capacity = 0; capacity < 33; ++capacity) {
        CHECK(zcl_public_key(node.secret, 32, blinding, 32, public_key, capacity) != ZCL_OK);
        for (size_t i = 0; i < sizeof(public_key); ++i)
            CHECK(public_key[i] == 0x5a);
    }
    CHECK(zcl_public_key(seed, 32, blinding, 32, public_key, 33) == ZCL_CRYPTO_FAILURE);
    CHECK(zcl_public_key(node.secret, SIZE_MAX, blinding, 32, public_key, 33) != ZCL_OK);
    CHECK(zcl_public_key(node.secret, 32, blinding, 32, public_key, 33) == ZCL_OK);
    CHECK(public_key[33] == 0x5a);
    zcl_secure_zero(&node, sizeof(node));
    return 0;
}

static int blinding_independence(void)
{
    uint8_t secret[32] = {0}, first_blind[32] = {1}, second_blind[32] = {2};
    uint8_t first[33] = {0}, second[33] = {0};
    secret[31] = 1;
    CHECK(zcl_public_key(secret, sizeof(secret), first_blind, sizeof(first_blind), first, sizeof(first)) == ZCL_OK);
    CHECK(zcl_public_key(secret, sizeof(secret), second_blind, sizeof(second_blind), second, sizeof(second)) == ZCL_OK);
    CHECK(memcmp(first, second, sizeof(first)) == 0);
    static const uint8_t generator[33] = {
        0x02, 0x79, 0xbe, 0x66, 0x7e, 0xf9, 0xdc, 0xbb, 0xac, 0x55, 0xa0,
        0x62, 0x95, 0xce, 0x87, 0x0b, 0x07, 0x02, 0x9b, 0xfc, 0xdb, 0x2d,
        0xce, 0x28, 0xd9, 0x59, 0xf2, 0x81, 0x5b, 0x16, 0xf8, 0x17, 0x98
    };
    CHECK(memcmp(first, generator, sizeof(first)) == 0);
    return 0;
}

int main(void)
{
    if (known_vectors() || failures_preserve_output() || blinding_independence())
        return 1;
    puts("BIP32: 17 published path vectors, bounds and blinding independence passed");
    return 0;
}
