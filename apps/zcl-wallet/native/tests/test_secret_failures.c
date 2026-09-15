/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <mbedtls/sha512.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Linker interposition exists only in this host test executable. It injects
 * provider failures without exposing a production fault-control interface. */
static size_t fail_at = SIZE_MAX, calls = 0, cleared_spans = 0;
static size_t contexts_opened = 0, contexts_closed = 0, live_contexts = 0;
static size_t clones = 0, pad_wipes = 0, digest_wipes = 0;

/* Store integer identities, not pointers that could outlive a faulty owner's
 * stack frame. Inspect bytes only via a currently live provider argument. */
typedef struct {
    uintptr_t identity;
    size_t hashed;
    bool live, started, finished;
} observed_context;
static observed_context observed[3];

void __real_mbedtls_sha512_init(mbedtls_sha512_context *);
void __real_mbedtls_sha512_free(mbedtls_sha512_context *);
void __real_mbedtls_sha512_clone(mbedtls_sha512_context *, const mbedtls_sha512_context *);
int __real_mbedtls_sha512_starts(mbedtls_sha512_context *, int);
int __real_mbedtls_sha512_update(mbedtls_sha512_context *, const unsigned char *, size_t);
int __real_mbedtls_sha512_finish(mbedtls_sha512_context *, unsigned char *);
int __real_mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
int __real_mbedtls_ripemd160(const unsigned char *, size_t, unsigned char *);
void __real_mbedtls_platform_zeroize(void *, size_t);

static void require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "secret context check failed: %s\n", message);
        abort();
    }
}

static void require_zero(const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i)
        require(bytes[i] == 0, "live scratch was not fully erased");
}

static observed_context *context_slot(const mbedtls_sha512_context *context)
{
    require(context != NULL, "NULL context");
    for (size_t i = 0; i < 3; ++i) {
        if (observed[i].live && observed[i].identity == (uintptr_t)context)
            return &observed[i];
    }
    require(false, "operation on unowned or released context");
    return NULL;
}

void __wrap_mbedtls_sha512_init(mbedtls_sha512_context *context)
{
    require(context != NULL, "NULL context initialization");
    for (size_t i = 0; i < 3; ++i)
        require(!observed[i].live || observed[i].identity != (uintptr_t)context,
            "context initialized twice without cleanup");
    for (size_t i = 0; i < 3; ++i) {
        if (observed[i].live)
            continue;
        __real_mbedtls_sha512_init(context);
        observed[i] = (observed_context){.identity = (uintptr_t)context, .live = true};
        ++contexts_opened;
        ++live_contexts;
        return;
    }
    require(false, "more than three live SHA512 contexts");
}

void __wrap_mbedtls_sha512_free(mbedtls_sha512_context *context)
{
    observed_context *slot = context_slot(context);
    __real_mbedtls_sha512_free(context);
    require_zero(context, sizeof(*context));
    *slot = (observed_context){0};
    ++contexts_closed;
    --live_contexts;
}

void __wrap_mbedtls_sha512_clone(mbedtls_sha512_context *destination,
                                 const mbedtls_sha512_context *source)
{
    observed_context *dst = context_slot(destination);
    const observed_context *src = context_slot(source);
    require(dst != src && src->started && !src->finished && src->hashed == 128,
        "clone source is not a completed immutable HMAC pad");
    __real_mbedtls_sha512_clone(destination, source);
    dst->started = src->started;
    dst->finished = src->finished;
    dst->hashed = src->hashed;
    ++clones;
}

static int fail_now(void)
{
    ++calls;
    return calls == fail_at;
}

int __wrap_mbedtls_sha512_starts(mbedtls_sha512_context *context, int is384)
{
    observed_context *slot = context_slot(context);
    require(!slot->started && is384 == 0, "unexpected SHA512 start");
    if (fail_now())
        return -1;
    int result = __real_mbedtls_sha512_starts(context, is384);
    slot->started = result == 0;
    return result;
}

int __wrap_mbedtls_sha512_update(mbedtls_sha512_context *context,
                                const unsigned char *bytes, size_t length)
{
    observed_context *slot = context_slot(context);
    require(slot->started && !slot->finished && bytes != NULL,
        "update without an active digest or input");
    require(length <= 512 && slot->hashed <= 640 - length, "unbounded SHA512 input");
    if (fail_now()) {
        memset(context, 0xee, sizeof(*context));
        return -1;
    }
    int result = __real_mbedtls_sha512_update(context, bytes, length);
    if (result == 0)
        slot->hashed += length;
    return result;
}

int __wrap_mbedtls_sha512_finish(mbedtls_sha512_context *context, unsigned char *output)
{
    observed_context *slot = context_slot(context);
    require(slot->started && !slot->finished && output != NULL, "invalid finish");
    if (fail_now()) {
        memset(output, 0xee, 64);
        memset(context, 0xee, sizeof(*context));
        return -1;
    }
    int result = __real_mbedtls_sha512_finish(context, output);
    slot->finished = result == 0;
    return result;
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
    require_zero(buffer, length);
    if (length == 128)
        ++pad_wipes;
    if (length == 64)
        ++digest_wipes;
    ++cleared_spans;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "secret failure check failed at line %d\n", __LINE__); return 1; } } while (0)

static void inject(size_t at)
{
    require(live_contexts == 0 && contexts_opened == contexts_closed,
        "a previous call returned with a live context");
    calls = 0;
    cleared_spans = 0;
    contexts_opened = 0;
    contexts_closed = 0;
    clones = 0;
    pad_wipes = 0;
    digest_wipes = 0;
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
            CHECK(calls == fail && pad_wipes == 1);
            CHECK(live_contexts == 0 && contexts_opened == contexts_closed);
            CHECK(digest_wipes == contexts_opened - 2);
            CHECK(memcmp(output, before, sizeof(output)) == 0);
        }
    }
    return 0;
}

static bool bytes_are(const uint8_t *bytes, size_t length, uint8_t value)
{
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != value)
            return false;
    }
    return true;
}

static int pbkdf2_case(size_t key_size, size_t failure)
{
    uint8_t key[131], salt[16], output[66];
    memset(key, 0x3c, sizeof(key));
    memset(salt, 0x6a, sizeof(salt));
    memset(output, 0x5a, sizeof(output));
    inject(failure);
    zcl_status status = zcl_pbkdf2_sha512_block(key, key_size, salt, sizeof(salt),
        output + 1, 64);
    if (failure == SIZE_MAX) {
        size_t normalization = key_size > 128 ? 1 : 0;
        CHECK(status == ZCL_OK && calls == 8196 + normalization * 4);
        CHECK(contexts_opened == 2050 + normalization && clones == 4096);
    } else {
        CHECK(status == ZCL_CRYPTO_FAILURE && calls == failure);
        CHECK(bytes_are(output, sizeof(output), 0x5a));
    }
    CHECK(output[0] == 0x5a && output[65] == 0x5a);
    CHECK(live_contexts == 0 && contexts_opened == contexts_closed);
    /* One 128-byte key block. Every non-pad context owns one 64-byte digest;
     * the KDF additionally owns current/next/accumulator, on every exit. */
    CHECK(pad_wipes == 1 && digest_wipes == contexts_opened + 1);
    CHECK(bytes_are(key, sizeof(key), 0x3c) && bytes_are(salt, sizeof(salt), 0x6a));
    return 0;
}

static int pbkdf2_failures(void)
{
    static const size_t rounds[] = {0, 1, 1023, 2046, 2047};
    for (size_t key_size = 32; key_size <= 131; key_size += 99) {
        const size_t preparation = key_size > 128 ? 8 : 4;
        for (size_t point = 1; point <= preparation; ++point)
            CHECK(pbkdf2_case(key_size, point) == 0);
        for (size_t round = 0; round < sizeof(rounds) / sizeof(rounds[0]); ++round) {
            for (size_t step = 1; step <= 4; ++step)
                CHECK(pbkdf2_case(key_size, preparation + rounds[round] * 4 + step) == 0);
        }
        CHECK(pbkdf2_case(key_size, SIZE_MAX) == 0);
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
    static const size_t points[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 101,
        4098, 4099, 4100, 4101, 8194, 8195, 8196, 8197};
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        inject(points[i]);
        CHECK(zcl_mnemonic_seed(text, sizeof(text) - 1, entropy, 0, output, sizeof(output)) == ZCL_CRYPTO_FAILURE);
        CHECK(calls == points[i] && cleared_spans >= 3);
        CHECK(live_contexts == 0 && contexts_opened == contexts_closed);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    inject(SIZE_MAX);
    CHECK(zcl_mnemonic_seed(text, sizeof(text) - 1, entropy, 0, output, sizeof(output)) == ZCL_OK);
    CHECK(calls == 8197 && contexts_opened == 2050 && clones == 4096);
    CHECK(live_contexts == 0 && contexts_closed == contexts_opened && pad_wipes == 1);
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
        CHECK(total > 8197 && contexts_opened == 2068 && clones == 4108);
        CHECK(live_contexts == 0 && contexts_closed == contexts_opened && pad_wipes == 7);
        const size_t points[] = {1, 2, 3, 4, 10, 101, 4097, 8197, 8198,
            total - 4, total - 3, total - 2, total - 1, total};
        for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
            memcpy(output, before, sizeof(output));
            length = 777;
            inject(points[i]);
            CHECK(functions[chain](entropy, sizeof(entropy), ZCL_MAINNET, 19, blinding, 32,
                output, sizeof(output), &length) == ZCL_CRYPTO_FAILURE);
            CHECK(calls == points[i] && cleared_spans >= 5 && length == 777);
            CHECK(live_contexts == 0 && contexts_opened == contexts_closed);
            CHECK(memcmp(output, before, sizeof(output)) == 0);
        }
    }
    return 0;
}

int main(void)
{
    if (hmac_failures() || pbkdf2_failures() || mnemonic_failures() || address_provider_failures())
        return 1;
    puts("secret failures: provider errors preserve output; all contexts and KDF scratch are erased");
    return 0;
}
