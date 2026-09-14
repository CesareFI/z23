/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "transaction_review_internal.h"
#include "transaction_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review context fault at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_review_owner owner, saved;
static zcl_review_block supplied;
static uint8_t wire[ZCL_TX_WIRE_MAX];
static uint32_t *branch_span;
static uint8_t *digest_span;
static unsigned failure, calls, wipes;

zcl_status zcl_review_context_test_branch(zcl_network network, uint32_t height, uint32_t *branch)
{
    CHECK(calls++ == 0 && wipes == 0 && network == ZCL_MAINNET && height == 1000000);
    CHECK(branch != NULL && *branch == 0 && branch_span == NULL);
    branch_span = branch;
    *branch = UINT32_C(0x930b540d); /* Plausible even on failure. */
    supplied.network = ZCL_TESTNET;
    supplied.height = 0;
    supplied.lock_time_cutoff = UINT64_MAX;
    return failure == 1 ? ZCL_UNSUPPORTED : ZCL_OK;
}

zcl_status zcl_review_context_test_hash(zcl_review_owner *review, uint64_t id, uint64_t now,
    size_t index, uint32_t branch, uint8_t *digest, size_t capacity)
{
    CHECK(calls++ == 1 && wipes == 0 && review == &owner);
    CHECK(id == saved.data.id && now == 100 && index == 0 && branch == UINT32_C(0x930b540d));
    CHECK(digest != NULL && capacity == 32 && digest_span == NULL);
    for (size_t i = 0; i < 32; ++i) CHECK(digest[i] == 0);
    digest_span = digest;
    memset(digest, 0x6a, 32);
    return failure == 2 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

void zcl_review_context_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && length >= sizeof(zcl_review_block) + 4 + 32 && length <= 128);
    CHECK(wipes++ == 0);
    memset(buffer, 0, length);
    /* Inspect and retire only while these captured subobjects remain live. */
    if (branch_span != NULL) { CHECK(*branch_span == 0); branch_span = NULL; }
    if (digest_span != NULL) {
        for (size_t i = 0; i < 32; ++i) CHECK(digest_span[i] == 0);
        digest_span = NULL;
    }
}

static void run(unsigned ordinal)
{
    struct { uint8_t before[8], digest[64], after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(branch_span == NULL && digest_span == NULL);
    failure = ordinal; calls = 0; wipes = 0;
    supplied = (zcl_review_block){ZCL_MAINNET, 1000000, 0};
    static const zcl_status statuses[] = {ZCL_OK, ZCL_UNSUPPORTED, ZCL_CRYPTO_FAILURE};
    CHECK(ordinal < sizeof(statuses) / sizeof(statuses[0]));
    CHECK(zcl_review_sighash_context(&owner, saved.data.id, 100, 0, &supplied, box.digest, 64) == statuses[ordinal]);
    CHECK(calls == (ordinal == 1 ? 1U : 2U) && wipes == 1);
    CHECK(branch_span == NULL && digest_span == NULL && supplied.height == 0);
    CHECK(memcmp(&owner, &saved, sizeof(saved)) == 0);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    for (size_t i = 0; i < 64; ++i) CHECK(box.digest[i] == (i < 32 && ordinal == 0 ? 0x6a : 0xa5));
}

static void early_refusals(void)
{
    calls = 0; wipes = 0;
    supplied = (zcl_review_block){ZCL_MAINNET, 1000000, 0};
    uint8_t digest[32] = {0};
    CHECK(zcl_review_sighash_context(&owner, saved.data.id, 100, 0, &supplied, digest, 31) == ZCL_BUFFER_TOO_SMALL);
    CHECK(zcl_review_sighash_context(&owner, saved.data.id, 100, SIZE_MAX, &supplied, digest, 32) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_review_sighash_context(&owner, saved.data.id + 1, UINT64_MAX, 0, &supplied, digest, 32) == ZCL_CANCELLED);
    CHECK(calls == 0 && wipes == 0);
    supplied.network = ZCL_TESTNET;
    CHECK(zcl_review_sighash_context(&owner, saved.data.id, 100, 0, &supplied, digest, 32) == ZCL_UNSUPPORTED);
    CHECK(calls == 0 && wipes == 1);
    CHECK(memcmp(&owner, &saved, sizeof(saved)) == 0);
}

int main(void)
{
    CHECK(assessment_fixture_init(&fixture));
    fixture.spending.expiry_height = 1000000;
    fixture.spending.lock_time = 999999;
    fixture.spending.inputs[1].sequence = 0;
    size_t length = 0;
    CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &length) == ZCL_OK);
    uint64_t id = 0;
    CHECK(zcl_review_open(&owner, wire, length, ZCL_MAINNET, fixture.sources, 2, 500, 100, &id) == ZCL_OK);
    memcpy(&saved, &owner, sizeof(saved));
    for (unsigned ordinal = 0; ordinal < 3; ++ordinal) run(ordinal);
    early_refusals();
    zcl_review_clear(&owner);
    CHECK(puts("Review context errors preserve caller output and clear private branch/digest work") >= 0);
    return 0;
}
