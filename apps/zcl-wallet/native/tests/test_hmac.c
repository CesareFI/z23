/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <stdio.h>
#include <string.h>

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

static int invalid_bounds(void)
{
    uint8_t input[512] = {0}, result[66] = {0}, before[66] = {0};
    memset(result, 0xa5, sizeof(result));
    memcpy(before, result, sizeof(before));
    CHECK(zcl_hmac_sha512(NULL, 0, input, 0, result, 64) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_hmac_sha512(input, SIZE_MAX, input, 0, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_hmac_sha512(input, 0, input, SIZE_MAX, result, 64) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_hmac_sha512(input, 257, input, 0, result, 64) == ZCL_OUT_OF_RANGE);
    for (size_t capacity = 0; capacity < 64; ++capacity) {
        CHECK(zcl_hmac_sha512(input, 128, input, 512, result + 1, capacity) == ZCL_BUFFER_TOO_SMALL);
        CHECK(memcmp(result, before, sizeof(result)) == 0);
    }
    CHECK(zcl_hmac_sha512(input, 256, input, 512, result + 1, 64) == ZCL_OK);
    CHECK(result[0] == 0xa5 && result[65] == 0xa5);
    return 0;
}

int main(void)
{
    if (short_keys() || long_keys() || invalid_bounds())
        return 1;
    puts("HMAC: RFC 4231 cases 1/2/6/7 and bounds passed");
    return 0;
}
