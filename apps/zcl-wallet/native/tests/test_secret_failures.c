/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <mbedtls/sha512.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Linker interposition exists only in this host test executable. It injects
 * provider failures without exposing a production fault-control interface. */
static size_t fail_at = SIZE_MAX, calls = 0, cleared_spans = 0;
static bool key_live;
static void *normalized_key;
static size_t normalizations, key_erasures;
enum { HMAC_CONTEXT, HMAC_BLOCK, HMAC_INNER, HMAC_RESULT, HMAC_SPANS };
typedef struct {
    uintptr_t address;
    size_t length, acquired, erased;
    bool live;
} hmac_span;
static hmac_span hmac_spans[HMAC_SPANS];
static bool watching_hmac, long_hmac;
static size_t hmac_hashes, hmac_updates;

int __real_mbedtls_sha512_starts(mbedtls_sha512_context *, int);
int __real_mbedtls_sha512_update(mbedtls_sha512_context *, const unsigned char *, size_t);
int __real_mbedtls_sha512_finish(mbedtls_sha512_context *, unsigned char *);
int __real_mbedtls_sha512(const unsigned char *, size_t, unsigned char *, int);
int __real_mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
int __real_mbedtls_ripemd160(const unsigned char *, size_t, unsigned char *);
void __real_mbedtls_platform_zeroize(void *, size_t);

static void acquire_hmac_span(size_t slot, const void *buffer, size_t length)
{
    if (slot >= HMAC_SPANS || buffer == NULL) abort();
    hmac_span *span = &hmac_spans[slot];
    if (span->live) {
        /* The same key block is consumed for both inner and outer hashing. */
        if (slot != HMAC_BLOCK || span->address != (uintptr_t)buffer || span->length != length)
            abort();
        return;
    }
    /* Address stamps are metadata only. Never retain/dereference a pointer
     * after the provider call; a missing clear may outlive its stack object. */
    span->address = (uintptr_t)buffer;
    span->length = length;
    span->live = true;
    ++span->acquired;
}

static void hmac_start(mbedtls_sha512_context *context)
{
    if (!watching_hmac) return;
    ++hmac_hashes;
    hmac_updates = 0;
    if (hmac_hashes > (long_hmac ? 3u : 2u)) abort();
    acquire_hmac_span(HMAC_CONTEXT, context, sizeof(*context));
}

static void hmac_update(const unsigned char *bytes, size_t length)
{
    if (!watching_hmac) return;
    if (++hmac_updates > 2) abort();
    if (long_hmac && hmac_hashes == 1) return; /* Borrowed long-key normalization. */
    if (hmac_updates == 1) {
        if (length != 128) abort();
        acquire_hmac_span(HMAC_BLOCK, bytes, length);
    } else if (hmac_hashes == (long_hmac ? 3u : 2u)) {
        if (length != 64) abort();
        acquire_hmac_span(HMAC_INNER, bytes, length);
    }
}

static void hmac_clear(void *buffer, size_t length)
{
    if (!watching_hmac) return;
    for (size_t i = 0; i < HMAC_SPANS; ++i) {
        hmac_span *span = &hmac_spans[i];
        if (!span->live || span->address != (uintptr_t)buffer) continue;
        if (length != span->length) abort();
        /* The outer observer has checked every byte of this live zeroizer
         * argument after the real clear. Retire before the object can expire. */
        span->live = false;
        span->address = 0;
        ++span->erased;
    }
}

static bool hmac_erased(void)
{
    const size_t expected[HMAC_SPANS] = {(calls + 3) / 4,
        calls >= (long_hmac ? 6u : 2u), calls >= (long_hmac ? 11u : 7u), calls / 4};
    for (size_t i = 0; i < HMAC_SPANS; ++i) {
        const hmac_span *span = &hmac_spans[i];
        if (span->live || span->acquired != expected[i] || span->erased != expected[i])
            return false;
    }
    return true;
}

static void watch_hmac(size_t key_length)
{
    if (watching_hmac) abort();
    memset(hmac_spans, 0, sizeof(hmac_spans));
    long_hmac = key_length > 128;
    hmac_hashes = hmac_updates = 0;
    watching_hmac = true;
}

static int fail_now(void)
{
    ++calls;
    return calls == fail_at;
}

int __wrap_mbedtls_sha512(const unsigned char *input, size_t length, unsigned char *output, int is384)
{
    if (key_live || input == NULL || output == NULL || length <= 128 || length > 215 || is384 != 0)
        abort();
    key_live = true;
    normalized_key = output;
    ++normalizations;
    memset(output, 0x42, 64); /* A failed provider may have partially written. */
    return fail_now() ? -1 : __real_mbedtls_sha512(input, length, output, is384);
}

int __wrap_mbedtls_sha512_starts(mbedtls_sha512_context *context, int is384)
{
    hmac_start(context);
    return fail_now() ? -1 : __real_mbedtls_sha512_starts(context, is384);
}

int __wrap_mbedtls_sha512_update(mbedtls_sha512_context *context,
                                const unsigned char *bytes, size_t length)
{
    hmac_update(bytes, length);
    return fail_now() ? -1 : __real_mbedtls_sha512_update(context, bytes, length);
}

int __wrap_mbedtls_sha512_finish(mbedtls_sha512_context *context, unsigned char *output)
{
    if (output == NULL) abort();
    if (watching_hmac) acquire_hmac_span(HMAC_RESULT, output, 64);
    memset(output, 0x42, 64); /* Failure may follow a partial secret write. */
    return fail_now() ? -1 : __real_mbedtls_sha512_finish(context, output);
}

int __wrap_mbedtls_sha256(const unsigned char *input, size_t length,
                         unsigned char *output, int is224)
{
    return fail_now() ? -1 : __real_mbedtls_sha256(input, length, output, is224);
}

int __wrap_mbedtls_ripemd160(const unsigned char *input, size_t length, unsigned char *output)
{
    return fail_now() ? -1 : __real_mbedtls_ripemd160(input, length, output);
}

void __wrap_mbedtls_platform_zeroize(void *buffer, size_t length)
{
    __real_mbedtls_platform_zeroize(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0)
            abort();
    }
    hmac_clear(buffer, length);
    if (key_live && buffer == normalized_key) {
        if (length != 64) abort();
        key_live = false;
        normalized_key = NULL; /* Retire the pointer while its object is live. */
        ++key_erasures;
    }
    ++cleared_spans;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "secret failure check failed at line %d\n", __LINE__); return 1; } } while (0)

static void inject(size_t at)
{
    if (key_live) abort();
    calls = 0;
    cleared_spans = 0;
    fail_at = at;
    normalizations = key_erasures = 0;
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
            watch_hmac(key_size);
            CHECK(zcl_hmac_sha512(key, key_size, input, sizeof(input), output, sizeof(output)) == ZCL_CRYPTO_FAILURE);
            CHECK(calls == fail && cleared_spans >= 4);
            CHECK(memcmp(output, before, sizeof(output)) == 0);
            CHECK(hmac_erased());
            watching_hmac = false;
        }
        inject(SIZE_MAX);
        watch_hmac(key_size);
        CHECK(zcl_hmac_sha512(key, key_size, input, sizeof(input), output, sizeof(output)) == ZCL_OK);
        CHECK(calls == steps && hmac_erased());
        watching_hmac = false;
        memcpy(output, before, sizeof(output));
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
    CHECK(normalizations == 0 && !key_live);
    return 0;
}

static int long_seed_failures(void)
{
    uint8_t entropy[32], text[215], output[64], before[64];
    const uint8_t empty[] = {0};
    memset(entropy, 0x42, sizeof(entropy));
    memset(before, 0xa5, sizeof(before));
    size_t length = 0;
    inject(SIZE_MAX);
    CHECK(zcl_mnemonic_encode(entropy, sizeof(entropy), text, sizeof(text), &length) == ZCL_OK);
    CHECK(length > 128);
    const size_t points[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 101, 8193, 16385, 16386};
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        memcpy(output, before, sizeof(output));
        inject(points[i]);
        CHECK(zcl_mnemonic_seed(text, length, empty, 0, output, sizeof(output)) == ZCL_CRYPTO_FAILURE);
        CHECK(calls == points[i] && !key_live);
        CHECK(normalizations == (points[i] == 1 ? 0u : 1u) && key_erasures == normalizations);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    inject(SIZE_MAX);
    CHECK(zcl_mnemonic_seed(text, length, empty, 0, output, sizeof(output)) == ZCL_OK);
    CHECK(calls == 16386 && normalizations == 1 && key_erasures == 1 && !key_live);
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(text, sizeof(text));
    zcl_secure_zero(output, sizeof(output));
    return 0;
}

static int address_provider_failures(void)
{
    typedef zcl_status (*address_function)(const uint8_t *, size_t, zcl_network, uint32_t,
        const uint8_t *, size_t, uint8_t *, size_t, size_t *);
    const address_function functions[] = {zcl_receive_from_entropy, zcl_change_from_entropy};
    uint8_t entropy[16] = {0}, blinding[32] = {1}, output[35], before[35];
    memset(before, 0xa5, sizeof(before));
    for (size_t chain = 0; chain < 2; ++chain) {
        size_t length = 0;
        inject(SIZE_MAX);
        CHECK(functions[chain](entropy, sizeof(entropy), ZCL_MAINNET, 19, blinding, 32,
            output, sizeof(output), &length) == ZCL_OK && length == 35);
        const size_t total = calls;
        CHECK(total > 16385 && cleared_spans > 12000);
        const size_t points[] = {1, 2, 3, 4, 10, 101, 8193, 16385, 16386,
            total - 4, total - 3, total - 2, total - 1, total};
        for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
            memcpy(output, before, sizeof(output));
            length = 777;
            inject(points[i]);
            CHECK(functions[chain](entropy, sizeof(entropy), ZCL_MAINNET, 19, blinding, 32,
                output, sizeof(output), &length) == ZCL_CRYPTO_FAILURE);
            CHECK(calls == points[i] && cleared_spans >= 5 && length == 777);
            CHECK(memcmp(output, before, sizeof(output)) == 0);
        }
    }
    return 0;
}

int main(void)
{
    if (hmac_failures() || mnemonic_failures() || long_seed_failures() || address_provider_failures())
        return 1;
    puts("secret failures: provider errors preserve output and cleanup clears storage");
    return 0;
}
