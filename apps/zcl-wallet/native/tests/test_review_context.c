/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "context_reference.h"
#include "transaction_review_internal.h"
#include "transaction_sighash.h"
#ifdef ZCL_SIGHASH_ORACLE
#include "sighash_oracle.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review context check at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_review_owner owner, saved;
static uint8_t wire[ZCL_TX_WIRE_MAX], expected[32];
static size_t wire_length;
static uint64_t id;

static void reset(void)
{
    zcl_review_clear(&owner);
    CHECK(assessment_fixture_init(&fixture));
}

static void open_review(zcl_network network)
{
    CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &wire_length) == ZCL_OK);
    CHECK(zcl_review_open(&owner, wire, wire_length, network, fixture.sources,
        fixture.spending.input_count, 500, 100, &id) == ZCL_OK);
    memcpy(&saved, &owner, sizeof(saved));
}

static void expected_hash(uint32_t branch)
{
    const zcl_tx_output *previous = &fixture.previous[0].outputs[0];
    CHECK(zcl_transaction_sighash_all(&fixture.spending, 0, previous->script, previous->script_len,
        previous->value, branch, expected, 32) == ZCL_OK);
#ifdef ZCL_SIGHASH_ORACLE
    uint8_t independent[32];
    zcl_test_sighash_all(wire, wire_length, 0, previous->script, previous->script_len,
        previous->value, branch, independent, 32);
    CHECK(memcmp(expected, independent, 32) == 0);
#endif
}

static void call(uint64_t request, uint64_t now, size_t input, const zcl_review_block *block,
    size_t capacity, zcl_status status)
{
    struct { uint8_t before[8], digest[64], after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_review_sighash_context(&owner, request, now, input, block, box.digest, capacity) == status);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    if (status == ZCL_OK) CHECK(memcmp(expected, box.digest, 32) == 0);
    for (size_t i = status == ZCL_OK ? 32 : 0; i < 64; ++i) CHECK(box.digest[i] == 0xa5);
}

static void activation_boundaries(void)
{
    static const uint32_t heights[] = {0, 19, 20, 21, 6349, 6350, 6351, 78855, 78856, 78857,
        476968, 476969, 476970, 585317, 585318, 585319, 585321, 585322, 585323,
        706999, 707000, 707001, INT32_MAX, UINT32_MAX};
    for (size_t network = 0; network < 2; ++network) {
        for (size_t i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
            reset(); open_review((zcl_network)network);
            zcl_review_block block = {(zcl_network)network, heights[i], 0};
            uint32_t branch = 0;
            const zcl_status status = zcl_test_context_branch(block.network, block.height, &branch);
            if (status == ZCL_OK) expected_hash(branch);
            memset(&fixture, 0, sizeof(fixture));
            memset(wire, 0xff, sizeof(wire));
            for (size_t capacity = 0; capacity <= 64; ++capacity)
                call(id, 100, 0, &block, capacity, capacity < 32 ? ZCL_BUFFER_TOO_SMALL : status);
            CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
        }
    }
}

static void expiry(void)
{
    static const uint32_t values[] = {0, 999999, 1000000, 1000001, ZCL_TX_EXPIRY_LIMIT - 1};
    zcl_review_block block = {ZCL_MAINNET, 1000000, 0};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        reset(); fixture.spending.expiry_height = values[i]; open_review(ZCL_MAINNET);
        expected_hash(UINT32_C(0x930b540d));
        memset(&fixture, 0, sizeof(fixture));
        call(id, 100, 0, &block, 64, values[i] == 999999 ? ZCL_OUT_OF_RANGE : ZCL_OK);
        CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
    }
}

static void lock_case(uint32_t lock, uint32_t height, uint64_t cutoff, bool all_final, bool final)
{
    reset();
    fixture.spending.lock_time = lock;
    /* Place the nonfinal sequence in the OTHER input, not the hashed input. */
    fixture.spending.inputs[1].sequence = all_final ? UINT32_MAX : 0;
    open_review(ZCL_MAINNET);
    expected_hash(UINT32_C(0x930b540d));
    memset(&fixture, 0, sizeof(fixture));
    zcl_review_block block = {ZCL_MAINNET, height, cutoff};
    call(id, 100, 0, &block, 64, final ? ZCL_OK : ZCL_OUT_OF_RANGE);
    CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
}

static void lock_boundaries(void)
{
    lock_case(0, 1000000, 0, false, true);
    lock_case(999999, 1000000, 0, false, true);
    lock_case(1000000, 1000000, 0, false, false);
    lock_case(1000001, 1000000, UINT32_MAX, false, false);
    lock_case(1000001, 1000000, 0, true, true);
    lock_case(499999999, 499999999, UINT32_MAX, false, false);
    lock_case(499999999, 500000000, 0, false, true);
    lock_case(500000000, 600000000, 499999999, false, false);
    lock_case(500000000, 600000000, 500000000, false, false);
    lock_case(500000000, 1000000, 500000001, false, true);
    lock_case(UINT32_MAX, 1000000, UINT32_MAX, false, false);
    lock_case(UINT32_MAX, 1000000, UINT64_C(4294967296), false, true);
    lock_case(UINT32_MAX, 1000000, INT64_MAX, false, true);
    lock_case(UINT32_MAX, 1000000, 0, true, true);
    lock_case(0, 1000000, UINT64_MAX, true, false);
}

static void bounds_and_lifetime(void)
{
    reset(); open_review(ZCL_MAINNET); expected_hash(UINT32_C(0x930b540d));
    zcl_review_block block = {ZCL_MAINNET, 1000000, 0};
    call(id, UINT64_MAX, 0, NULL, 64, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_sighash_context(NULL, id, 100, 0, &block, expected, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_sighash_context(&owner, id, UINT64_MAX, 0, &block, NULL, 32) == ZCL_INVALID_ARGUMENT);
    call(id, 100, 1, &block, 64, ZCL_UNSUPPORTED);
    call(id, 100, 2, &block, 64, ZCL_OUT_OF_RANGE);
    call(id, 100, SIZE_MAX, &block, 64, ZCL_OUT_OF_RANGE);
    block.network = ZCL_TESTNET;
    call(id, 100, 0, &block, 64, ZCL_UNSUPPORTED);
    CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
    block.network = ZCL_MAINNET;
    call(id, 90099, 0, &block, 64, ZCL_OK);
    CHECK(owner.data.last_ms == 90099 && owner.data.deadline_ms == 90100);
    call(id, 90100, 0, &block, 64, ZCL_TIMED_OUT);
    CHECK(owner.data.id == 0);
    const uint64_t old = id;
    reset(); open_review(ZCL_MAINNET);
    call(old, UINT64_MAX, 0, &block, 64, ZCL_CANCELLED);
    call(0, 0, 0, &block, 64, ZCL_CANCELLED);
    CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
    block.height = UINT32_MAX;
    call(id, 101, 0, &block, 64, ZCL_OUT_OF_RANGE);
    CHECK(owner.data.last_ms == 101 && owner.data.deadline_ms == 90100);
    call(id, 100, 0, &block, 64, ZCL_CANCELLED);
    CHECK(owner.data.id == 0);
}

static void maximum_finality(void)
{
    reset();
    zcl_transparent_tx *previous = &fixture.previous[0];
    previous->output_count = ZCL_TX_INPUT_MAX;
    for (size_t i = 1; i < ZCL_TX_INPUT_MAX; ++i) {
        previous->outputs[i] = previous->outputs[0];
        previous->outputs[i].value = 0;
    }
    CHECK(assessment_fixture_rebind(&fixture, 0));
    fixture.spending.input_count = ZCL_TX_INPUT_MAX;
    fixture.spending.outputs[0].value = 9500;
    fixture.spending.outputs[1].value = 0;
    fixture.spending.lock_time = 1000000;
    for (size_t i = 1; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture.spending.inputs[i] = fixture.spending.inputs[0];
        fixture.spending.inputs[i].previous_index = (uint32_t)i;
        fixture.sources[i] = fixture.sources[0];
    }
    fixture.spending.inputs[7].sequence = UINT32_MAX - 1;
    open_review(ZCL_MAINNET);
    zcl_review_block block = {ZCL_MAINNET, 1000000, 0};
    call(id, 100, 0, &block, 64, ZCL_OUT_OF_RANGE);
    zcl_review_clear(&owner);
    fixture.spending.inputs[7].sequence = UINT32_MAX;
    open_review(ZCL_MAINNET);
    expected_hash(UINT32_C(0x930b540d));
    memset(&fixture, 0, sizeof(fixture));
    call(id, 100, 0, &block, 64, ZCL_OK);
    CHECK(memcmp(&saved, &owner, sizeof(saved)) == 0);
}

int main(void)
{
    activation_boundaries();
    expiry();
    lock_boundaries();
    bounds_and_lifetime();
    maximum_finality();
    zcl_review_clear(&owner);
    CHECK(puts("Live review digests bind pinned candidate branch and owned expiry/finality fields") >= 0);
    return 0;
}
