/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent host backend: no app normalization, padding or hash helper
 * computes the expected MAC. OpenSSL never enters the Android build. */
static bool compare_hmac(const uint8_t *key, size_t key_len,
                          const uint8_t *data, size_t data_len)
{
    if (key == NULL || data == NULL || key_len > 256 || data_len > 512) abort();
    uint8_t key_before[256] = {0}, data_before[512] = {0};
    uint8_t expected[66], actual[66];
    memcpy(key_before, key, key_len);
    memcpy(data_before, data, data_len);
    memset(expected, 0xa5, sizeof(expected));
    memset(actual, 0xa5, sizeof(actual));
    unsigned int length = 0;
    bool okay = HMAC(EVP_sha512(), key, (int)key_len, data, data_len,
        expected + 1, &length) == expected + 1 && length == 64;
    if (okay) {
        okay = zcl_hmac_sha512(key, key_len, data, data_len, actual + 1, 64) == ZCL_OK;
        okay = okay && memcmp(expected, actual, sizeof(actual)) == 0;
        okay = okay && memcmp(key_before, key, key_len) == 0;
        okay = okay && memcmp(data_before, data, data_len) == 0;
    }
    OPENSSL_cleanse(key_before, sizeof(key_before));
    OPENSSL_cleanse(data_before, sizeof(data_before));
    OPENSSL_cleanse(expected, sizeof(expected));
    OPENSSL_cleanse(actual, sizeof(actual));
    return okay;
}

#if defined(ZCL_HMAC_FUZZ)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == NULL || size < 2 || size > 770) return 0;
    const size_t key_len = (size_t)data[0] + (size_t)(data[1] & 1u);
    if (key_len > size - 2) return 0;
    const size_t data_len = size - 2 - key_len;
    if (data_len > 512) return 0;
    if (!compare_hmac(data + 2, key_len, data + 2 + key_len, data_len)) abort();
    return 0;
}
#else
/* Only the fixed <=512-byte fixtures below call this helper. */
static void fill_pattern(uint8_t *bytes, size_t length, unsigned pattern, size_t salt)
{
    for (size_t i = 0; i < length; ++i)
        bytes[i] = pattern == 0 ? 0 : pattern == 1 ? UINT8_MAX : (uint8_t)((i * 73 + salt) & 255);
}

static bool boundary_matrix(uint8_t *key, size_t key_capacity, uint8_t *data, size_t data_capacity)
{
    if (key_capacity != 256 || data_capacity != 512) abort();
    const size_t lengths[] = {0, 1, 127, 128, 129, 255, 256, 511, 512};
    for (unsigned pattern = 0; pattern < 3; ++pattern) {
        fill_pattern(key, key_capacity, pattern, 17);
        fill_pattern(data, data_capacity, pattern, 93);
        for (size_t key_len = 0; key_len <= key_capacity; ++key_len) {
            for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
                if (!compare_hmac(key, key_len, data, lengths[i])) return false;
            }
        }
    }
    return true;
}

int main(void)
{
    uint8_t key[256], data[512];
    const bool okay = boundary_matrix(key, sizeof(key), data, sizeof(data));
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(data, sizeof(data));
    if (!okay) {
        fputs("HMAC oracle output/input/guard mismatch\n", stderr);
        return 1;
    }
    puts("HMAC: 6939 independent binary-key/message comparisons and input/output guards passed");
    return 0;
}
#endif
