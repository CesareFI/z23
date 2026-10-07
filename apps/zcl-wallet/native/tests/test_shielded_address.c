/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_shielded_address.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "shielded_address_vectors.h"

static void refused(const uint8_t *text, size_t length, zcl_network network)
{
    zcl_shielded_address address, sentinel;
    memset(&sentinel, 0xa5, sizeof(sentinel));
    memcpy(&address, &sentinel, sizeof(address));
    assert(zcl_shielded_address_parse(text, length, network, &address) != ZCL_OK);
    assert(memcmp(&address, &sentinel, sizeof(address)) == 0);
}

static void expected_bytes(const zcl_shielded_address *address, unsigned pattern)
{
    const size_t count = address->kind == ZCL_SPROUT_ADDRESS ? 64 : 43;
    for (size_t index = 0; index < count; ++index) {
        uint8_t expected = (uint8_t)index;
        if (pattern == 0) expected = 0;
        if (pattern == 1) expected = 255;
        assert(address->bytes[index] == expected);
    }
    for (size_t index = count; index < sizeof(address->bytes); ++index)
        assert(address->bytes[index] == 0);
}

static void reference_vectors(void)
{
    for (size_t row = 0; row < sizeof(shielded_vectors) / sizeof(shielded_vectors[0]); ++row) {
        const uint8_t *text = (const uint8_t *)shielded_vectors[row].text;
        size_t length = strlen(shielded_vectors[row].text);
        if (!shielded_vectors[row].valid) {
            refused(text, length, shielded_vectors[row].network);
            continue;
        }
        struct { uint64_t before; zcl_shielded_address value; uint64_t after; } guarded;
        memset(&guarded, 0xa5, sizeof(guarded));
        assert(zcl_shielded_address_parse(text, length, shielded_vectors[row].network, &guarded.value) == ZCL_OK);
        assert(guarded.before == UINT64_C(0xa5a5a5a5a5a5a5a5));
        assert(guarded.after == UINT64_C(0xa5a5a5a5a5a5a5a5));
        assert(guarded.value.network == shielded_vectors[row].network);
        assert(guarded.value.kind == shielded_vectors[row].kind);
        expected_bytes(&guarded.value, shielded_vectors[row].pattern);
        const zcl_network other = guarded.value.network == ZCL_MAINNET ? ZCL_TESTNET : ZCL_MAINNET;
        refused(text, length, other);
    }
}

static void sapling_case(void)
{
    for (size_t row = 0; row < sizeof(shielded_vectors) / sizeof(shielded_vectors[0]); ++row) {
        if (!shielded_vectors[row].valid || shielded_vectors[row].kind != ZCL_SAPLING_ADDRESS) continue;
        uint8_t upper[96] = {0};
        const size_t length = strlen(shielded_vectors[row].text);
        memcpy(upper, shielded_vectors[row].text, length);
        for (size_t index = 0; index < length; ++index) {
            if (upper[index] >= 'a' && upper[index] <= 'z')
                upper[index] = (uint8_t)(upper[index] - ('a' - 'A'));
        }
        zcl_shielded_address address = {0};
        assert(zcl_shielded_address_parse(upper, length, shielded_vectors[row].network, &address) == ZCL_OK);
        expected_bytes(&address, shielded_vectors[row].pattern);
        upper[0] = 'z';
        refused(upper, length, shielded_vectors[row].network);
    }
}

static void altered_characters(void)
{
    static const uint8_t invalid[] = {0, ' ', '\n', '\t', 127, 128, 255};
    for (size_t row = 0; row < sizeof(shielded_vectors) / sizeof(shielded_vectors[0]); ++row) {
        if (!shielded_vectors[row].valid) continue;
        const size_t length = strlen(shielded_vectors[row].text);
        uint8_t text[96] = {0};
        memcpy(text, shielded_vectors[row].text, length);
        for (size_t index = 0; index < length; ++index) {
            const uint8_t saved = text[index];
            text[index] = saved == 'q' ? 'p' : 'q';
            refused(text, length, shielded_vectors[row].network);
            for (size_t byte = 0; byte < sizeof(invalid); ++byte) {
                text[index] = invalid[byte];
                refused(text, length, shielded_vectors[row].network);
            }
            text[index] = saved;
        }
    }
}

static void lengths_and_arguments(void)
{
    const uint8_t one = 0;
    refused(NULL, 0, ZCL_MAINNET);
    refused(&one, SIZE_MAX, ZCL_MAINNET);
    refused(&one, 0, ZCL_MAINNET);
    refused(&one, 1, ZCL_MAINNET);
    refused(&one, SIZE_MAX, (zcl_network)2);
    zcl_shielded_address output = {0};
    assert(zcl_shielded_address_parse(&one, 0, ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    assert(zcl_shielded_address_parse(NULL, 0, ZCL_MAINNET, &output) == ZCL_INVALID_ARGUMENT);
    for (size_t row = 0; row < sizeof(shielded_vectors) / sizeof(shielded_vectors[0]); ++row) {
        const size_t length = strlen(shielded_vectors[row].text);
        uint8_t text[96] = {0};
        memcpy(text, shielded_vectors[row].text, length);
        for (size_t prefix = 0; prefix < length; ++prefix)
            refused(text, prefix, shielded_vectors[row].network);
        refused(text, length + 1, shielded_vectors[row].network);
        refused(text, length, (zcl_network)-1);
    }
}

int main(void)
{
    reference_vectors();
    sapling_case();
    altered_characters();
    lengths_and_arguments();
    return puts("shielded public envelope: 21 reference fixtures and boundary refusals passed") == EOF ? 1 : 0;
}
