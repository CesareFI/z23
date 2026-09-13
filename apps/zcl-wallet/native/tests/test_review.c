/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "zcl_transaction_review.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review check failed at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static uint8_t draft[ZCL_TX_WIRE_MAX];
static size_t draft_length;
static zcl_review_owner owner;

static void encode_draft(void)
{
    CHECK(zcl_transaction_serialize(&fixture.spending, draft, sizeof(draft), &draft_length) == ZCL_OK);
}

static void reset_fixture(void)
{
    CHECK(assessment_fixture_init(&fixture));
    memset(&owner, 0, sizeof(owner)); /* New isolated owner; no outstanding callbacks. */
    encode_draft();
}

static zcl_status open_draft(uint64_t now, uint64_t *id)
{
    return zcl_review_open(&owner, draft, draft_length, ZCL_MAINNET, fixture.sources, 2, 500, now, id);
}

static void cleared(uint64_t issued)
{
    static const zcl_review_data zero;
    CHECK(memcmp(&owner.data, &zero, sizeof(zero)) == 0);
    CHECK(owner.issued == issued);
}

static void read_refused(uint64_t id, uint64_t now, zcl_status expected)
{
    zcl_review_snapshot snapshot, original;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    memcpy(&original, &snapshot, sizeof(original));
    CHECK(zcl_review_snapshot_get(&owner, id, now, &snapshot) == expected);
    CHECK(memcmp(&snapshot, &original, sizeof(snapshot)) == 0);
}

static void copy_refused(uint64_t id, uint64_t now, size_t capacity, zcl_status expected)
{
    uint8_t wire[ZCL_TX_WIRE_MAX], original[ZCL_TX_WIRE_MAX];
    memset(wire, 0xa5, sizeof(wire));
    memcpy(original, wire, sizeof(original));
    size_t length = SIZE_MAX;
    CHECK(zcl_review_copy_wire(&owner, id, now, wire, capacity, &length) == expected);
    CHECK(memcmp(wire, original, sizeof(wire)) == 0 && length == SIZE_MAX);
}

static void immutable_draft(void)
{
    reset_fixture();
    uint64_t id = 99;
    CHECK(open_draft(1000, &id) == ZCL_OK && id == 1);
    uint8_t expected[ZCL_TX_WIRE_MAX];
    memcpy(expected, draft, draft_length);
    memset(draft, 0xff, sizeof(draft));
    memset(&fixture, 0, sizeof(fixture)); /* No borrowed pointer may survive open. */
    struct { uint64_t before; zcl_review_snapshot value; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_review_snapshot_get(&owner, id, 1000, &box.value) == ZCL_OK);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
    CHECK(box.value.remaining_ms == 90000 && box.value.assessment.fee == 500);
    CHECK(box.value.assessment.input_total == 11000 && box.value.assessment.output_total == 10500);
    CHECK(box.value.assessment.network == ZCL_MAINNET && box.value.assessment.maximum_fee == 500);
    CHECK(box.value.assessment.serialized_size == draft_length);
    struct { uint64_t before; uint8_t value[ZCL_TX_WIRE_MAX]; uint64_t after; } bytes;
    memset(&bytes, 0xa5, sizeof(bytes));
    size_t length = 0;
    CHECK(zcl_review_copy_wire(&owner, id, 1001, bytes.value, draft_length, &length) == ZCL_OK);
    CHECK(length == draft_length && memcmp(bytes.value, expected, length) == 0);
    CHECK(bytes.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && bytes.after == bytes.before);
    for (size_t i = length; i < sizeof(bytes.value); ++i) CHECK(bytes.value[i] == 0xa5);
    memset(&box.value, 0, sizeof(box.value));
    memset(bytes.value, 0, sizeof(bytes.value));
    CHECK(zcl_review_snapshot_get(&owner, id, 1002, &box.value) == ZCL_OK);
    CHECK(box.value.assessment.fee == 500 && box.value.remaining_ms == 89998);
    CHECK(zcl_review_copy_wire(&owner, id, 1002, bytes.value, sizeof(bytes.value), &length) == ZCL_OK);
    CHECK(memcmp(bytes.value, expected, length) == 0);
    zcl_review_clear(&owner);
    cleared(1);
}

static void opening_refused(const uint8_t *wire, size_t length, size_t previous_count,
                            uint64_t fee, uint64_t now, zcl_status expected)
{
    zcl_review_owner original;
    memcpy(&original, &owner, sizeof(original));
    uint64_t id = 1234;
    const zcl_status status = zcl_review_open(&owner, wire, length, ZCL_MAINNET,
        fixture.sources, previous_count, fee, now, &id);
    CHECK(status != ZCL_OK && (expected == ZCL_OK || status == expected));
    CHECK(id == 1234 && memcmp(&owner, &original, sizeof(owner)) == 0);
}

static void opening_atomicity(void)
{
    reset_fixture();
    for (size_t cut = 0; cut < draft_length; ++cut)
        opening_refused(draft, cut, 2, 500, 100, ZCL_OK);
    opening_refused(NULL, draft_length, 2, 500, 100, ZCL_INVALID_ARGUMENT);
    opening_refused(draft, SIZE_MAX, 2, 500, 100, ZCL_RESOURCE_EXHAUSTED);
    opening_refused(draft, draft_length, 1, 500, 100, ZCL_INVALID_ARGUMENT);
    opening_refused(draft, draft_length, SIZE_MAX, 500, 100, ZCL_INVALID_ARGUMENT);
    opening_refused(draft, draft_length, 2, 499, 100, ZCL_OUT_OF_RANGE);
    fixture.wire[1][0] ^= 1;
    opening_refused(draft, draft_length, 2, 500, 100, ZCL_OK);
    fixture.wire[1][0] ^= 1;
    fixture.spending.inputs[1].script_len = 1;
    fixture.spending.inputs[1].script[0] = 0x51;
    encode_draft();
    opening_refused(draft, draft_length, 2, 500, 100, ZCL_UNSUPPORTED);
    fixture.spending.inputs[1].script_len = 0;
    encode_draft();
    uint64_t id = 0;
    CHECK(open_draft(100, &id) == ZCL_OK && id == 1);
    opening_refused(draft, draft_length, 2, 500, UINT64_MAX, ZCL_BUSY);
    /* Expired active state still requires an expiry read or explicit cancel. */
    opening_refused(draft, draft_length, 2, 500, 90100, ZCL_BUSY);
    read_refused(id, 90100, ZCL_TIMED_OUT);
    cleared(1);
}

static void stale_callbacks(void)
{
    reset_fixture();
    uint64_t previous = 0;
    for (uint64_t round = 1; round <= 12; ++round) {
        uint64_t id = 0;
        CHECK(open_draft(100, &id) == ZCL_OK && id == round);
        zcl_review_owner original;
        memcpy(&original, &owner, sizeof(original));
        const uint64_t invalid[] = {0, previous, ZCL_REVIEW_ID_MAX + 1, UINT64_MAX, id + 1};
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            read_refused(invalid[i], UINT64_MAX, ZCL_CANCELLED);
            copy_refused(invalid[i], 0, sizeof(draft), ZCL_CANCELLED);
            CHECK(zcl_review_cancel(&owner, invalid[i]) == ZCL_CANCELLED);
            CHECK(memcmp(&owner, &original, sizeof(owner)) == 0);
        }
        CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
        cleared(round);
        CHECK(zcl_review_cancel(&owner, id) == ZCL_CANCELLED);
        previous = id;
    }
    uint64_t id = 0;
    CHECK(open_draft(100, &id) == ZCL_OK && id == 13);
    zcl_review_clear(&owner);
    zcl_review_clear(&owner);
    cleared(13);
    read_refused(id, 100, ZCL_CANCELLED);
}

static void deadlines(void)
{
    reset_fixture();
    uint64_t id = 0;
    zcl_review_snapshot snapshot;
    CHECK(open_draft(0, &id) == ZCL_OK);
    CHECK(zcl_review_snapshot_get(&owner, id, 0, &snapshot) == ZCL_OK && snapshot.remaining_ms == 90000);
    CHECK(zcl_review_snapshot_get(&owner, id, 89999, &snapshot) == ZCL_OK && snapshot.remaining_ms == 1);
    CHECK(zcl_review_snapshot_get(&owner, id, 89999, &snapshot) == ZCL_OK && snapshot.remaining_ms == 1);
    copy_refused(id, 90000, sizeof(draft), ZCL_TIMED_OUT);
    cleared(1);
    CHECK(open_draft(100, &id) == ZCL_OK && id == 2);
    CHECK(zcl_review_snapshot_get(&owner, id, 200, &snapshot) == ZCL_OK);
    read_refused(id, 199, ZCL_CANCELLED);
    cleared(2);
    CHECK(open_draft(300, &id) == ZCL_OK);
    copy_refused(id, 301, 0, ZCL_BUFFER_TOO_SMALL);
    CHECK(owner.data.last_ms == 301 && owner.data.deadline_ms == 90300);
    copy_refused(id, 300, sizeof(draft), ZCL_CANCELLED);
    cleared(3);
    opening_refused(draft, draft_length, 2, 500, UINT64_MAX - 89999, ZCL_OUT_OF_RANGE);
    CHECK(open_draft(UINT64_MAX - 90000, &id) == ZCL_OK && id == 4);
    CHECK(zcl_review_snapshot_get(&owner, id, UINT64_MAX - 1, &snapshot) == ZCL_OK);
    CHECK(snapshot.remaining_ms == 1);
    read_refused(id, UINT64_MAX, ZCL_TIMED_OUT);
    cleared(4);
}

static void capacity_and_issuance(void)
{
    reset_fixture();
    uint64_t id = 0;
    CHECK(open_draft(100, &id) == ZCL_OK);
    for (size_t capacity = 0; capacity < draft_length; ++capacity)
        copy_refused(id, 100, capacity, ZCL_BUFFER_TOO_SMALL);
    CHECK(owner.data.last_ms == 100 && owner.data.deadline_ms == 90100);
    CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
    owner.issued = ZCL_REVIEW_ID_MAX - 1; /* Test-only exhaustion injection. */
    CHECK(open_draft(100, &id) == ZCL_OK && id == ZCL_REVIEW_ID_MAX);
    zcl_review_snapshot snapshot;
    CHECK(zcl_review_snapshot_get(&owner, id, 100, &snapshot) == ZCL_OK);
    CHECK(zcl_review_cancel(&owner, id) == ZCL_OK);
    cleared(ZCL_REVIEW_ID_MAX);
    opening_refused(draft, draft_length, 2, 500, 100, ZCL_RESOURCE_EXHAUSTED);
    owner.issued = UINT64_MAX;
    opening_refused(draft, draft_length, 2, 500, 100, ZCL_RESOURCE_EXHAUSTED);
}

static void arguments(void)
{
    reset_fixture();
    uint64_t id = 99;
    CHECK(zcl_review_open(NULL, draft, draft_length, ZCL_MAINNET, fixture.sources, 2, 500, 0, &id)
        == ZCL_INVALID_ARGUMENT && id == 99);
    CHECK(open_draft(0, NULL) == ZCL_INVALID_ARGUMENT);
    cleared(0);
    CHECK(open_draft(100, &id) == ZCL_OK);
    zcl_review_owner original;
    memcpy(&original, &owner, sizeof(owner));
    zcl_review_snapshot snapshot;
    size_t length = 0;
    CHECK(zcl_review_snapshot_get(NULL, id, UINT64_MAX, &snapshot) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_snapshot_get(&owner, id, UINT64_MAX, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_copy_wire(NULL, id, UINT64_MAX, draft, sizeof(draft), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_copy_wire(&owner, id, UINT64_MAX, NULL, sizeof(draft), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_copy_wire(&owner, id, UINT64_MAX, draft, sizeof(draft), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_cancel(NULL, id) == ZCL_INVALID_ARGUMENT);
    zcl_review_clear(NULL);
    CHECK(memcmp(&owner, &original, sizeof(owner)) == 0);
    zcl_review_clear(&owner);
    cleared(1);
}

int main(void)
{
    immutable_draft(); opening_atomicity(); stale_callbacks(); deadlines();
    capacity_and_issuance(); arguments();
    puts("Immutable unsigned review lifetime checks passed");
    return 0;
}
