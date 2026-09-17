/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "source_assessment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Source review at %d\n", __LINE__); abort(); } } while (0)
static source_assessment_fixture fixture;
static zcl_review_owner owner, saved;
static uint8_t draft[ZCL_TX_WIRE_MAX], expected[ZCL_TX_WIRE_MAX];
static size_t draft_length;

static void prepare(unsigned tail)
{
    CHECK(source_assessment_init(&fixture, tail));
    fixture.base.spending.lock_time = UINT32_MAX;
    fixture.base.spending.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    fixture.base.spending.inputs[0].sequence = UINT32_C(0x80000000);
    CHECK(zcl_transaction_serialize(&fixture.base.spending, draft, sizeof(draft), &draft_length) == ZCL_OK);
    memcpy(expected, draft, sizeof(expected));
}

static zcl_status open_review(zcl_network network, uint64_t now, uint64_t *id)
{
    return zcl_review_open_full_sources(&owner, draft, draft_length, network,
        fixture.base.sources, 2, 500, now, id);
}

static void retained(zcl_network network, uint64_t id)
{
    zcl_review_snapshot snapshot;
    CHECK(zcl_review_snapshot_get(&owner, id, 101, &snapshot) == ZCL_OK);
    CHECK(snapshot.remaining_ms == 89999 && snapshot.assessment.network == network);
    CHECK(snapshot.assessment.input_total == 11000 && snapshot.assessment.output_total == 10500);
    CHECK(snapshot.assessment.fee == 500 && snapshot.assessment.maximum_fee == 500);
    CHECK(snapshot.context.lock_time == UINT32_MAX && snapshot.context.expiry_height == ZCL_TX_EXPIRY_LIMIT - 1);
    CHECK(snapshot.context.inputs[0].sequence == UINT32_C(0x80000000));
    CHECK(snapshot.context.inputs[1].previous_index == 1);
    uint8_t wire[ZCL_TX_WIRE_MAX];
    size_t length = SIZE_MAX;
    CHECK(zcl_review_copy_wire(&owner, id, 102, wire, sizeof(wire), &length) == ZCL_OK);
    CHECK(length == draft_length && memcmp(wire, expected, length) == 0);
    memset(wire, 0, sizeof(wire));
    CHECK(zcl_review_copy_wire(&owner, id, 103, wire, sizeof(wire), &length) == ZCL_OK);
    CHECK(memcmp(wire, expected, length) == 0);
}

static void ownership(void)
{
    for (unsigned network = 0; network < 2; ++network) {
        for (unsigned tail = 0; tail < 8; ++tail) {
            prepare(tail);
            uint64_t id = UINT64_MAX;
            if (tail != 0) {
                memcpy(&saved, &owner, sizeof(saved));
                CHECK(zcl_review_open(&owner, draft, draft_length, (zcl_network)network,
                    fixture.base.sources, 2, 500, 100, &id) != ZCL_OK);
                CHECK(id == UINT64_MAX && memcmp(&owner, &saved, sizeof(owner)) == 0);
            }
            CHECK(open_review((zcl_network)network, 100, &id) == ZCL_OK);
            CHECK(memcmp(owner.data.context.inputs[0].previous_txid,
                fixture.base.spending.inputs[0].previous_txid, 32) == 0);
            memset(&fixture, 0xcc, sizeof(fixture)); memset(draft, 0xcc, sizeof(draft));
            retained((zcl_network)network, id);
            CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
            static const zcl_review_data zero;
            CHECK(memcmp(&owner.data, &zero, sizeof(zero)) == 0 && owner.issued == id);
        }
    }
}

static void refused(zcl_status expected_status)
{
    uint64_t id = UINT64_MAX;
    memcpy(&saved, &owner, sizeof(saved));
    const zcl_status status = open_review(ZCL_MAINNET, 100, &id);
    CHECK(status != ZCL_OK && (expected_status == ZCL_OK || status == expected_status));
    CHECK(id == UINT64_MAX && memcmp(&owner, &saved, sizeof(owner)) == 0);
}

static void partial_sources(void)
{
    prepare(7);
    for (size_t index = 0; index < 2; ++index) {
        const size_t length = fixture.base.sources[index].length;
        for (size_t cut = 0; cut < length; ++cut) {
            fixture.base.sources[index].length = cut;
            refused(ZCL_INVALID_ENCODING);
        }
        fixture.base.sources[index].length = length;
        fixture.wire[index][length - 1] ^= 1;
        refused(ZCL_INVALID_ENCODING);
        fixture.wire[index][length - 1] ^= 1;
    }
    uint64_t id;
    CHECK(open_review(ZCL_TESTNET, 100, &id) == ZCL_OK);
    refused(ZCL_BUSY);
    CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
}

static void lifetimes(void)
{
    prepare(7);
    uint64_t first, second;
    CHECK(open_review(ZCL_MAINNET, 100, &first) == ZCL_OK);
    zcl_review_snapshot snapshot;
    CHECK(zcl_review_snapshot_get(&owner, first, 90099, &snapshot) == ZCL_OK && snapshot.remaining_ms == 1);
    CHECK(zcl_review_snapshot_get(&owner, first, 90100, &snapshot) == ZCL_TIMED_OUT);
    prepare(1);
    CHECK(open_review(ZCL_MAINNET, 100, &second) == ZCL_OK && second > first);
    CHECK(zcl_review_cancel(&owner, first) == ZCL_CANCELLED);
    CHECK(zcl_review_snapshot_get(&owner, first, UINT64_MAX, &snapshot) == ZCL_CANCELLED);
    CHECK(zcl_review_snapshot_get(&owner, second, 100, &snapshot) == ZCL_OK);
    CHECK(zcl_review_snapshot_get(&owner, second, 99, &snapshot) == ZCL_CANCELLED);
    prepare(0);
    CHECK(zcl_review_open(&owner, draft, draft_length, ZCL_MAINNET,
        fixture.base.sources, 2, 500, 100, &first) == ZCL_OK && first > second);
    CHECK(zcl_review_cancel(&owner, first) == ZCL_OK);
}

int main(void)
{
    ownership(); partial_sources(); lifetimes();
    puts("Full-source review owns exact draft data and preserves cancellation, expiry and legacy isolation");
    return 0;
}
