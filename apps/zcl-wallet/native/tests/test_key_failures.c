/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"
#include "bip32_internal.h"
#include "zcl_wallet_record.h"

#include <secp256k1_preallocated.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { NORMAL, NO_MEMORY, HUGE_CONTEXT, EMPTY_CONTEXT, NO_CONTEXT,
    NO_BLINDING, HASH_FAILURE, CHILD_FAILURE, ZERO_HASH, ORDER_TWEAK, NEGATIVE_TWEAK, ZERO_TWEAK } failure_mode;
static failure_mode mode = NORMAL;
static void *owned = NULL;
static size_t owned_size = 0, allocations = 0, releases = 0;
static size_t child_calls = 0, fail_child = 0;
static size_t watched_contexts, fail_context;
static int watch_contexts;
typedef zcl_status (*address_function)(const uint8_t *, size_t, zcl_network, uint32_t,
    const uint8_t *, size_t, uint8_t *, size_t, size_t *);
static const address_function address_functions[] = {zcl_receive_from_entropy, zcl_change_from_entropy};

void *__real_malloc(size_t);
void __real_free(void *);
size_t __real_secp256k1_context_preallocated_size(unsigned int);
secp256k1_context *__real_secp256k1_context_preallocated_create(void *, unsigned int);
int __real_secp256k1_context_randomize(secp256k1_context *, const unsigned char *);
zcl_status __real_zcl_hmac_sha512(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t *, size_t);

static failure_mode context_mode(void)
{
    return watch_contexts && watched_contexts != fail_context ? NORMAL : mode;
}

void *__wrap_malloc(size_t size)
{
    if (context_mode() == NO_MEMORY)
        return NULL;
    if (owned != NULL || size == 0 || size > 1024)
        abort();
    void *result = __real_malloc(size);
    if (result != NULL) {
        owned = result;
        owned_size = size;
        ++allocations;
    }
    return result;
}

void __wrap_free(void *memory)
{
    if (memory != NULL) {
        if (memory != owned)
            abort();
        const uint8_t *bytes = memory;
        for (size_t i = 0; i < owned_size; ++i) {
            if (bytes[i] != 0)
                abort();
        }
        owned = NULL;
        owned_size = 0;
        ++releases;
    }
    __real_free(memory);
}

size_t __wrap_secp256k1_context_preallocated_size(unsigned int flags)
{
    if (watch_contexts && ++watched_contexts > 2) abort();
    if (context_mode() == HUGE_CONTEXT)
        return SIZE_MAX;
    if (context_mode() == EMPTY_CONTEXT)
        return 0;
    return __real_secp256k1_context_preallocated_size(flags);
}

secp256k1_context *__wrap_secp256k1_context_preallocated_create(void *storage, unsigned int flags)
{
    return context_mode() == NO_CONTEXT ? NULL : __real_secp256k1_context_preallocated_create(storage, flags);
}

int __wrap_secp256k1_context_randomize(secp256k1_context *context, const unsigned char *seed)
{
    if (watch_contexts) {
        if (seed == NULL || watched_contexts == 0 || watched_contexts > 2) abort();
        for (size_t i = 0; i < 32; ++i) {
            if (seed[i] != (uint8_t)watched_contexts) {
                fputs("key failure check failed: independent context blinding changed\n", stderr);
                abort();
            }
        }
    }
    return context_mode() == NO_BLINDING ? 0 : __real_secp256k1_context_randomize(context, seed);
}

zcl_status __wrap_zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                                 const uint8_t *data, size_t data_len, uint8_t *out, size_t capacity)
{
    static const uint8_t order[32] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41
    };
    if (mode == HASH_FAILURE)
        return ZCL_CRYPTO_FAILURE;
    if (mode == CHILD_FAILURE && key_len == 32 && data_len == 37) {
        ++child_calls;
        if (child_calls == fail_child) {
            if (out == NULL || capacity < 64) abort();
            memset(out, 0, 64);
            memcpy(out, order, sizeof(order));
            return ZCL_OK;
        }
    }
    if (mode < ZERO_HASH)
        return __real_zcl_hmac_sha512(key, key_len, data, data_len, out, capacity);
    if (out == NULL || capacity < 64)
        abort();
    memset(out, 0, 64);
    if (mode == ORDER_TWEAK || mode == NEGATIVE_TWEAK)
        memcpy(out, order, sizeof(order));
    if (mode == NEGATIVE_TWEAK)
        --out[31];
    if (mode == ZERO_TWEAK)
        memset(out + 32, 0x42, 32);
    return ZCL_OK;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "key failure check failed at line %d\n", __LINE__); return 1; } } while (0)

static int context_failures(void)
{
    uint8_t secret[32] = {0}, blinding[32] = {1}, out[35] = {0}, before[35] = {0};
    secret[31] = 1;
    memset(out, 0xa5, sizeof(out));
    memcpy(before, out, sizeof(before));
    for (mode = NO_MEMORY; mode <= NO_BLINDING; mode = (failure_mode)((int)mode + 1)) {
        zcl_status expected = mode == NO_MEMORY ? ZCL_RESOURCE_EXHAUSTED : ZCL_CRYPTO_FAILURE;
        CHECK(zcl_public_key(secret, 32, blinding, 32, out, 33) == expected);
        CHECK(memcmp(out, before, sizeof(out)) == 0 && owned == NULL && allocations == releases);
    }
    mode = NO_MEMORY;
    size_t length = 777;
    CHECK(zcl_receive_from_entropy(secret, 32, ZCL_MAINNET, 0, blinding, 32,
                                    out, sizeof(out), &length) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(length == 777 && memcmp(out, before, sizeof(out)) == 0);
    mode = NORMAL;
    CHECK(zcl_public_key(secret, 32, blinding, 32, out, 33) == ZCL_OK);
    CHECK(owned == NULL && allocations == releases);
    return 0;
}

static int derivation_failures(void)
{
    uint8_t seed[32] = {0}, blinding[32] = {1};
    zcl_extended_private parent = {0}, out = {0}, before = {0};
    parent.secret[31] = 1;
    memset(&out, 0xa5, sizeof(out));
    before = out;
    mode = HASH_FAILURE;
    CHECK(zcl_bip32_master(seed, 32, &out) == ZCL_CRYPTO_FAILURE);
    CHECK(zcl_bip32_child(&parent, 0, blinding, 32, &out) == ZCL_CRYPTO_FAILURE);
    mode = ZERO_HASH;
    CHECK(zcl_bip32_master(seed, 32, &out) == ZCL_CRYPTO_FAILURE);
    mode = ORDER_TWEAK;
    CHECK(zcl_bip32_master(seed, 32, &out) == ZCL_CRYPTO_FAILURE);
    CHECK(zcl_bip32_child(&parent, UINT32_MAX, blinding, 32, &out) == ZCL_INVALID_CHILD);
    mode = NEGATIVE_TWEAK;
    CHECK(zcl_bip32_child(&parent, 0, blinding, 32, &out) == ZCL_INVALID_CHILD);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0 && owned == NULL && allocations == releases);
    mode = ZERO_TWEAK;
    CHECK(zcl_bip32_child(&parent, 0, blinding, 32, &out) == ZCL_OK);
    CHECK(memcmp(out.secret, parent.secret, 32) == 0);
    for (size_t i = 0; i < 32; ++i)
        CHECK(out.chain_code[i] == 0x42);
    CHECK(owned == NULL && allocations == releases);
    return 0;
}

static int address_failures(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, output[35], before[35];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    size_t length = 777;
    for (size_t chain = 0; chain < 2; ++chain) {
        for (mode = NO_MEMORY; mode <= NO_BLINDING; mode = (failure_mode)((int)mode + 1)) {
            const zcl_status expected = mode == NO_MEMORY ? ZCL_RESOURCE_EXHAUSTED : ZCL_CRYPTO_FAILURE;
            CHECK(address_functions[chain](entropy, sizeof(entropy), ZCL_TESTNET, 19, blinding, 32,
                output, sizeof(output), &length) == expected);
            CHECK(length == 777 && memcmp(output, before, sizeof(output)) == 0);
            CHECK(owned == NULL && allocations == releases);
        }
        mode = CHILD_FAILURE;
        for (fail_child = 1; fail_child <= 5; ++fail_child) {
            child_calls = 0;
            CHECK(address_functions[chain](entropy, sizeof(entropy), ZCL_MAINNET, UINT32_C(0x7fffffff),
                blinding, 32, output, sizeof(output), &length) == ZCL_INVALID_CHILD);
            CHECK(child_calls == fail_child && length == 777);
            CHECK(memcmp(output, before, sizeof(output)) == 0 && owned == NULL && allocations == releases);
        }
    }
    mode = NORMAL;
    return 0;
}

static int recovered_change_failures(void)
{
    uint8_t entropy[16] = {0}, blinding[64] = {1}, header[80], output[35], before[35];
    blinding[32] = 2;
    mode = NORMAL;
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, 32,
        header, sizeof(header)) == ZCL_OK);
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    for (mode = NO_MEMORY; mode <= NO_BLINDING; mode = (failure_mode)((int)mode + 1)) {
        const zcl_status expected = mode == NO_MEMORY ? ZCL_RESOURCE_EXHAUSTED : ZCL_CRYPTO_FAILURE;
        CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), 17,
            blinding, sizeof(blinding), output, sizeof(output)) == expected);
        CHECK(memcmp(output, before, sizeof(output)) == 0 && owned == NULL && allocations == releases);
    }
    mode = CHILD_FAILURE;
    for (fail_child = 1; fail_child <= 10; ++fail_child) {
        child_calls = 0;
        CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), UINT32_C(0x7fffffff),
            blinding, sizeof(blinding), output, sizeof(output)) == ZCL_INVALID_CHILD);
        CHECK(child_calls == fail_child);
        CHECK(memcmp(output, before, sizeof(output)) == 0 && owned == NULL && allocations == releases);
    }
    mode = NORMAL;
    zcl_secure_zero(entropy, sizeof(entropy));
    return 0;
}

static int recovered_context_stages(void)
{
    uint8_t entropy[16] = {0}, blinding[64], header[80], output[37], before[37];
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_TESTNET, blinding, 32,
        header, sizeof(header)) == ZCL_OK);
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    watch_contexts = 1;
    for (fail_context = 1; fail_context <= 2; ++fail_context) {
        for (mode = NO_MEMORY; mode <= NO_BLINDING; mode = (failure_mode)((int)mode + 1)) {
            watched_contexts = 0;
            const zcl_status expected = mode == NO_MEMORY ? ZCL_RESOURCE_EXHAUSTED : ZCL_CRYPTO_FAILURE;
            CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), 19,
                blinding, sizeof(blinding), output + 1, 35) == expected);
            CHECK(watched_contexts == fail_context && memcmp(output, before, sizeof(output)) == 0);
            CHECK(owned == NULL && allocations == releases);
        }
    }
    mode = NORMAL; watched_contexts = 0;
    CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, sizeof(entropy), 19,
        blinding, sizeof(blinding), output + 1, 35) == ZCL_OK);
    CHECK(watched_contexts == 2 && owned == NULL && allocations == releases);
    CHECK(output[0] == 0xa5 && output[36] == 0xa5);
    watch_contexts = 0;
    zcl_secure_zero(entropy, sizeof(entropy)); zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

static int seed_bounds(void)
{
    uint8_t entropy[32] = {0}, output[66], before[66];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    CHECK(zcl_entropy_seed(NULL, 16, output + 1, 64) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_entropy_seed(entropy, 16, NULL, 64) == ZCL_INVALID_ARGUMENT);
    const size_t lengths[] = {0, 1, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        CHECK(zcl_entropy_seed(entropy, lengths[i], output + 1, 64) == ZCL_OUT_OF_RANGE);
    for (size_t capacity = 0; capacity < 64; ++capacity)
        CHECK(zcl_entropy_seed(entropy, 16, output + 1, capacity) == ZCL_BUFFER_TOO_SMALL);
    CHECK(memcmp(output, before, sizeof(output)) == 0 && owned == NULL && allocations == releases);
    zcl_secure_zero(entropy, sizeof(entropy));
    return 0;
}

static int seed_address_bounds(const zcl_ec_context *context, const uint8_t seed[64])
{
    uint8_t output[37], before[37];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
    size_t length = 777;
    CHECK(zcl_seed_address(NULL, 64, ZCL_TESTNET, 1, 19, context->handle, output + 1, 35, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, 19, NULL, output + 1, 35, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, 19, context->handle, NULL, 35, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, 19, context->handle, output + 1, 35, NULL) == ZCL_INVALID_ARGUMENT);
    const size_t lengths[] = {0, 1, 16, 32, 63, 65, SIZE_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        CHECK(zcl_seed_address(seed, lengths[i], ZCL_TESTNET, 1, 19, context->handle, output + 1, 35, &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_seed_address(seed, 64, (zcl_network)2, 1, 19, context->handle, output + 1, 35, &length) == ZCL_UNSUPPORTED);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 2, 19, context->handle, output + 1, 35, &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, UINT32_MAX, 19, context->handle, output + 1, 35, &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, UINT32_C(0x80000000), context->handle, output + 1, 35, &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, UINT32_MAX, context->handle, output + 1, 35, &length) == ZCL_OUT_OF_RANGE);
    for (size_t capacity = 0; capacity < 35; ++capacity)
        CHECK(zcl_seed_address(seed, 64, ZCL_TESTNET, 1, 19, context->handle, output + 1, capacity, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == 777 && memcmp(output, before, sizeof(output)) == 0);
    return 0;
}

static int seeded_address(void)
{
    uint8_t entropy[16] = {0}, seed[64] = {0}, blinding[32] = {1}, address[37];
    static const uint8_t expected[] = "tmWdoCDUViginL7DVpVu2YSB3GXm2K5b2ZN";
    zcl_ec_context context = {0};
    CHECK(zcl_entropy_seed(entropy, sizeof(entropy), seed, sizeof(seed)) == ZCL_OK);
    CHECK(zcl_ec_begin(&context, blinding, sizeof(blinding)) == ZCL_OK);
    const size_t before_allocations = allocations;
    CHECK(seed_address_bounds(&context, seed) == 0);
    size_t length = 777;
    memset(address, 0xa5, sizeof(address));
    CHECK(zcl_seed_address(seed, sizeof(seed), ZCL_TESTNET, 1, 19, context.handle, address + 1, 35, &length) == ZCL_OK);
    CHECK(length == 35 && address[0] == 0xa5 && address[36] == 0xa5 && memcmp(address + 1, expected, 35) == 0);
    CHECK(allocations == before_allocations); /* Seed/address helpers borrow this sole context. */
    zcl_ec_end(&context);
    CHECK(owned == NULL && allocations == releases);
    zcl_secure_zero(seed, sizeof(seed)); zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

int main(void)
{
    if (context_failures() || derivation_failures() || address_failures() || recovered_change_failures()
        || recovered_context_stages() || seed_bounds() || seeded_address())
        return 1;
    puts("key failures: allocation, context, blinding and invalid BIP32 results handled");
    return 0;
}
