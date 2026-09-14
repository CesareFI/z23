/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"
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
enum { DIGEST_SPAN, CHILD_DATA_SPAN, CHILD_RESULT_SPAN, SPAN_COUNT };
typedef struct {
    uintptr_t address;
    size_t length, observed, erased;
    bool live;
} observed_span;
static observed_span spans[SPAN_COUNT];
typedef zcl_status (*address_function)(const uint8_t *, size_t, zcl_network, uint32_t,
    const uint8_t *, size_t, uint8_t *, size_t, size_t *);
static const address_function address_functions[] = {zcl_receive_from_entropy, zcl_change_from_entropy};

void *__real_malloc(size_t);
void __real_free(void *);
size_t __real_secp256k1_context_preallocated_size(unsigned int);
secp256k1_context *__real_secp256k1_context_preallocated_create(void *, unsigned int);
int __real_secp256k1_context_randomize(secp256k1_context *, const unsigned char *);
zcl_status __real_zcl_hmac_sha512(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t *, size_t);
int __real_secp256k1_ec_seckey_tweak_add(const secp256k1_context *, unsigned char *, const unsigned char *);
void __real_zcl_secure_zero(void *, size_t);

static void observe_span(size_t slot, const void *buffer, size_t length)
{
    if (slot >= SPAN_COUNT || buffer == NULL || spans[slot].live)
        abort();
    /* Retain only an address stamp, never an expired C pointer. All byte reads
     * below use the zeroizer's currently live argument. Counts/flags remain
     * inspectable if a mutation returns without clearing an observed object. */
    spans[slot].address = (uintptr_t)buffer;
    spans[slot].length = length;
    spans[slot].live = true;
    ++spans[slot].observed;
}

static bool all_spans_erased(void)
{
    for (size_t i = 0; i < SPAN_COUNT; ++i) {
        if (spans[i].live || spans[i].observed != spans[i].erased)
            return false;
    }
    return true;
}

void __wrap_zcl_secure_zero(void *buffer, size_t length)
{
    __real_zcl_secure_zero(buffer, length);
    for (size_t i = 0; i < SPAN_COUNT; ++i) {
        if (!spans[i].live || spans[i].address != (uintptr_t)buffer)
            continue;
        if (buffer == NULL || length != spans[i].length)
            abort();
        const uint8_t *bytes = buffer;
        for (size_t j = 0; j < length; ++j) {
            if (bytes[j] != 0) abort();
        }
        spans[i].live = false;
        spans[i].address = 0;
        ++spans[i].erased;
    }
}

int __wrap_secp256k1_ec_seckey_tweak_add(const secp256k1_context *context,
                                        unsigned char *secret, const unsigned char *tweak)
{
    /* derive_child passes the first member of its complete 64-byte result.
     * Observe that object's later full clear; do not inspect beyond secret here. */
    _Static_assert(offsetof(zcl_extended_private, secret) == 0 && sizeof(zcl_extended_private) == 64,
        "BIP32 result observer requires the exact public struct layout");
    observe_span(CHILD_RESULT_SPAN, secret, sizeof(zcl_extended_private));
    return __real_secp256k1_ec_seckey_tweak_add(context, secret, tweak);
}

static void observe_hmac(const uint8_t *key, size_t key_len, const uint8_t *data,
                          size_t data_len, uint8_t *out, size_t capacity)
{
    if (key_len != 12 && !(key_len == 32 && data_len == 37))
        return; /* The mnemonic PBKDF2 HMAC buffers have a different owner. */
    if (key == NULL || data == NULL || out == NULL || capacity < 64)
        abort();
    observe_span(DIGEST_SPAN, out, 64);
    if (key_len == 32)
        observe_span(CHILD_DATA_SPAN, data, 37);
}

void *__wrap_malloc(size_t size)
{
    if (mode == NO_MEMORY)
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
    if (mode == HUGE_CONTEXT)
        return SIZE_MAX;
    if (mode == EMPTY_CONTEXT)
        return 0;
    return __real_secp256k1_context_preallocated_size(flags);
}

secp256k1_context *__wrap_secp256k1_context_preallocated_create(void *storage, unsigned int flags)
{
    return mode == NO_CONTEXT ? NULL : __real_secp256k1_context_preallocated_create(storage, flags);
}

int __wrap_secp256k1_context_randomize(secp256k1_context *context, const unsigned char *seed)
{
    return mode == NO_BLINDING ? 0 : __real_secp256k1_context_randomize(context, seed);
}

static zcl_status synthetic_hash(uint8_t *out, size_t capacity)
{
    static const uint8_t order[32] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41
    };
    if (out == NULL || capacity < 64)
        abort();
    memset(out, 0, 64);
    if (mode == CHILD_FAILURE || mode == ORDER_TWEAK || mode == NEGATIVE_TWEAK)
        memcpy(out, order, sizeof(order));
    if (mode == NEGATIVE_TWEAK)
        --out[31];
    if (mode == ZERO_TWEAK)
        memset(out + 32, 0x42, 32);
    return ZCL_OK;
}

zcl_status __wrap_zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                                 const uint8_t *data, size_t data_len, uint8_t *out, size_t capacity)
{
    observe_hmac(key, key_len, data, data_len, out, capacity);
    if (mode == HASH_FAILURE) {
        if (out == NULL || capacity < 64) abort();
        memset(out, 0x42, 64); /* Failed providers can leave partial secrets. */
        return ZCL_CRYPTO_FAILURE;
    }
    if (mode == CHILD_FAILURE && key_len == 32 && data_len == 37) {
        ++child_calls;
        if (child_calls == fail_child)
            return synthetic_hash(out, capacity);
    }
    if (mode < ZERO_HASH)
        return __real_zcl_hmac_sha512(key, key_len, data, data_len, out, capacity);
    return synthetic_hash(out, capacity);
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

static void reset_observations(void)
{
    if (!all_spans_erased() || owned != NULL || allocations != releases)
        abort();
    memset(spans, 0, sizeof(spans));
}

static int derivation_erasure(void)
{
    static const struct {
        failure_mode fault;
        zcl_status master, child;
    } cases[] = {
        {NORMAL, ZCL_OK, ZCL_OK}, {HASH_FAILURE, ZCL_CRYPTO_FAILURE, ZCL_CRYPTO_FAILURE},
        {ZERO_HASH, ZCL_CRYPTO_FAILURE, ZCL_OK}, {ORDER_TWEAK, ZCL_CRYPTO_FAILURE, ZCL_INVALID_CHILD},
        {NEGATIVE_TWEAK, ZCL_OK, ZCL_INVALID_CHILD}, {ZERO_TWEAK, ZCL_CRYPTO_FAILURE, ZCL_OK}
    };
    uint8_t seed[32] = {0}, blinding[32] = {1};
    zcl_extended_private parent = {0}, output, before;
    parent.secret[31] = 1;
    memset(&before, 0xa5, sizeof(before));
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mode = cases[i].fault;
        output = before;
        reset_observations();
        CHECK(zcl_bip32_master(seed, sizeof(seed), &output) == cases[i].master);
        CHECK(all_spans_erased() && spans[DIGEST_SPAN].observed == 1);
        CHECK(spans[CHILD_DATA_SPAN].observed == 0 && spans[CHILD_RESULT_SPAN].observed == 0);
        CHECK(cases[i].master == ZCL_OK || memcmp(&output, &before, sizeof(output)) == 0);
        for (size_t hardened = 0; hardened < 2; ++hardened) {
            output = before;
            reset_observations();
            CHECK(zcl_bip32_child(&parent, hardened == 0 ? 19 : UINT32_MAX,
                blinding, sizeof(blinding), &output) == cases[i].child);
            CHECK(all_spans_erased() && spans[DIGEST_SPAN].observed == 1);
            CHECK(spans[CHILD_DATA_SPAN].observed == 1);
            CHECK(spans[CHILD_RESULT_SPAN].observed == (mode == HASH_FAILURE ? 0u : 1u));
            CHECK(cases[i].child == ZCL_OK || memcmp(&output, &before, sizeof(output)) == 0);
        }
    }
    zcl_secure_zero(seed, sizeof(seed));
    zcl_secure_zero(&parent, sizeof(parent));
    zcl_secure_zero(&output, sizeof(output));
    mode = NORMAL;
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

int main(void)
{
    if (context_failures() || derivation_failures() || derivation_erasure() ||
        address_failures() || recovered_change_failures())
        return 1;
    CHECK(all_spans_erased());
    puts("key failures: allocation, context, blinding and invalid BIP32 results handled");
    return 0;
}
