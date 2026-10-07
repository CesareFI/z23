/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_recovery.h"
#include "bip32_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Link wrappers observe only live borrowed spans, never retired stack memory.
 * Real providers run except at the selected failure boundary. */
typedef enum { NONE, ALLOCATE, BEGIN, SEED, CHILD, LENGTH } fault_kind;
static fault_kind fault;
static size_t fail_at, begins, seeds, children, ends, seed_wipes, candidate_wipes;
static uint8_t *seed_span, *candidate_span;
zcl_status __real_zcl_ec_begin(zcl_ec_context *, const uint8_t *, size_t);
void __real_zcl_ec_end(zcl_ec_context *);
zcl_status __real_zcl_entropy_seed(const uint8_t *, size_t, uint8_t *, size_t);
zcl_status __real_zcl_seed_address(const uint8_t *, size_t, zcl_network, uint32_t,
    uint32_t, const secp256k1_context *, uint8_t *, size_t, size_t *);
void __real_zcl_secure_zero(void *, size_t);
void *__real_malloc(size_t);

void *__wrap_malloc(size_t size)
{
    return fault == ALLOCATE ? NULL : __real_malloc(size);
}

zcl_status __wrap_zcl_ec_begin(zcl_ec_context *context, const uint8_t *blinding, size_t length)
{
    ++begins;
    const zcl_status status = __real_zcl_ec_begin(context, blinding, length);
    if (fault == ALLOCATE) {
        assert(status == ZCL_RESOURCE_EXHAUSTED);
        return status;
    }
    assert(status == ZCL_OK);
    /* Refusal after ownership is acquired must still retire the whole context. */
    return fault == BEGIN ? ZCL_RESOURCE_EXHAUSTED : status;
}

zcl_status __wrap_zcl_entropy_seed(const uint8_t *entropy, size_t length, uint8_t *seed, size_t capacity)
{
    ++seeds; seed_span = seed;
    if (fault == SEED) {
        memset(seed, 0x42, capacity);
        return ZCL_CRYPTO_FAILURE;
    }
    return __real_zcl_entropy_seed(entropy, length, seed, capacity);
}

zcl_status __wrap_zcl_seed_address(const uint8_t *seed, size_t length, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context,
    uint8_t *address, size_t capacity, size_t *written)
{
    if (children == 0) candidate_span = address;
    ++children;
    assert(index == 19 + (uint32_t)children - 1); /* Never skip a failed child. */
    assert(capacity == 35);
    if (fault == CHILD && children == fail_at) {
        memset(address, 0x42, capacity);
        return ZCL_INVALID_CHILD;
    }
    const zcl_status status = __real_zcl_seed_address(seed, length, network, chain, index,
        context, address, capacity, written);
    assert(status == ZCL_OK);
    if (fault == LENGTH && children == fail_at) *written = 34;
    return status;
}

static void cleared(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    for (size_t i = 0; i < length; ++i) assert(bytes[i] == 0);
}

void __wrap_zcl_secure_zero(void *data, size_t length)
{
    __real_zcl_secure_zero(data, length);
    if (data == seed_span && seed_span != NULL) {
        assert(length == 64); cleared(data, length); seed_span = NULL; ++seed_wipes;
    }
    if (data == candidate_span && candidate_span != NULL) {
        assert(length == 560); cleared(data, length); candidate_span = NULL; ++candidate_wipes;
    }
}

void __wrap_zcl_ec_end(zcl_ec_context *context)
{
    assert(seed_span == NULL);
    ++ends;
    __real_zcl_ec_end(context);
    assert(context->handle == NULL && context->storage == NULL && context->storage_len == 0);
}

static void run(fault_kind selected, size_t ordinal, zcl_status expected, size_t expected_children)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, output[562], sentinel[562];
    memset(output, 0xa5, sizeof(output)); memcpy(sentinel, output, sizeof(output));
    fault = selected; fail_at = ordinal;
    begins = seeds = children = ends = seed_wipes = candidate_wipes = 0;
    seed_span = candidate_span = NULL;
    assert(zcl_recovery_address_batch(entropy, sizeof(entropy), ZCL_TESTNET, 1, 19, 16,
        blinding, sizeof(blinding), output + 1, 560) == expected);
    assert(begins == 1 && ends == 1 && children == expected_children);
    assert(seeds == (size_t)(selected != BEGIN && selected != ALLOCATE));
    assert(seed_wipes == seeds);
    assert(candidate_wipes == (size_t)(expected_children != 0));
    assert(seed_span == NULL && candidate_span == NULL);
    assert(output[0] == 0xa5 && output[561] == 0xa5);
    if (expected != ZCL_OK) assert(memcmp(output, sentinel, sizeof(output)) == 0);
    else assert(memcmp(output, sentinel, sizeof(output)) != 0);
}

int main(void)
{
    run(NONE, 0, ZCL_OK, 16);
    run(ALLOCATE, 0, ZCL_RESOURCE_EXHAUSTED, 0);
    run(BEGIN, 0, ZCL_RESOURCE_EXHAUSTED, 0);
    run(SEED, 0, ZCL_CRYPTO_FAILURE, 0);
    for (size_t i = 1; i <= 16; ++i) {
        run(CHILD, i, ZCL_INVALID_CHILD, i);
        run(LENGTH, i, ZCL_CRYPTO_FAILURE, i);
    }
    run(NONE, 0, ZCL_OK, 16);
    puts("recovery batch: 37 ownership/failure/retry cases passed, every partial batch refused");
    return 0;
}
