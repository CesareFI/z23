/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <secp256k1_preallocated.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { NORMAL, NO_MEMORY, HUGE_CONTEXT, EMPTY_CONTEXT, NO_CONTEXT,
    NO_BLINDING, HASH_FAILURE, ZERO_HASH, ORDER_TWEAK, NEGATIVE_TWEAK, ZERO_TWEAK } failure_mode;
static failure_mode mode = NORMAL;
static void *owned = NULL;
static size_t owned_size = 0, allocations = 0, releases = 0;

void *__real_malloc(size_t);
void __real_free(void *);
size_t __real_secp256k1_context_preallocated_size(unsigned int);
secp256k1_context *__real_secp256k1_context_preallocated_create(void *, unsigned int);
int __real_secp256k1_context_randomize(secp256k1_context *, const unsigned char *);
zcl_status __real_zcl_hmac_sha512(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t *, size_t);

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

zcl_status __wrap_zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                                 const uint8_t *data, size_t data_len, uint8_t *out, size_t capacity)
{
    static const uint8_t order[32] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41
    };
    if (mode == HASH_FAILURE)
        return ZCL_CRYPTO_FAILURE;
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

int main(void)
{
    if (context_failures() || derivation_failures())
        return 1;
    puts("key failures: allocation, context, blinding and invalid BIP32 results handled");
    return 0;
}
