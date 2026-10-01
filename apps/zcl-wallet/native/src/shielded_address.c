/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_shielded_address.h"

#include <string.h>

/* Original Zclassic key_io.cpp: 64 Sprout bytes; 43 Sapling bytes encoded as
 * 69 five-bit groups, then six Bech32 (not Bech32m) checksum symbols.
 * See docs/SHIELDED_ADDRESS.md for the pinned reference and scope. */
enum { SPROUT_TEXT = 95, SAPLING_GROUPS = 69, CHECKED_GROUPS = 75 };

static zcl_status sprout_parse(const uint8_t *text, zcl_shielded_address *address)
{
    uint8_t payload[66] = {0};
    size_t length = 0;
    zcl_status status = zcl_base58check_decode(text, SPROUT_TEXT, payload, sizeof(payload), &length);
    if (status != ZCL_OK) return status;
    if (length != sizeof(payload)) return ZCL_INVALID_ENCODING;
    const uint8_t suffix = address->network == ZCL_MAINNET ? 0x9a : 0xb6;
    if (payload[0] != 0x16 || payload[1] != suffix) return ZCL_UNSUPPORTED;
    address->kind = ZCL_SPROUT_ADDRESS;
    memcpy(address->bytes, payload + 2, sizeof(address->bytes));
    return ZCL_OK;
}

static bool lowercase(const uint8_t *text, size_t length, uint8_t *normalized)
{
    bool lower = false, upper = false;
    for (size_t index = 0; index < length; ++index) {
        uint8_t byte = text[index];
        if (byte < 33 || byte > 126) return false;
        if (byte >= 'a' && byte <= 'z') lower = true;
        if (byte >= 'A' && byte <= 'Z') {
            upper = true;
            byte = (uint8_t)(byte + ('a' - 'A'));
        }
        normalized[index] = byte;
    }
    return !(lower && upper);
}

/* BIP 173 / original Zclassic bech32.cpp polynomial. The state is 30 bits;
 * masking before the shift keeps every intermediate within uint32_t. */
static uint32_t polymod(uint32_t state, uint8_t value)
{
    static const uint32_t generators[5] = {
        UINT32_C(0x3b6a57b2), UINT32_C(0x26508e6d), UINT32_C(0x1ea119fa),
        UINT32_C(0x3d4233dd), UINT32_C(0x2a1462b3)
    };
    const uint32_t top = state >> 25;
    state = ((state & UINT32_C(0x1ffffff)) << 5) ^ value;
    for (size_t bit = 0; bit < 5; ++bit) {
        if (((top >> bit) & UINT32_C(1)) != 0) state ^= generators[bit];
    }
    return state;
}

static uint32_t prefix_checksum(const uint8_t *prefix, size_t length)
{
    uint32_t state = 1;
    for (size_t index = 0; index < length; ++index)
        state = polymod(state, (uint8_t)(prefix[index] >> 5));
    state = polymod(state, 0);
    for (size_t index = 0; index < length; ++index)
        state = polymod(state, (uint8_t)(prefix[index] & 31));
    return state;
}

static int symbol_value(uint8_t symbol)
{
    static const uint8_t alphabet[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
    for (size_t index = 0; index < 32; ++index) {
        if (alphabet[index] == symbol) return (int)index;
    }
    return -1;
}

static bool checked_groups(const uint8_t *text, uint32_t state, uint8_t *groups)
{
    for (size_t index = 0; index < CHECKED_GROUPS; ++index) {
        const int value = symbol_value(text[index]);
        if (value < 0) return false;
        groups[index] = (uint8_t)value;
        state = polymod(state, groups[index]);
    }
    return state == 1;
}

/* Exact 344-bit conversion, reading only groups 0..68. The 345th padding bit
 * has already been checked. No variable-length accumulator or allocation. */
static void unpack_sapling(const uint8_t *groups, uint8_t *bytes)
{
    for (size_t index = 0; index < 43; ++index) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            const size_t position = index * 8 + bit;
            const unsigned value = (unsigned)groups[position / 5] >> (4 - position % 5);
            byte = (uint8_t)(((unsigned)byte << 1) | (value & 1U));
        }
        bytes[index] = byte;
    }
}

static zcl_status sapling_parse(const uint8_t *text, size_t length, zcl_shielded_address *address)
{
    static const char main_prefix[] = "zs", test_prefix[] = "ztestsapling";
    const char *prefix = address->network == ZCL_MAINNET ? main_prefix : test_prefix;
    const size_t prefix_len = address->network == ZCL_MAINNET ? sizeof(main_prefix) - 1 : sizeof(test_prefix) - 1;
    if (length != prefix_len + 1 + CHECKED_GROUPS) return ZCL_INVALID_ENCODING;
    uint8_t normalized[sizeof(test_prefix) + CHECKED_GROUPS] = {0}, groups[CHECKED_GROUPS] = {0};
    if (!lowercase(text, length, normalized)) return ZCL_INVALID_ENCODING;
    if (memcmp(normalized, prefix, prefix_len) != 0 || normalized[prefix_len] != '1')
        return ZCL_UNSUPPORTED;
    if (!checked_groups(normalized + prefix_len + 1, prefix_checksum(normalized, prefix_len), groups))
        return ZCL_INVALID_ENCODING;
    if ((groups[SAPLING_GROUPS - 1] & 1U) != 0) return ZCL_INVALID_ENCODING;
    address->kind = ZCL_SAPLING_ADDRESS;
    unpack_sapling(groups, address->bytes);
    return ZCL_OK;
}

zcl_status zcl_shielded_address_parse(const uint8_t *text, size_t text_len,
                                    zcl_network network, zcl_shielded_address *address)
{
    if (text == NULL || address == NULL) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    zcl_shielded_address candidate = {0};
    candidate.network = network;
    zcl_status status = text_len == SPROUT_TEXT
        ? sprout_parse(text, &candidate) : sapling_parse(text, text_len, &candidate);
    if (status != ZCL_OK) return status;
    *address = candidate;
    return ZCL_OK;
}
