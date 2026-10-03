/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"
#include "bip32_internal.h"
#include "zcl_wallet_record.h"

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

static bool watch_seed;
static uintptr_t seed_identity;
static uintptr_t anchor_identity;
static uint8_t seed_snapshot[64];
static size_t seed_calls, address_calls, seed_wipes, seed_end_calls, anchor_end_calls;
static size_t short_address;
static bool watch_address_key, fail_address_public;
static uintptr_t address_key_identity;
static size_t public_calls, address_key_wipes;
static bool watch_owned_seed;
static uintptr_t owned_seed_identity;
static size_t owned_seed_wipes, owned_seed_master_calls;

void __real_mbedtls_sha512_init(mbedtls_sha512_context *);
void __real_mbedtls_sha512_free(mbedtls_sha512_context *);
void __real_mbedtls_sha512_clone(mbedtls_sha512_context *, const mbedtls_sha512_context *);
int __real_mbedtls_sha512_starts(mbedtls_sha512_context *, int);
int __real_mbedtls_sha512_update(mbedtls_sha512_context *, const unsigned char *, size_t);
int __real_mbedtls_sha512_finish(mbedtls_sha512_context *, unsigned char *);
int __real_mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
int __real_mbedtls_ripemd160(const unsigned char *, size_t, unsigned char *);
void __real_mbedtls_platform_zeroize(void *, size_t);
zcl_status __real_zcl_entropy_seed(const uint8_t *, size_t, uint8_t *, size_t);
zcl_status __real_zcl_seed_address(const uint8_t *, size_t, zcl_network, uint32_t, uint32_t,
    const secp256k1_context *, uint8_t *, size_t, size_t *);
zcl_status __real_zcl_ec_public(const secp256k1_context *, const uint8_t *, size_t, uint8_t *, size_t);
void __real_zcl_ec_end(zcl_ec_context *);
zcl_status __real_zcl_bip32_master(const uint8_t *, size_t, zcl_extended_private *);

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

zcl_status __wrap_zcl_bip32_master(const uint8_t *seed, size_t length, zcl_extended_private *output)
{
    if (watch_owned_seed) {
        require(seed != NULL && length == 64 && owned_seed_identity == 0,
            "owned receive seed must enter master derivation once");
        owned_seed_identity = (uintptr_t)seed;
        owned_seed_master_calls = calls;
    }
    return __real_zcl_bip32_master(seed, length, output);
}

zcl_status __wrap_zcl_entropy_seed(const uint8_t *entropy, size_t length, uint8_t *seed, size_t capacity)
{
    if (watch_seed) {
        require(seed_calls++ == 0 && seed != NULL && capacity == 64, "seed must be derived exactly once");
        seed_identity = (uintptr_t)seed;
        require_zero(seed, capacity);
    }
    const zcl_status status = __real_zcl_entropy_seed(entropy, length, seed, capacity);
    if (watch_seed) { memcpy(seed_snapshot, seed, 64); seed_end_calls = calls; }
    return status;
}

zcl_status __wrap_zcl_seed_address(const uint8_t *seed, size_t length, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context,
    uint8_t *address, size_t capacity, size_t *written)
{
    if (watch_seed) {
        require(seed_calls == 1 && address_calls < 2 && (uintptr_t)seed == seed_identity,
            "address must use the one live recovered seed");
        require(length == 64 && memcmp(seed, seed_snapshot, 64) == 0, "reused seed changed");
        require(chain == address_calls && index == (address_calls == 0 ? 0U : 19U), "recovered path changed");
        if (chain == 0) anchor_identity = (uintptr_t)address;
        else require(anchor_identity == 0, "receive anchor must retire before change derivation");
        ++address_calls;
    }
    const zcl_status status = __real_zcl_seed_address(seed, length, network, chain, index,
        context, address, capacity, written);
    if (watch_seed && address_calls == short_address && status == ZCL_OK) *written = 34;
    if (watch_seed && address_calls == 1) anchor_end_calls = calls;
    return status;
}

static void observe_seed_wipe(const void *buffer, size_t length)
{
    if (!watch_seed || seed_identity == 0 || buffer == NULL) return;
    const uintptr_t start = (uintptr_t)buffer;
    if (seed_identity < start || seed_identity - start >= length) return;
    const size_t offset = (size_t)(seed_identity - start);
    require(offset == 0 && length == 64, "recovered seed needs its own last-use retirement");
    require_zero((const uint8_t *)buffer + offset, 64);
    seed_identity = 0;
    ++seed_wipes;
}

static void observe_anchor_wipe(const void *buffer, size_t length)
{
    if (!watch_seed || anchor_identity == 0 || (uintptr_t)buffer != anchor_identity) return;
    require(length == 35, "receive anchor wipe must cover its exact object");
    require_zero(buffer, length);
    anchor_identity = 0;
}

void __wrap_zcl_ec_end(zcl_ec_context *context)
{
    if (watch_seed && address_calls == 2)
        require(seed_identity == 0, "seed must retire before final context release");
    __real_zcl_ec_end(context);
}

zcl_status __wrap_zcl_ec_public(const secp256k1_context *context, const uint8_t *secret,
    size_t secret_len, uint8_t *output, size_t capacity)
{
    if (watch_address_key && ++public_calls == 3) {
        if (watch_owned_seed)
            require(owned_seed_identity == 0 && owned_seed_wipes == 1,
                "owned seed survived into final public key generation");
        /* Fixed BIP44 path: two nonhardened child inputs, then the final key.
         * Keep only its identity; inspect through the later live wipe span. */
        require(secret != NULL && secret_len == 32 && address_key_identity == 0,
            "final address key capture changed");
        address_key_identity = (uintptr_t)secret;
        if (fail_address_public) return ZCL_CRYPTO_FAILURE;
    }
    return __real_zcl_ec_public(context, secret, secret_len, output, capacity);
}

static void observe_address_key_wipe(const void *buffer, size_t length)
{
    if (!watch_address_key || address_key_identity == 0 || buffer == NULL) return;
    const uintptr_t start = (uintptr_t)buffer;
    if (address_key_identity < start || address_key_identity - start >= length) return;
    const size_t offset = (size_t)(address_key_identity - start);
    /* secret is the first member; require the chain code to clear as well. */
    _Static_assert(offsetof(zcl_extended_private, secret) == 0, "private key starts at secret");
    require(length - offset >= sizeof(zcl_extended_private), "address private-key wipe is short");
    require_zero((const uint8_t *)buffer + offset, sizeof(zcl_extended_private));
    address_key_identity = 0;
    ++address_key_wipes;
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
    if (watch_address_key && public_calls == 3)
        require(address_key_identity == 0 && address_key_wipes == 1,
            "private address key survived into public hashing");
    return fail_now() ? -1 : __real_mbedtls_sha256(input, length, output, is224);
}

int __wrap_mbedtls_ripemd160(const unsigned char *input, size_t length, unsigned char *output)
{
    return fail_now() ? -1 : __real_mbedtls_ripemd160(input, length, output);
}

void __wrap_mbedtls_platform_zeroize(void *buffer, size_t length)
{
    __real_mbedtls_platform_zeroize(buffer, length);
    if (watch_owned_seed && owned_seed_identity != 0 && (uintptr_t)buffer == owned_seed_identity) {
        require(length == 64, "owned receive seed wipe must cover all64 bytes");
        require_zero(buffer, length);
        owned_seed_identity = 0;
        ++owned_seed_wipes;
    }
    observe_anchor_wipe(buffer, length);
    require_zero(buffer, length);
    observe_seed_wipe(buffer, length);
    observe_address_key_wipe(buffer, length);
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

static void seed_watch_start(size_t failure)
{
    require(seed_identity == 0 && anchor_identity == 0 && !watch_seed, "previous recovered seed lifetime did not end");
    inject(failure);
    seed_calls = address_calls = seed_wipes = seed_end_calls = anchor_end_calls = 0;
    __real_mbedtls_platform_zeroize(seed_snapshot, sizeof(seed_snapshot));
    watch_seed = true;
}

static void seed_watch_end(void)
{
    require(seed_identity == 0 && seed_calls <= 1 && seed_wipes == seed_calls,
        "recovered seed escaped without complete live erasure");
    require(anchor_identity == 0, "receive anchor escaped without last-use retirement");
    require(live_contexts == 0 && contexts_opened == contexts_closed, "recovered provider context escaped");
    watch_seed = false;
    __real_mbedtls_platform_zeroize(seed_snapshot, sizeof(seed_snapshot));
}

static int recovered_binding_failures(void)
{
    uint8_t entropy[16] = {0}, blinding[64], header[80], output[37], before[37];
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    inject(SIZE_MAX);
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_TESTNET, blinding, 32,
        header, sizeof(header)) == ZCL_OK);
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    entropy[0] = 1;
    seed_watch_start(SIZE_MAX);
    CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), 19,
        blinding, sizeof(blinding), output + 1, 35) == ZCL_INVALID_ENCODING);
    seed_watch_end();
    CHECK(seed_calls == 1 && address_calls == 1 && seed_wipes == 1);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    entropy[0] = 0;
    for (short_address = 1; short_address <= 2; ++short_address) {
        seed_watch_start(SIZE_MAX);
        const zcl_status expected = short_address == 1 ? ZCL_INVALID_ENCODING : ZCL_CRYPTO_FAILURE;
        CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), 19,
            blinding, sizeof(blinding), output + 1, 35) == expected);
        seed_watch_end();
        CHECK(seed_calls == 1 && address_calls == short_address && seed_wipes == 1);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    short_address = 0;
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

static int recovered_seed_failures(size_t entropy_len)
{
    uint8_t entropy[32] = {0}, blinding[64] = {0}, header[80] = {0}, output[37];
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    inject(SIZE_MAX);
    CHECK(zcl_wallet_header_create(entropy, entropy_len, ZCL_TESTNET, blinding, 32, header, sizeof(header)) == ZCL_OK);
    seed_watch_start(SIZE_MAX);
    memset(output, 0xa5, sizeof(output));
    CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, entropy_len, 19,
        blinding, sizeof(blinding), output + 1, 35) == ZCL_OK);
    seed_watch_end();
    CHECK(seed_calls == 1 && address_calls == 2 && seed_wipes == 1);
    /* The zero-entropy 24-word mnemonic exceeds one SHA512 key block and
     * needs one normalization context; the 12-word mnemonic does not. */
    const size_t normalization = entropy_len == 32 ? 1 : 0;
    CHECK(contexts_opened == 2086 + normalization && clones == 4120 && pad_wipes == 13);
    CHECK(output[0] == 0xa5 && output[36] == 0xa5);
    const size_t total = calls;
    CHECK(seed_end_calls > 3 && anchor_end_calls > seed_end_calls && total > anchor_end_calls + 3);
    const size_t points[] = {1, 2, 3, 4, seed_end_calls - 3, seed_end_calls,
        seed_end_calls + 1, anchor_end_calls - 3, anchor_end_calls,
        anchor_end_calls + 1, total - 3, total - 2, total - 1, total};
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        seed_watch_start(points[i]);
        memset(output, 0xa5, sizeof(output));
        CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, entropy_len, 19,
            blinding, sizeof(blinding), output + 1, 35) == ZCL_CRYPTO_FAILURE);
        seed_watch_end();
        CHECK(calls == points[i] && bytes_are(output, sizeof(output), 0xa5));
    }
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

static int address_key_retirement(void)
{
    uint8_t seed[64] = {0}, blinding[32] = {1}, output[35], before[35];
    zcl_ec_context context = {0};
    CHECK(zcl_ec_begin(&context, blinding, sizeof(blinding)) == ZCL_OK);
    size_t total = 0;
    for (uint32_t chain = 0; chain < 2; ++chain) {
        for (size_t mode = 0; mode < 4; ++mode) {
            require(address_key_identity == 0 && !watch_address_key, "address key escaped previous call");
            /* The last four hashes encode the final public address. */
            inject(mode == 1 ? total - 3 : mode == 2 ? total - 2 : SIZE_MAX);
            watch_address_key = true;
            fail_address_public = mode == 3;
            public_calls = address_key_wipes = 0;
            memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
            size_t length = 777;
            const zcl_status status = zcl_seed_address(seed, sizeof(seed), ZCL_TESTNET, chain, 19,
                context.handle, output, sizeof(output), &length);
            require(address_key_identity == 0 && address_key_wipes == 1 && public_calls == 3,
                "final address key was not retired exactly once");
            watch_address_key = false;
            if (mode == 0) {
                CHECK(status == ZCL_OK && length == 35 && calls > 4);
                total = calls;
            } else {
                CHECK(status == ZCL_CRYPTO_FAILURE && length == 777);
                CHECK(memcmp(output, before, sizeof(output)) == 0);
            }
        }
    }
    zcl_ec_end(&context);
    zcl_secure_zero(seed, sizeof(seed)); zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

static int owned_seed_retirement(void)
{
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    uint8_t output[35], before[35];
    size_t total = 0;
    for (unsigned chain = 0; chain < 2; ++chain) {
        for (unsigned failure = 0; failure < 4; ++failure) {
            inject(failure == 2 ? owned_seed_master_calls + 1 : failure == 3 ? total - 3 : SIZE_MAX);
            watch_owned_seed = watch_address_key = true;
            fail_address_public = failure == 1;
            public_calls = address_key_wipes = owned_seed_wipes = 0;
            memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
            size_t length = 777;
            const zcl_status status = chain == 0
                ? zcl_receive_from_entropy(entropy, sizeof(entropy), ZCL_TESTNET, 19,
                    blinding, sizeof(blinding), output, sizeof(output), &length)
                : zcl_change_from_entropy(entropy, sizeof(entropy), ZCL_TESTNET, 19,
                    blinding, sizeof(blinding), output, sizeof(output), &length);
            require(owned_seed_identity == 0 && owned_seed_wipes == 1 && address_key_identity == 0,
                "owned seed or final address key escaped retirement");
            watch_owned_seed = watch_address_key = false;
            CHECK(status == (failure == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
            if (failure == 0) { CHECK(length == 35 && calls > 4); total = calls; }
            else CHECK(length == 777 && memcmp(output, before, sizeof(output)) == 0);
        }
    }
    return 0;
}

int main(void)
{
    if (hmac_failures() || pbkdf2_failures() || mnemonic_failures() || address_provider_failures()
        || recovered_seed_failures(16) || recovered_seed_failures(32) || recovered_binding_failures()
        || address_key_retirement() || owned_seed_retirement())
        return 1;
    puts("secret failures: provider errors preserve output; all contexts and KDF scratch are erased");
    return 0;
}
