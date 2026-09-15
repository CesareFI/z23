/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "bip32_oracle.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

typedef struct { uint8_t before[8]; zcl_extended_private node; uint8_t after[8]; } guarded_node;

static void compare(const guarded_node *actual, const guarded_node *expected, zcl_status got, zcl_status wanted)
{
    if (got != wanted || memcmp(actual, expected, sizeof(*actual)) != 0) abort();
    for (size_t i = 0; i < 8; ++i)
        if (actual->before[i] != 0xa5 || actual->after[i] != 0xa5) abort();
}

static bool master(const uint8_t *seed, size_t length, uint8_t control, zcl_extended_private *result)
{
    guarded_node actual, expected;
    uint8_t original[64];
    memcpy(original, seed, sizeof(original));
    memset(&actual, 0xa5, sizeof(actual)); memset(&expected, 0xa5, sizeof(expected));
    const uint8_t *input = (control & 1U) != 0 ? NULL : seed;
    const bool missing = (control & 2U) != 0;
    const zcl_status wanted = zcl_test_bip32_master(input, length, missing ? NULL : &expected.node);
    const zcl_status got = zcl_bip32_master(input, length, missing ? NULL : &actual.node);
    compare(&actual, &expected, got, wanted);
    if (memcmp(seed, original, sizeof(original)) != 0) abort();
    if (got == ZCL_OK) *result = actual.node;
    zcl_secure_zero(original, sizeof(original));
    zcl_secure_zero(&actual, sizeof(actual)); zcl_secure_zero(&expected, sizeof(expected));
    return got == ZCL_OK;
}

static bool child(const zcl_extended_private *parent, uint32_t index, uint8_t control,
    const uint8_t *blinding, size_t blinding_len, zcl_extended_private *result)
{
    guarded_node actual, expected;
    zcl_extended_private original = *parent;
    uint8_t original_blinding[32] = {0};
    if (blinding != NULL) memcpy(original_blinding, blinding, sizeof(original_blinding));
    memset(&actual, 0xa5, sizeof(actual)); memset(&expected, 0xa5, sizeof(expected));
    const zcl_extended_private *input = (control & 1U) != 0 ? NULL : parent;
    const bool missing = (control & 2U) != 0;
    zcl_status wanted = ZCL_INVALID_ARGUMENT;
    if (blinding != NULL && blinding_len == 32)
        wanted = zcl_test_bip32_child(input, index, missing ? NULL : &expected.node);
    const zcl_status got = zcl_bip32_child(input, index, blinding, blinding_len, missing ? NULL : &actual.node);
    compare(&actual, &expected, got, wanted);
    if (memcmp(parent, &original, sizeof(original)) != 0) abort();
    if (blinding != NULL && memcmp(blinding, original_blinding, sizeof(original_blinding)) != 0) abort();
    if (got == ZCL_OK) *result = actual.node;
    zcl_secure_zero(&original, sizeof(original));
    zcl_secure_zero(original_blinding, sizeof(original_blinding));
    zcl_secure_zero(&actual, sizeof(actual)); zcl_secure_zero(&expected, sizeof(expected));
    return got == ZCL_OK;
}

static uint32_t index_bytes(const uint8_t *data)
{
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3];
}

static void follow_path(zcl_extended_private *node, const uint8_t *data)
{
    uint8_t blinding[32] = {1}, other_blinding[32] = {2};
    uint8_t actual[33] = {0}, expected[33] = {0}, repeated[33] = {0};
    zcl_extended_private next = {0};
    const size_t depth = (size_t)(data[2] % 6);
    for (size_t i = 0; i < depth; ++i) {
        if (!child(node, index_bytes(data + 4 + i * 4), 0, blinding, 32, &next)) goto cleanup;
        *node = next;
        zcl_secure_zero(&next, sizeof(next));
    }
    if (!zcl_test_bip32_public(node->secret, 32, expected, sizeof(expected))) abort();
    if (zcl_public_key(node->secret, 32, blinding, 32, actual, sizeof(actual)) != ZCL_OK) abort();
    if (zcl_public_key(node->secret, 32, other_blinding, 32, repeated, sizeof(repeated)) != ZCL_OK) abort();
    if (memcmp(actual, expected, 33) != 0 || memcmp(actual, repeated, 33) != 0) abort();
cleanup:
    zcl_secure_zero(&next, sizeof(next));
    zcl_secure_zero(blinding, sizeof(blinding)); zcl_secure_zero(other_blinding, sizeof(other_blinding));
    zcl_secure_zero(actual, sizeof(actual)); zcl_secure_zero(expected, sizeof(expected));
    zcl_secure_zero(repeated, sizeof(repeated));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 104) return 0;
    uint8_t expanded[104];
    for (size_t i = 0; i < sizeof(expanded); ++i) expanded[i] = data[i % size];
    zcl_extended_private node = {0}, parent = {0}, result = {0};
    const size_t seed_len = data[0] == 255 ? SIZE_MAX : (size_t)(data[0] % 67);
    if (master(expanded + 8, seed_len, expanded[1], &node)) follow_path(&node, expanded);
    memcpy(parent.secret, expanded + 8, 32); memcpy(parent.chain_code, expanded + 40, 32);
    const size_t blinding_len = expanded[3] == 255 ? SIZE_MAX : (expanded[3] & 1U) != 0 ? 31 : 32;
    const uint8_t *blinding = (expanded[3] & 2U) != 0 ? NULL : expanded + 72;
    (void)child(&parent, index_bytes(expanded + 4), expanded[2], blinding, blinding_len, &result);
    zcl_secure_zero(&node, sizeof(node)); zcl_secure_zero(&parent, sizeof(parent));
    zcl_secure_zero(&result, sizeof(result)); zcl_secure_zero(expanded, sizeof(expanded));
    return 0;
}
