/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "transaction_review_internal.h"
#include "transaction_sighash.h"
#ifdef ZCL_SIGHASH_ORACLE
#include "sighash_oracle.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review sighash check failed at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_review_owner owner, before;
static uint8_t wire[ZCL_TX_WIRE_MAX];
static size_t wire_length;
static uint8_t expected[ZCL_TX_INPUT_MAX][32];

static void reset_fixture(void)
{
    CHECK(assessment_fixture_init(&fixture));
    memset(&owner, 0, sizeof(owner)); /* Isolated owner, no retained IDs. */
}

static uint64_t open_fixture(zcl_network network, uint64_t now)
{
    CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &wire_length) == ZCL_OK);
    uint64_t id = 0;
    CHECK(zcl_review_open(&owner, wire, wire_length, network, fixture.sources,
        fixture.spending.input_count, 500, now, &id) == ZCL_OK);
    return id;
}

static void expected_hash(size_t index, uint32_t branch)
{
    zcl_tx_output previous;
    CHECK(zcl_transaction_prevout(&fixture.spending.inputs[index], fixture.sources[index].wire,
        fixture.sources[index].length, &previous) == ZCL_OK);
    CHECK(zcl_transaction_sighash_all(&fixture.spending, index, previous.script, previous.script_len,
        previous.value, branch, expected[index], sizeof(expected[index])) == ZCL_OK);
#ifdef ZCL_SIGHASH_ORACLE
    uint8_t independent[32];
    zcl_test_sighash_all(wire, wire_length, index, previous.script, previous.script_len,
        previous.value, branch, independent, sizeof(independent));
    CHECK(memcmp(expected[index], independent, sizeof(independent)) == 0);
#endif
}

static void check_call(uint64_t id, uint64_t now, size_t index, uint32_t branch,
                       size_t capacity, zcl_status status)
{
    struct { uint8_t before[8]; uint8_t digest[64]; uint8_t after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_review_sighash_p2pkh(&owner, id, now, index, branch, box.digest, capacity) == status);
    for (size_t i = 0; i < sizeof(box.before); ++i)
        CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    if (status == ZCL_OK) CHECK(memcmp(box.digest, expected[index], 32) == 0);
    for (size_t i = status == ZCL_OK ? 32 : 0; i < sizeof(box.digest); ++i) CHECK(box.digest[i] == 0xa5);
}

static void immutable_inputs(void)
{
    static const uint32_t branches[] = {0, UINT32_C(0x5ba81b19), UINT32_C(0x76b809bb), UINT32_MAX};
    for (size_t network = 0; network < 2; ++network) {
        for (size_t b = 0; b < sizeof(branches) / sizeof(branches[0]); ++b) {
            reset_fixture();
            const uint64_t id = open_fixture((zcl_network)network, 100);
            expected_hash(0, branches[b]);
            /* Neither source bytes nor mutable public copies remain authoritative. */
            memset(&fixture, 0, sizeof(fixture));
            memset(wire, 0xff, sizeof(wire));
            zcl_review_snapshot copy;
            CHECK(zcl_review_snapshot_get(&owner, id, 100, &copy) == ZCL_OK);
            memset(&copy, 0x6a, sizeof(copy));
            size_t copied = 0;
            CHECK(zcl_review_copy_wire(&owner, id, 100, wire, sizeof(wire), &copied) == ZCL_OK);
            memset(wire, 0x6a, sizeof(wire));
            memcpy(&before, &owner, sizeof(before));
            for (size_t capacity = 0; capacity <= 64; ++capacity) {
                check_call(id, 100, 0, branches[b], capacity, capacity < 32 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK);
                CHECK(memcmp(&owner, &before, sizeof(owner)) == 0);
            }
            check_call(id, 100, 1, branches[b], 64, ZCL_UNSUPPORTED); /* P2SH. */
            check_call(id, 100, 2, branches[b], 64, ZCL_OUT_OF_RANGE);
            check_call(id, 100, SIZE_MAX, branches[b], 64, ZCL_OUT_OF_RANGE);
            zcl_review_clear(&owner);
        }
    }
}

static void maximum_profile(void)
{
    reset_fixture();
    fixture.previous[0].output_count = ZCL_TX_OUTPUT_MAX;
    const zcl_tx_output destination = fixture.previous[0].outputs[0];
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.previous[0].outputs[i] = destination;
        fixture.previous[0].outputs[i].value = i == 8 ? ZCL_MAX_MONEY - 7 : (i > 8 ? 1 : 0);
    }
    CHECK(assessment_fixture_rebind(&fixture, 0));
    fixture.spending.input_count = ZCL_TX_INPUT_MAX;
    fixture.spending.output_count = ZCL_TX_OUTPUT_MAX;
    fixture.spending.lock_time = UINT32_MAX;
    fixture.spending.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture.spending.inputs[i] = fixture.spending.inputs[0];
        fixture.spending.inputs[i].previous_index = (uint32_t)(8 + i);
        fixture.spending.inputs[i].sequence = UINT32_MAX - (uint32_t)i;
        fixture.sources[i] = fixture.sources[0];
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.spending.outputs[i] = destination;
        fixture.spending.outputs[i].value = i == 0 ? ZCL_MAX_MONEY - 515 : 1;
    }
    const uint64_t id = open_fixture(ZCL_TESTNET, 100);
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) expected_hash(i, UINT32_MAX);
    memset(&fixture, 0, sizeof(fixture));
    memset(wire, 0, sizeof(wire));
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i)
        for (size_t capacity = 0; capacity <= 64; ++capacity)
            check_call(id, 100, i, UINT32_MAX, capacity, capacity < 32 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK);
    check_call(id, 100, ZCL_TX_INPUT_MAX, 0, 64, ZCL_OUT_OF_RANGE);
    zcl_review_clear(&owner);
}

static void boundary_amounts(void)
{
    static const uint64_t amounts[] = {0, 1, ZCL_MAX_MONEY};
    for (size_t i = 0; i < sizeof(amounts) / sizeof(amounts[0]); ++i) {
        reset_fixture();
        fixture.previous[0].outputs[0].value = amounts[i];
        fixture.previous[0].outputs[1].value = 0;
        fixture.previous[1].outputs[1].value = amounts[i] == ZCL_MAX_MONEY ? 0 : 1500;
        fixture.spending.outputs[0].value = amounts[i] == ZCL_MAX_MONEY ? ZCL_MAX_MONEY - 500 : amounts[i] + 1000;
        fixture.spending.outputs[1].value = 0;
        CHECK(assessment_fixture_rebind(&fixture, 0));
        CHECK(assessment_fixture_rebind(&fixture, 1));
        const uint64_t id = open_fixture(ZCL_MAINNET, 100);
        expected_hash(0, UINT32_C(0x76b809bb));
        memset(&fixture, 0, sizeof(fixture));
        memset(wire, 0, sizeof(wire));
        check_call(id, 100, 0, UINT32_C(0x76b809bb), 64, ZCL_OK);
        zcl_review_clear(&owner);
    }
}

static void cleared(uint64_t issued)
{
    static const zcl_review_data zero;
    CHECK(owner.issued == issued && memcmp(&owner.data, &zero, sizeof(zero)) == 0);
}

static void lifetime(void)
{
    reset_fixture();
    uint64_t id = open_fixture(ZCL_MAINNET, 100);
    expected_hash(0, 0);
    check_call(id, 90099, 0, 0, 64, ZCL_OK);
    CHECK(owner.data.last_ms == 90099 && owner.data.deadline_ms == 90100);
    check_call(id, 90100, 0, 0, 64, ZCL_TIMED_OUT);
    cleared(id);
    check_call(id, 90100, 0, 0, 64, ZCL_CANCELLED);
    const uint64_t old_id = id;
    id = open_fixture(ZCL_MAINNET, 100000);
    CHECK(id == old_id + 1);
    memcpy(&before, &owner, sizeof(before));
    check_call(old_id, UINT64_MAX, 0, 0, 64, ZCL_CANCELLED);
    check_call(0, UINT64_MAX, 0, 0, 64, ZCL_CANCELLED);
    check_call(UINT64_MAX, 0, 0, 0, 64, ZCL_CANCELLED);
    CHECK(memcmp(&owner, &before, sizeof(owner)) == 0);
    check_call(id, 100001, 0, 0, 0, ZCL_BUFFER_TOO_SMALL);
    CHECK(owner.data.last_ms == 100001 && owner.data.deadline_ms == 190000);
    check_call(id, 100000, 0, 0, 64, ZCL_CANCELLED); /* Rollback after refused live read. */
    cleared(id);
    id = open_fixture(ZCL_MAINNET, 200000);
    CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
    check_call(id, 200000, 0, 0, 64, ZCL_CANCELLED);
    cleared(id);
}

static void pointer_refusals(void)
{
    reset_fixture();
    const uint64_t id = open_fixture(ZCL_MAINNET, 100);
    memcpy(&before, &owner, sizeof(before));
    uint8_t digest[32];
    memset(digest, 0xa5, sizeof(digest));
    CHECK(zcl_review_sighash_p2pkh(NULL, id, 100, 0, 0, digest, sizeof(digest)) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_sighash_p2pkh(&owner, id, UINT64_MAX, 0, 0, NULL, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(&owner, &before, sizeof(owner)) == 0);
    for (size_t i = 0; i < sizeof(digest); ++i) CHECK(digest[i] == 0xa5);
    zcl_review_clear(&owner);
}

int main(void)
{
    immutable_inputs();
    maximum_profile();
    boundary_amounts();
    lifetime();
    pointer_refusals();
    puts("Review-bound P2PKH SIGHASH_ALL tests passed");
    return 0;
}
