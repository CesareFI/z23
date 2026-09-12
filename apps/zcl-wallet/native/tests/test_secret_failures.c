/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <mbedtls/sha512.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Linker interposition exists only in this host test executable. It injects
 * provider failures without exposing a production fault-control interface. */
static size_t fail_at = SIZE_MAX, calls = 0, cleared_spans = 0;

int __real_mbedtls_sha512_starts(mbedtls_sha512_context *, int);
int __real_mbedtls_sha512_update(mbedtls_sha512_context *, const unsigned char *, size_t);
int __real_mbedtls_sha512_finish(mbedtls_sha512_context *, unsigned char *);
int __real_mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
void __real_mbedtls_platform_zeroize(void *, size_t);

static int fail_now(void)
{
    ++calls;
    return calls == fail_at;
}

int __wrap_mbedtls_sha512_starts(mbedtls_sha512_context *context, int is384)
{
    return fail_now() ? -1 : __real_mbedtls_sha512_starts(context, is384);
}

int __wrap_mbedtls_sha512_update(mbedtls_sha512_context *context,
                                const unsigned char *bytes, size_t length)
{
    return fail_now() ? -1 : __real_mbedtls_sha512_update(context, bytes, length);
}

int __wrap_mbedtls_sha512_finish(mbedtls_sha512_context *context, unsigned char *output)
{
    return fail_now() ? -1 : __real_mbedtls_sha512_finish(context, output);
}

int __wrap_mbedtls_sha256(const unsigned char *input, size_t length,
                         unsigned char *output, int is224)
{
    return fail_now() ? -1 : __real_mbedtls_sha256(input, length, output, is224);
}

void __wrap_mbedtls_platform_zeroize(void *buffer, size_t length)
{
    __real_mbedtls_platform_zeroize(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0)
            abort();
    }
    ++cleared_spans;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "secret failure check failed at line %d\n", __LINE__); return 1; } } while (0)

static void inject(size_t at)
{
    calls = 0;
    cleared_spans = 0;
    fail_at = at;
}

static int hmac_failures(void)
{
    uint8_t key[131] = {0}, input[64] = {0}, output[64] = {0}, before[64] = {0};
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    for (size_t key_size = 32; key_size <= 131; key_size += 99) {
        size_t steps = key_size == 32 ? 8 : 12;
        for (size_t fail = 1; fail <= steps; ++fail) {
            inject(fail);
            CHECK(zcl_hmac_sha512(key, key_size, input, sizeof(input), output, sizeof(output)) == ZCL_CRYPTO_FAILURE);
            CHECK(calls == fail && cleared_spans >= 4);
            CHECK(memcmp(output, before, sizeof(output)) == 0);
        }
    }
    return 0;
}

static int mnemonic_failures(void)
{
    static const uint8_t text[] = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    uint8_t entropy[16] = {0}, output[215] = {0}, before[215] = {0};
    memset(output, 0x5a, sizeof(output));
    memcpy(before, output, sizeof(before));
    size_t length = 999;
    inject(1);
    CHECK(zcl_mnemonic_encode(entropy, sizeof(entropy), output, sizeof(output), &length) == ZCL_CRYPTO_FAILURE);
    CHECK(length == 999 && cleared_spans >= 3 && memcmp(output, before, sizeof(output)) == 0);
    inject(1);
    CHECK(zcl_mnemonic_decode(text, sizeof(text) - 1, output, sizeof(output), &length) == ZCL_CRYPTO_FAILURE);
    CHECK(length == 999 && cleared_spans >= 2 && memcmp(output, before, sizeof(output)) == 0);
    inject(1);
    CHECK(zcl_mnemonic_confirm(entropy, sizeof(entropy), text, sizeof(text) - 1) == ZCL_CRYPTO_FAILURE);
    CHECK(calls == 1 && cleared_spans >= 3);
    static const size_t points[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 101, 8193, 16385};
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        inject(points[i]);
        CHECK(zcl_mnemonic_seed(text, sizeof(text) - 1, entropy, 0, output, sizeof(output)) == ZCL_CRYPTO_FAILURE);
        CHECK(calls == points[i] && cleared_spans >= 3);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    inject(SIZE_MAX);
    CHECK(zcl_mnemonic_seed(text, sizeof(text) - 1, entropy, 0, output, sizeof(output)) == ZCL_OK);
    CHECK(calls == 16385 && cleared_spans > 12000);
    return 0;
}

int main(void)
{
    if (hmac_failures() || mnemonic_failures())
        return 1;
    puts("secret failures: provider errors preserve output and cleanup clears storage");
    return 0;
}
