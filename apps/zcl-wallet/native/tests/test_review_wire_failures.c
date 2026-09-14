/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_fixture.h"
#include "transaction_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef zcl_transaction_check
zcl_status zcl_transaction_check(const zcl_transparent_tx *transaction, size_t *length);
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review wire fault at %d\n", __LINE__); abort(); } } while (0)
static signed_review_fixture fixture;
static zcl_signature expected_signatures[8];
static zcl_review_block expected_block;
static zcl_review_owner replacement;
static zcl_review_snapshot snapshot;
static unsigned failure, parse_calls, hash_calls, script_calls, check_calls, serialize_calls;
static unsigned row_wipes, wire_wipes, owner_wipes;
static size_t expected_length;
static zcl_transparent_tx *owned_tx;
static const zcl_signature *owned_signatures[8];
static const zcl_review_block *owned_block;
static const uint8_t *live_digest, *live_wire;
static const size_t *live_expected, *live_length;

static void filled(const void *pointer, size_t length, uint8_t value)
{
    CHECK(pointer != NULL);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

zcl_status zcl_transaction_parse(const uint8_t *wire, size_t length, zcl_transparent_tx *output)
{
    CHECK(parse_calls++ == 0 && wire == fixture.owner.data.wire && length == fixture.owner.data.wire_length);
    CHECK(output != NULL && output != &fixture.spending);
    *output = fixture.spending; owned_tx = output;
    memset(fixture.signatures, 0xff, sizeof(fixture.signatures));
    fixture.block.network = (zcl_network)255; fixture.block.height = UINT32_MAX;
    fixture.block.lock_time_cutoff = UINT64_MAX;
    if (failure == 2) output->input_count = SIZE_MAX;
    if (failure == 3) output->inputs[7].script_len = 1;
    return failure == 1 ? ZCL_INVALID_ENCODING : ZCL_OK;
}

zcl_status zcl_review_sighash_context(zcl_review_owner *owner, uint64_t id, uint64_t now,
    size_t index, const zcl_review_block *block, uint8_t *output, size_t capacity)
{
    CHECK(owner == &fixture.owner && id == fixture.id && now == 100 && index < 8);
    CHECK(index == hash_calls && script_calls == hash_calls && row_wipes == hash_calls);
    CHECK(capacity == 32 && live_digest == NULL);
    CHECK(block != &fixture.block && memcmp(block, &expected_block, sizeof(*block)) == 0);
    owned_block = block; live_digest = output;
    filled(output, 32, 0); memcpy(output, fixture.digests[index], 32); ++hash_calls;
    return failure == 4 && index == 7 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

static zcl_status script_completion(size_t index, size_t *length)
{
    if (failure == 6 && index == 7) *length = SIZE_MAX;
    if (failure == 7 && index == 7) *length = 43;
    if (failure == 14 && index == 7) CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    return failure == 5 && index == 7 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

zcl_status zcl_signature_p2pkh(const zcl_signature *signature, const uint8_t *digest, size_t digest_len,
    const uint8_t *hash, size_t hash_len, uint8_t *output, size_t capacity, size_t *length)
{
    const size_t index = script_calls;
    CHECK(index < 8 && hash_calls == script_calls + 1 && digest == live_digest && digest_len == 32);
    CHECK(signature != &fixture.signatures[index] && memcmp(signature, &expected_signatures[index], sizeof(*signature)) == 0);
    CHECK(memcmp(digest, fixture.digests[index], 32) == 0 && hash_len == 20);
    CHECK(memcmp(hash, fixture.hashes[index], 20) == 0 && output != NULL && length != NULL && capacity == 128);
    owned_signatures[index] = signature;
    filled(output, 128, 0); memset(output, 0x27, 106); *length = 106; ++script_calls;
    return script_completion(index, length);
}

zcl_status zcl_wire_test_check(const zcl_transparent_tx *transaction, size_t *length)
{
    CHECK(check_calls++ == 0 && transaction == owned_tx && length != NULL);
    CHECK(script_calls == 8 && transaction->input_count == 8); live_expected = length;
    const zcl_status status = zcl_transaction_check(transaction, length);
    CHECK(status == ZCL_OK && *length <= ZCL_TX_WIRE_MAX);
    expected_length = *length;
    if (failure == 9) *length = SIZE_MAX;
    if (failure == 10) *length = 0;
    return failure == 8 ? ZCL_INVALID_ENCODING : ZCL_OK;
}

static void replacement_open(void)
{
    CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    zcl_previous_transaction previous[8] = {{0}};
    for (size_t i = 0; i < 8; ++i) { previous[i].wire = fixture.funding_wire; previous[i].length = fixture.funding_length; }
    uint64_t id = 0;
    CHECK(zcl_review_open(&fixture.owner, fixture.unsigned_wire, fixture.unsigned_length, ZCL_MAINNET,
        previous, 8, 500, 100, &id) == ZCL_OK);
    CHECK(id == fixture.id + 1); memcpy(&replacement, &fixture.owner, sizeof(replacement));
}

zcl_status zcl_transaction_serialize(const zcl_transparent_tx *transaction, uint8_t *output,
    size_t capacity, size_t *length)
{
    CHECK(serialize_calls++ == 0 && transaction == owned_tx && check_calls == 1);
    CHECK(output != NULL && length != NULL && capacity == ZCL_TX_WIRE_MAX);
    filled(output, capacity, 0); live_wire = output; live_length = length;
    memset(output, 0x42, expected_length); *length = expected_length;
    if (failure == 12) *length = SIZE_MAX;
    if (failure == 13) --*length;
    if (failure == 15) CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    if (failure == 16) replacement_open();
    if (failure == 17) CHECK(zcl_review_snapshot_get(&fixture.owner, fixture.id, 101, &snapshot) == ZCL_OK);
    if (failure == 18) CHECK(zcl_review_snapshot_get(&fixture.owner, fixture.id, 90100, &snapshot) == ZCL_TIMED_OUT);
    return failure == 11 ? ZCL_IO_FAILURE : ZCL_OK;
}

static void clear_wire(void *buffer, size_t length)
{
    CHECK(length >= ZCL_TX_WIRE_MAX + 2 * sizeof(size_t) && length <= 2000 && wire_wipes++ == 0);
    memset(buffer, 0, length);
    if (live_wire != NULL) filled(live_wire, ZCL_TX_WIRE_MAX, 0);
    CHECK(live_expected != NULL && *live_expected == 0);
    if (live_length != NULL) CHECK(*live_length == 0);
    live_wire = NULL; live_expected = live_length = NULL;
}

void zcl_wire_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    if (length == 32) {
        CHECK(buffer == live_digest); memset(buffer, 0, length);
        live_digest = NULL; ++row_wipes; return;
    }
    if (length <= 2000) { clear_wire(buffer, length); return; }
    CHECK(length >= sizeof(zcl_transparent_tx) + 8 * sizeof(zcl_signature) + sizeof(zcl_review_block)
        && length <= 4096 && owner_wipes++ == 0 && row_wipes == hash_calls);
    CHECK(live_digest == NULL);
    memset(buffer, 0, length);
    CHECK(owned_tx != NULL); filled(owned_tx, sizeof(*owned_tx), 0); owned_tx = NULL;
    for (size_t i = 0; i < 8; ++i) {
        if (owned_signatures[i] != NULL) filled(owned_signatures[i], sizeof(*owned_signatures[i]), 0);
        owned_signatures[i] = NULL;
    }
    if (owned_block != NULL) filled(owned_block, sizeof(*owned_block), 0);
    owned_block = NULL;
}

static void reset(unsigned mode)
{
    CHECK(mode < 20 && owned_tx == NULL && live_digest == NULL && live_wire == NULL);
    CHECK(signed_review_fixture_init(&fixture, ZCL_MAINNET, 8, 16));
    memcpy(expected_signatures, fixture.signatures, sizeof(expected_signatures)); expected_block = fixture.block;
    failure = mode; parse_calls = hash_calls = script_calls = check_calls = serialize_calls = 0;
    row_wipes = wire_wipes = owner_wipes = 0; expected_length = 0;
    memset(owned_signatures, 0, sizeof(owned_signatures));
}

static zcl_status expected(unsigned mode)
{
    if (mode == 0) return ZCL_OK;
    if (mode == 3) return ZCL_UNSUPPORTED;
    if (mode == 4 || mode == 5) return ZCL_CRYPTO_FAILURE;
    if (mode == 11) return ZCL_IO_FAILURE;
    if (mode >= 14 && mode <= 18) return ZCL_CANCELLED;
    return mode == 19 ? ZCL_BUFFER_TOO_SMALL : ZCL_INVALID_ENCODING;
}

static void provider_stages(unsigned mode)
{
    static const struct { unsigned hash, script, check, serialize; } expected[] = {
        {8,8,1,1}, {0,0,0,0}, {0,0,0,0}, {7,7,0,0}, {8,7,0,0},
        {8,8,0,0}, {8,8,0,0}, {8,8,0,0}, {8,8,1,0}, {8,8,1,0},
        {8,8,1,0}, {8,8,1,1}, {8,8,1,1}, {8,8,1,1}, {8,8,1,0},
        {8,8,1,1}, {8,8,1,1}, {8,8,1,1}, {8,8,1,1}, {8,8,1,0}
    };
    CHECK(mode < sizeof(expected) / sizeof(expected[0]));
    CHECK(hash_calls == expected[mode].hash && row_wipes == hash_calls);
    CHECK(script_calls == expected[mode].script && check_calls == expected[mode].check);
    CHECK(wire_wipes == check_calls && serialize_calls == expected[mode].serialize);
}

static void stages(unsigned mode)
{
    provider_stages(mode); /* Check wipe counts before post-return NULL markers. */
    CHECK(parse_calls == 1 && owner_wipes == 1 && owned_tx == NULL && owned_block == NULL);
    CHECK(live_digest == NULL && live_wire == NULL && live_expected == NULL && live_length == NULL);
}

static void run(unsigned mode)
{
    reset(mode);
    struct { uint8_t before[8], wire[ZCL_TX_WIRE_MAX], after[8]; } box;
    memset(&box, 0xa5, sizeof(box)); size_t length = SIZE_MAX;
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 100, &fixture.block, fixture.signatures,
        8, box.wire, mode == 19 ? 0 : sizeof(box.wire), &length) == expected(mode));
    stages(mode); filled(box.before, 8, 0xa5); filled(box.after, 8, 0xa5);
    if (mode == 0) { CHECK(length == expected_length); filled(box.wire, length, 0x42); }
    else CHECK(length == SIZE_MAX);
    const size_t start = mode == 0 ? length : 0;
    filled(box.wire + start, sizeof(box.wire) - start, 0xa5);
    if (mode == 16) CHECK(memcmp(&replacement, &fixture.owner, sizeof(replacement)) == 0);
    else if (mode >= 14 && mode <= 18) CHECK(fixture.owner.data.id == 0 && fixture.owner.issued == 1);
    else CHECK(fixture.owner.data.id == fixture.id && fixture.owner.data.last_ms == 100);
}

int main(void)
{
    for (unsigned mode = 0; mode < 20; ++mode) run(mode);
    CHECK(puts("Signed-wire faults preserve publication, isolate copied sources, and reject cancellation or replacement before delivery") >= 0);
    return 0;
}
