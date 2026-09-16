/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_transaction_parse
#undef zcl_transaction_assess
#undef zcl_transaction_serialize
#undef zcl_secure_zero
#include "assessment_fixture.h"
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review retirement at %d\n", __LINE__); exit(1); } } while (0)
static assessment_fixture fixture;
static zcl_review_owner owner, saved;
static uint8_t wire[ZCL_TX_WIRE_MAX], original[ZCL_TX_WIRE_MAX];
static size_t wire_length;
static uintptr_t parsed_identity, candidate_identity;
static unsigned fault, calls, parsed_wipes, candidate_wipes, snapshot_wipes, owner_wipes;

zcl_status zcl_review_test_parse(const uint8_t *bytes, size_t length, zcl_transparent_tx *transaction);
zcl_status zcl_review_test_assess(const zcl_transparent_tx *transaction, zcl_network network,
    const zcl_previous_transaction *previous, size_t count, uint64_t fee, zcl_transaction_assessment *assessment);
zcl_status zcl_review_test_serialize(const zcl_transparent_tx *transaction, uint8_t *bytes,
    size_t capacity, size_t *length);
void zcl_review_test_zero(void *buffer, size_t length);

zcl_status zcl_review_test_parse(const uint8_t *bytes, size_t length, zcl_transparent_tx *transaction)
{
    CHECK(calls == 0 && parsed_identity == 0);
    calls = 1;
    parsed_identity = (uintptr_t)transaction;
    if (fault == 1) {
        memset(transaction, 0x5a, sizeof(*transaction));
        return ZCL_INVALID_ENCODING;
    }
    const zcl_status status = zcl_transaction_parse(bytes, length, transaction);
    /* The admitted parsed value must be the only later source of draft data. */
    memset(wire, 0xcc, sizeof(wire));
    return status;
}

zcl_status zcl_review_test_assess(const zcl_transparent_tx *transaction, zcl_network network,
    const zcl_previous_transaction *previous, size_t count, uint64_t fee, zcl_transaction_assessment *assessment)
{
    CHECK(calls == 1 && (uintptr_t)transaction == parsed_identity && parsed_wipes == 0);
    calls |= 2;
    candidate_identity = (uintptr_t)assessment - offsetof(zcl_review_data, assessment);
    if (fault == 2) {
        memset(assessment, 0x6a, sizeof(*assessment));
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_transaction_assess(transaction, network, previous, count, fee, assessment);
}

zcl_status zcl_review_test_serialize(const zcl_transparent_tx *transaction, uint8_t *bytes,
    size_t capacity, size_t *length)
{
    CHECK(calls == 3 && (uintptr_t)transaction == parsed_identity && parsed_wipes == 0);
    CHECK((uintptr_t)bytes == candidate_identity + offsetof(zcl_review_data, wire));
    CHECK(capacity == ZCL_TX_WIRE_MAX);
    calls |= 4;
    if (fault == 3) {
        memset(bytes, 0x7a, capacity);
        *length = SIZE_MAX;
        return ZCL_BUFFER_TOO_SMALL;
    }
    return zcl_transaction_serialize(transaction, bytes, capacity, length);
}

static void zero_bytes(const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}

void zcl_review_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    zero_bytes(buffer, length);
    if ((uintptr_t)buffer == parsed_identity) {
        CHECK(length == sizeof(zcl_transparent_tx) && parsed_wipes++ == 0);
        parsed_identity = 0;
    } else if (buffer == &owner.data) {
        CHECK(length == sizeof(owner.data));
        ++owner_wipes;
    } else if (length == sizeof(zcl_review_data)) {
        CHECK(parsed_identity == 0 && parsed_wipes == 1 && candidate_wipes++ == 0);
        CHECK(candidate_identity == 0 || candidate_identity == (uintptr_t)buffer);
        candidate_identity = 0;
    } else {
        CHECK(length == sizeof(zcl_review_snapshot));
        ++snapshot_wipes;
    }
}

static void reset(unsigned selected)
{
    CHECK(parsed_identity == 0 && candidate_identity == 0);
    CHECK(assessment_fixture_init(&fixture));
    if (selected == 4) {
        fixture.spending.inputs[0].script[0] = 0x51;
        fixture.spending.inputs[0].script_len = 1;
    }
    CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &wire_length) == ZCL_OK);
    memcpy(original, wire, sizeof(original));
    memset(&owner, 0, sizeof(owner));
    owner.issued = 41;
    memcpy(&saved, &owner, sizeof(saved));
    fault = selected;
    calls = 0; parsed_wipes = 0; candidate_wipes = 0; snapshot_wipes = 0; owner_wipes = 0;
}

static zcl_status open_review(uint64_t now, uint64_t *id)
{
    return zcl_review_open(&owner, wire, wire_length, ZCL_MAINNET, fixture.sources, 2, 500, now, id);
}

static void preparation(void)
{
    const zcl_status statuses[] = {ZCL_OK, ZCL_INVALID_ENCODING, ZCL_CRYPTO_FAILURE,
        ZCL_BUFFER_TOO_SMALL, ZCL_UNSUPPORTED};
    const unsigned expected_calls[] = {7, 1, 3, 7, 1};
    for (unsigned selected = 0; selected < sizeof(statuses) / sizeof(statuses[0]); ++selected) {
        reset(selected);
        uint64_t id = UINT64_MAX;
        CHECK(open_review(100, &id) == statuses[selected]);
        CHECK(calls == expected_calls[selected]);
        CHECK(parsed_wipes == 1 && candidate_wipes == 1 && owner_wipes == 0);
        CHECK(parsed_identity == 0 && candidate_identity == 0);
        if (selected == 0) {
            CHECK(id == 42 && owner.issued == 42 && owner.data.id == 42);
            CHECK(owner.data.wire_length == wire_length);
            CHECK(memcmp(owner.data.wire, original, wire_length) == 0);
            CHECK(owner.data.assessment.fee == 500 && owner.data.assessment.input_count == 2);
            CHECK(owner.data.context.lock_time == fixture.spending.lock_time);
        } else {
            CHECK(id == UINT64_MAX && memcmp(&owner, &saved, sizeof(owner)) == 0);
        }
    }
}

static void admission(void)
{
    reset(0);
    uint64_t id = UINT64_MAX;
    CHECK(zcl_review_open(NULL, wire, wire_length, ZCL_MAINNET, fixture.sources, 2, 500, 100, &id)
        == ZCL_INVALID_ARGUMENT);
    CHECK(open_review(100, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_prepare(wire, wire_length, ZCL_MAINNET, fixture.sources, 2, 500, NULL)
        == ZCL_INVALID_ARGUMENT);
    CHECK(open_review(UINT64_MAX, &id) == ZCL_OUT_OF_RANGE);
    owner.issued = ZCL_REVIEW_ID_MAX;
    CHECK(open_review(100, &id) == ZCL_RESOURCE_EXHAUSTED);
    owner.data.id = 7;
    CHECK(open_review(100, &id) == ZCL_BUSY);
    CHECK(id == UINT64_MAX && calls == 0 && parsed_wipes == 0 && candidate_wipes == 0);
}

static void snapshots(void)
{
    reset(0);
    uint64_t id = 0;
    CHECK(open_review(100, &id) == ZCL_OK);
    zcl_review_snapshot snapshot, before;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    memcpy(&before, &snapshot, sizeof(before));
    CHECK(zcl_review_snapshot_get(&owner, id + 1, 101, &snapshot) == ZCL_CANCELLED);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0 && snapshot_wipes == 0);
    CHECK(zcl_review_snapshot_get(&owner, id, 101, &snapshot) == ZCL_OK);
    CHECK(snapshot_wipes == 1 && snapshot.assessment.fee == 500);
    CHECK(snapshot.remaining_ms == ZCL_REVIEW_LIFETIME_MS - 1);
    CHECK(memcmp(&snapshot.context, &owner.data.context, sizeof(snapshot.context)) == 0);
    memcpy(&before, &snapshot, sizeof(before));
    CHECK(zcl_review_snapshot_get(&owner, id, 100 + ZCL_REVIEW_LIFETIME_MS, &snapshot) == ZCL_TIMED_OUT);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0 && snapshot_wipes == 1 && owner_wipes == 1);
    zero_bytes(&owner.data, sizeof(owner.data));
    CHECK(owner.issued == id);
}

int main(void)
{
    preparation();
    admission();
    snapshots();
    puts("Review preparation and publication retirement passed");
    return 0;
}
