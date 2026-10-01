/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/ripemd160.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "codec check failed at line %d\n", __LINE__); return false; \
} } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1

static bool provider_vectors(void)
{
    CHECK(mbedtls_sha256_self_test(0) == 0);
    CHECK(mbedtls_sha512_self_test(0) == 0);
    CHECK(mbedtls_ripemd160_self_test(0) == 0);
    return true;
}

static bool public_reference_vectors(void)
{
    static const uint8_t expected[] = {
        0x00, 0x65, 0xa1, 0x60, 0x59, 0x86, 0x4a, 0x2f, 0xdb, 0xc7,
        0xc9, 0x9a, 0x47, 0x23, 0xa8, 0x39, 0x5b, 0xc6, 0xf1, 0x88, 0xeb
    };
    uint8_t payload[128] = {0}, text[184] = {0};
    size_t length = 0;
    CHECK(zcl_base58check_decode(TEXT("1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i"), payload, sizeof(payload), &length) == ZCL_OK);
    CHECK(length == sizeof(expected) && memcmp(payload, expected, length) == 0);
    CHECK(zcl_base58check_encode(payload, length, text, sizeof(text), &length) == ZCL_OK);
    CHECK(length == 34 && memcmp(text, "1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i", length) == 0);
    zcl_address address = {0};
    CHECK(zcl_address_parse(TEXT("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"), ZCL_MAINNET, &address) == ZCL_OK);
    CHECK(address.kind == ZCL_P2PKH && memcmp(address.hash, expected + 1, 20) == 0);
    CHECK(zcl_address_from_hash(address.hash, sizeof(address.hash), ZCL_MAINNET, text, sizeof(text), &length) == ZCL_OK);
    CHECK(length == 35 && memcmp(text, "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF", length) == 0);
    return true;
}

static bool all_lengths_roundtrip(void)
{
    for (size_t size = 1; size <= 128; ++size) {
        uint8_t payload[128] = {0}, text[186], recovered[130];
        memset(text, 0xa5, sizeof(text));
        memset(recovered, 0xa5, sizeof(recovered));
        for (size_t index = 0; index < size; ++index)
            payload[index] = (uint8_t)index;
        size_t text_len = 0, payload_len = 0;
        CHECK(zcl_base58check_encode(payload, size, text + 1, 184, &text_len) == ZCL_OK);
        CHECK(text[0] == 0xa5 && text[185] == 0xa5);
        CHECK(zcl_base58check_decode(text + 1, text_len, recovered + 1, 128, &payload_len) == ZCL_OK);
        CHECK(recovered[0] == 0xa5 && recovered[129] == 0xa5);
        CHECK(payload_len == size && memcmp(payload, recovered + 1, size) == 0);
        memset(payload, 0, sizeof(payload));
        CHECK(zcl_base58check_encode(payload, size, text + 1, 184, &text_len) == ZCL_OK);
        CHECK(zcl_base58check_decode(text + 1, text_len, recovered + 1, 128, &payload_len) == ZCL_OK);
        CHECK(payload_len == size && memcmp(payload, recovered + 1, size) == 0);
    }
    return true;
}

static bool reject_bad_arguments_and_capacity(void)
{
    uint8_t bytes[184];
    memset(bytes, 0xa5, sizeof(bytes));
    size_t length = SIZE_MAX;
    CHECK(zcl_base58check_decode(NULL, 0, bytes, sizeof(bytes), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_base58check_decode(bytes, SIZE_MAX, bytes, sizeof(bytes), &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_base58check_encode(bytes, SIZE_MAX, bytes, sizeof(bytes), &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_base58check_encode(bytes, 1, NULL, 0, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_base58check_decode(TEXT("1"), bytes, sizeof(bytes), &length) != ZCL_OK);
    CHECK(zcl_base58check_decode(TEXT("0OIl"), bytes, sizeof(bytes), &length) != ZCL_OK);
    CHECK(zcl_base58check_decode(TEXT("1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i"), bytes, 20, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(zcl_base58check_encode(TEXT("x"), bytes, 0, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == SIZE_MAX);
    for (size_t index = 0; index < sizeof(bytes); ++index)
        CHECK(bytes[index] == 0xa5);
    return true;
}

static bool check_zero_prefix(size_t size, size_t zeroes, uint8_t suffix)
{
    uint8_t payload[128] = {0}, original[128], text[186], recovered[128];
    memset(payload + zeroes, suffix, size - zeroes);
    memcpy(original, payload, sizeof(original));
    memset(text, 0xa5, sizeof(text));
    size_t length = 0, decoded = 0;
    CHECK(zcl_base58check_encode(payload, size, text + 1, 184, &length) == ZCL_OK);
    CHECK(length > zeroes && length <= 184);
    CHECK(text[0] == 0xa5 && text[length + 1] == 0xa5);
    for (size_t i = 0; i < zeroes; ++i) CHECK(text[i + 1] == '1');
    CHECK(zcl_base58check_decode(text + 1, length, recovered, sizeof(recovered), &decoded) == ZCL_OK);
    CHECK(decoded == size && memcmp(payload, recovered, size) == 0);
    uint8_t exact[184] = {0};
    size_t exact_length = 0;
    CHECK(zcl_base58check_encode(payload, size, exact, length, &exact_length) == ZCL_OK);
    CHECK(exact_length == length && memcmp(exact, text + 1, length) == 0);
    memset(text, 0xa5, sizeof(text));
    exact_length = SIZE_MAX;
    CHECK(zcl_base58check_encode(payload, size, text, length - 1, &exact_length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(exact_length == SIZE_MAX);
    for (size_t i = 0; i < sizeof(text); ++i) CHECK(text[i] == 0xa5);
    CHECK(memcmp(payload, original, sizeof(payload)) == 0);
    return true;
}

static bool all_zero_prefixes(void)
{
    for (size_t size = 1; size <= 128; ++size) {
        for (size_t zeroes = 0; zeroes <= size; ++zeroes) {
            CHECK(check_zero_prefix(size, zeroes, 1));
            CHECK(check_zero_prefix(size, zeroes, UINT8_MAX));
        }
    }
    return true;
}

static bool address_refusals_and_scripts(void)
{
    zcl_address address = {0};
    CHECK(zcl_address_parse(TEXT("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"), ZCL_TESTNET, &address) != ZCL_OK);
    CHECK(zcl_address_parse(TEXT("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"), (zcl_network)2, &address) == ZCL_UNSUPPORTED);
    CHECK(zcl_address_parse(TEXT("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"), ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_address_parse(TEXT("t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ"), ZCL_MAINNET, &address) == ZCL_OK);
    uint8_t script[25] = {0};
    size_t length = 0;
    CHECK(zcl_address_script(&address, script, sizeof(script), &length) == ZCL_OK);
    CHECK(length == 23 && script[0] == 0xa9 && script[1] == 20 && script[22] == 0x87);
    CHECK(memcmp(script + 2, address.hash, sizeof(address.hash)) == 0);
    CHECK(zcl_address_script(&address, script, 22, &length) == ZCL_BUFFER_TOO_SMALL);
    address.kind = (zcl_address_kind)0;
    CHECK(zcl_address_script(&address, script, sizeof(script), &length) == ZCL_UNSUPPORTED);
    return true;
}

int main(void)
{
    if (!provider_vectors() || !public_reference_vectors() || !all_lengths_roundtrip() ||
        !reject_bad_arguments_and_capacity() || !address_refusals_and_scripts() || !all_zero_prefixes())
        return 1;
    return puts("wallet-core: 6 codec test groups passed") == EOF ? 1 : 0;
}
