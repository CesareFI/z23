/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Transaction signing fault at %d\n", __LINE__); abort(); } } while (0)
#undef zcl_secure_zero
void zcl_secure_zero(void *, size_t);

static zcl_review_owner owner;
static zcl_review_wallet_input claims[ZCL_TX_INPUT_MAX];
static const zcl_review_wallet_input *copied;
static const zcl_signature *staged;
static unsigned checks, signs, completions, claim_wipes, work_wipes;
static size_t count, failure;
static unsigned mode;

static void zeroed(const void *pointer, size_t length)
{
    const uint8_t *bytes = pointer;
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}

static zcl_status sample(void *context, uint64_t *now)
{
    CHECK(context == NULL && now != NULL);
    *now = 100;
    return ZCL_OK;
}

zcl_status zcl_batch_test_check(zcl_review_owner *review, uint64_t id, uint64_t now,
    size_t index, const zcl_review_wallet_input *claim)
{
    CHECK(review == &owner && id == 1 && now == 100 && index == checks);
    CHECK(signs == 0 && claim_wipes == 0 && claim != &claims[index]);
    if (index == 0) copied = claim;
    CHECK(claim == &copied[index] && claim->index == index);
    ++checks;
    return mode == 1 && index == failure ? ZCL_NOT_FOUND : ZCL_OK;
}

zcl_status zcl_batch_test_sign(zcl_review_owner *review, uint64_t id,
    const zcl_review_clock *clock, size_t index, const zcl_review_block *block,
    const zcl_review_wallet_input *claim, zcl_signature *signature)
{
    CHECK(review == &owner && id == 1 && clock->read == sample && index == signs);
    CHECK(checks == count && claim_wipes == 0 && block->height == 100000);
    CHECK(claim == &copied[index] && claim->index == index);
    if (index == 0) staged = signature;
    CHECK(signature == &staged[index]);
    zeroed(signature, sizeof(*signature));
    memset(signature, (int)(index + 1), sizeof(*signature));
    ++signs;
    return mode == 2 && index == failure ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

zcl_status zcl_batch_test_complete(zcl_review_owner *review, uint64_t id,
    const zcl_review_clock *clock, const zcl_review_block *block,
    const zcl_signature *signatures, size_t signature_count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    CHECK(review == &owner && id == 1 && clock->read == sample && block->height == 100000);
    CHECK(signs == count && checks == count && claim_wipes == 1 && copied == NULL);
    CHECK(signatures == staged && signature_count == count && capacity == 8);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *bytes = (const uint8_t *)&signatures[i];
        for (size_t j = 0; j < sizeof(*signatures); ++j) CHECK(bytes[j] == i + 1);
    }
    ++completions;
    if (mode == 3) return ZCL_INVALID_ENCODING;
    wire[0] = 0x42; *length = 1;
    return ZCL_OK;
}

void zcl_batch_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL);
    zcl_secure_zero(pointer, length);
    zeroed(pointer, length);
    if (length == sizeof(claims)) {
        CHECK(pointer == copied && claim_wipes++ == 0);
        copied = NULL;
    } else {
        CHECK(length > sizeof(claims) && length <= 4096 && claim_wipes == 1);
        if (staged != NULL) zeroed(staged, count * sizeof(*staged));
        staged = NULL; ++work_wipes;
    }
}

static void run(unsigned selected, size_t at)
{
    mode = selected; failure = at;
    checks = signs = completions = claim_wipes = work_wipes = 0;
    copied = NULL; staged = NULL;
    memset(&owner, 0, sizeof(owner));
    owner.data.id = 1; owner.data.last_ms = 100; owner.data.deadline_ms = 90100;
    owner.data.assessment.input_count = count;
    for (size_t i = 0; i < count; ++i) claims[i].index = (uint32_t)i;
    const zcl_review_clock clock = {sample, NULL};
    const zcl_review_block block = {ZCL_TESTNET, 100000, 0};
    uint8_t wire[8]; memset(wire, 0xa5, sizeof(wire)); size_t length = SIZE_MAX;
    const zcl_status statuses[] = {ZCL_OK, ZCL_NOT_FOUND, ZCL_CRYPTO_FAILURE, ZCL_INVALID_ENCODING};
    CHECK(zcl_review_wallet_transaction_sign(&owner, 1, &clock, &block, claims,
        count, wire, sizeof(wire), &length) == statuses[mode]);
    CHECK(claim_wipes == 1 && work_wipes == 1 && copied == NULL && staged == NULL);
    CHECK(signs == (mode == 1 ? 0 : mode == 2 ? at + 1 : count));
    CHECK(completions == (mode == 0 || mode == 3 ? 1u : 0u));
    CHECK(length == (mode == 0 ? 1 : SIZE_MAX));
    for (size_t i = 0; i < sizeof(wire); ++i) CHECK(wire[i] == (mode == 0 && i == 0 ? 0x42 : 0xa5));
}

int main(void)
{
    for (count = 1; count <= ZCL_TX_INPUT_MAX; ++count) {
        run(0, 0); run(3, 0);
        for (size_t i = 0; i < count; ++i) { run(1, i); run(2, i); }
    }
    puts("Complete signing faults: preflight, dirty partial signatures and scratch retirement passed");
    return 0;
}
