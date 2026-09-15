/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <stdio.h>
#include <string.h>
#if defined(ZCL_HMAC_ORACLE)
#include <openssl/evp.h>
#endif

typedef zcl_status (*bounded_prf)(const uint8_t *, size_t, const uint8_t *, size_t,
    uint8_t *, size_t);

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "HMAC check failed at line %d\n", __LINE__); return 1; } } while (0)

static int matches_hex(const uint8_t *bytes, size_t size, const char *hex, size_t length)
{
    static const char digits[] = "0123456789abcdef";
    if (size > SIZE_MAX / 2 || length != size * 2)
        return 0;
    for (size_t i = 0; i < size; ++i) {
        if (digits[bytes[i] >> 4] != hex[i * 2] || digits[bytes[i] & 15] != hex[i * 2 + 1])
            return 0;
    }
    return 1;
}

static int short_keys(void)
{
    uint8_t key[20] = {0}, result[64] = {0};
    memset(key, 0x0b, sizeof(key));
    static const uint8_t first[] = "Hi There";
    static const char first_hex[] =
        "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
        "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854";
    CHECK(zcl_hmac_sha512(key, sizeof(key), first, sizeof(first) - 1, result, sizeof(result)) == ZCL_OK);
    CHECK(matches_hex(result, sizeof(result), first_hex, sizeof(first_hex) - 1));
    static const uint8_t second_key[] = "Jefe", second[] = "what do ya want for nothing?";
    static const char second_hex[] =
        "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
        "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737";
    CHECK(zcl_hmac_sha512(second_key, sizeof(second_key) - 1, second, sizeof(second) - 1,
                          result, sizeof(result)) == ZCL_OK);
    CHECK(matches_hex(result, sizeof(result), second_hex, sizeof(second_hex) - 1));
    return 0;
}

static int long_keys(void)
{
    uint8_t key[131] = {0}, result[64] = {0};
    memset(key, 0xaa, sizeof(key));
    static const uint8_t first[] = "Test Using Larger Than Block-Size Key - Hash Key First";
    static const char first_hex[] =
        "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
        "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598";
    CHECK(zcl_hmac_sha512(key, sizeof(key), first, sizeof(first) - 1, result, sizeof(result)) == ZCL_OK);
    CHECK(matches_hex(result, sizeof(result), first_hex, sizeof(first_hex) - 1));
    static const uint8_t second[] = "This is a test using a larger than block-size key and a larger than block-size data. "
        "The key needs to be hashed before being used by the HMAC algorithm.";
    static const char second_hex[] =
        "e37b6a775dc87dbaa4dfa9f96e5e3ffd"
        "debd71f8867289865df5a32d20cdc944"
        "b6022cac3c4982b10d5eeb55c3e4de15"
        "134676fb6de0446065c97440fa8c6a58";
    CHECK(zcl_hmac_sha512(key, sizeof(key), second, sizeof(second) - 1, result, sizeof(result)) == ZCL_OK);
    CHECK(matches_hex(result, sizeof(result), second_hex, sizeof(second_hex) - 1));
    return 0;
}

static int invalid_bounds(bounded_prf function)
{
    uint8_t input[512] = {0}, result[66] = {0}, before[66] = {0};
    memset(result, 0xa5, sizeof(result));
    memcpy(before, result, sizeof(before));
    CHECK(function(NULL, 0, input, 0, result, 64) == ZCL_INVALID_ARGUMENT);
    CHECK(function(input, 0, NULL, 0, result, 64) == ZCL_INVALID_ARGUMENT);
    CHECK(function(input, 0, input, 0, NULL, 64) == ZCL_INVALID_ARGUMENT);
    CHECK(function(input, SIZE_MAX, input, 0, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(function(input, 0, input, SIZE_MAX, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(function(input, 257, input, 0, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(function(input, 0, input, 513, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(memcmp(result, before, sizeof(result)) == 0);
    for (size_t capacity = 0; capacity < 64; ++capacity) {
        CHECK(function(input, 128, input, 512, result + 1, capacity) == ZCL_BUFFER_TOO_SMALL);
        CHECK(memcmp(result, before, sizeof(result)) == 0);
    }
    CHECK(function(input, 256, input, 512, result + 1, 64) == ZCL_OK);
    CHECK(result[0] == 0xa5 && result[65] == 0xa5);
    return 0;
}

#if defined(ZCL_HMAC_ORACLE)
static int oracle_case(size_t key_length, size_t salt_length)
{
    uint8_t key[256], salt[512], actual[66], expected[64];
    CHECK(key_length <= sizeof(key) && salt_length <= sizeof(salt) - 4);
    for (size_t i = 0; i < sizeof(key); ++i)
        key[i] = (uint8_t)(i ^ UINT8_C(0xa5));
    for (size_t i = 0; i < salt_length; ++i)
        salt[i] = (uint8_t)(i & UINT8_C(0xff));
    salt[salt_length] = 0;
    salt[salt_length + 1] = 0;
    salt[salt_length + 2] = 0;
    salt[salt_length + 3] = 1;
    memset(actual, 0xa5, sizeof(actual));
    CHECK(PKCS5_PBKDF2_HMAC((const char *)key, (int)key_length, salt, (int)salt_length,
        2048, EVP_sha512(), 64, expected) == 1);
    CHECK(zcl_pbkdf2_sha512_block(key, key_length, salt, salt_length + 4,
        actual + 1, 64) == ZCL_OK);
    CHECK(actual[0] == 0xa5 && actual[65] == 0xa5);
    CHECK(memcmp(actual + 1, expected, sizeof(expected)) == 0);
    return 0;
}

static int pbkdf2_oracle(void)
{
    static const size_t key_lengths[] = {0, 1, 127, 128, 129, 215, 256};
    static const size_t salt_lengths[] = {0, 1, 111, 112, 127, 128, 129, 256, 508};
    for (size_t i = 0; i < sizeof(key_lengths) / sizeof(key_lengths[0]); ++i) {
        for (size_t j = 0; j < sizeof(salt_lengths) / sizeof(salt_lengths[0]); ++j)
            CHECK(oracle_case(key_lengths[i], salt_lengths[j]) == 0);
    }
    puts("PBKDF2: 63 independent OpenSSL key/salt boundary comparisons passed");
    return 0;
}
#endif

int main(void)
{
    if (short_keys() || long_keys() || invalid_bounds(zcl_hmac_sha512) ||
        invalid_bounds(zcl_pbkdf2_sha512_block))
        return 1;
#if defined(ZCL_HMAC_ORACLE)
    if (pbkdf2_oracle())
        return 1;
#endif
    puts("HMAC: RFC 4231 cases 1/2/6/7 and bounds passed");
    return 0;
}
