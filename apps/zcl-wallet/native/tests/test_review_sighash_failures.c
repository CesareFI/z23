/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "transaction_review_internal.h"
#include "transaction_sighash.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only the separately compiled review-hash translation unit uses these
 * substitutions; real review opening and its providers remain unchanged. */
#undef zcl_transaction_parse
#undef zcl_address_script
zcl_status zcl_transaction_parse(const uint8_t *wire, size_t length, zcl_transparent_tx *transaction);
zcl_status zcl_address_script(const zcl_address *address, uint8_t *script, size_t capacity, size_t *length);

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review hash fault check failed at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_review_owner owner, saved;
static zcl_transparent_tx expected;
static uint8_t wire[ZCL_TX_WIRE_MAX];
static size_t wire_length, selected;
static unsigned failure, calls, wipes;
static struct { const uint8_t *bytes; size_t length; bool cleared; } spans[4];
static size_t span_count;

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void observe(const void *bytes, size_t length)
{
    CHECK(bytes != NULL && length <= 4096 && span_count < 4);
    spans[span_count].bytes = bytes;
    spans[span_count].length = length;
    spans[span_count].cleared = false;
    ++span_count;
}

zcl_status zcl_review_hash_test_parse(const uint8_t *input, size_t length, zcl_transparent_tx *transaction)
{
    CHECK(calls++ == 0 && wipes == 0);
    CHECK(length == wire_length && memcmp(input, wire, length) == 0);
    filled((const uint8_t *)transaction, sizeof(*transaction), 0);
    observe(transaction, sizeof(*transaction));
    if (failure == 1) {
        memset(transaction, 0x6a, sizeof(*transaction));
        return ZCL_INVALID_ENCODING;
    }
    return zcl_transaction_parse(input, length, transaction);
}

zcl_status zcl_review_hash_test_script(const zcl_address *address, uint8_t *script,
                                      size_t capacity, size_t *length)
{
    CHECK(calls++ == 1 && wipes == 0 && capacity == 25 && length != NULL);
    CHECK(address->kind == ZCL_P2PKH && address->network == ZCL_MAINNET);
    CHECK(memcmp(address->hash, saved.data.assessment.inputs[selected].destination.hash, 20) == 0);
    filled(script, capacity, 0);
    CHECK(*length == 0);
    observe(script, capacity);
    observe(length, sizeof(*length));
    if (failure == 2) {
        memset(script, 0x6a, capacity);
        *length = SIZE_MAX;
        return ZCL_UNSUPPORTED;
    }
    return zcl_address_script(address, script, capacity, length);
}

zcl_status zcl_transaction_sighash_all(const zcl_transparent_tx *transaction,
    size_t index, const uint8_t *script, size_t length, uint64_t amount, uint32_t branch,
    uint8_t *digest, size_t capacity)
{
    CHECK(calls++ == 2 && wipes == 0 && index == selected && capacity == 32);
    CHECK(memcmp(transaction, &expected, sizeof(expected)) == 0);
    const zcl_tx_output *previous = &fixture.previous[selected].outputs[0];
    CHECK(length == previous->script_len && memcmp(script, previous->script, length) == 0);
    CHECK(amount == previous->value && branch == UINT32_C(0x01020304));
    filled(digest, capacity, 0);
    observe(digest, capacity);
    memset(digest, 0x6a, capacity);
    return failure == 3 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

void zcl_secure_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && length <= 4096 && wipes++ == 0);
    CHECK(span_count <= sizeof(spans) / sizeof(spans[0]));
    memset(buffer, 0, length);
    /* Observe and retire captured spans while their caller object is live. */
    for (size_t i = 0; i < span_count; ++i) {
        filled(spans[i].bytes, spans[i].length, 0);
        spans[i].bytes = NULL;
        spans[i].cleared = true;
    }
}

static void run(unsigned ordinal, size_t index)
{
    CHECK(ordinal <= 3 && index < 2);
    struct { uint8_t before[8], digest[64], after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    memset(spans, 0, sizeof(spans));
    calls = 0; wipes = 0; span_count = 0; failure = ordinal; selected = index;
    static const zcl_status statuses[] = {ZCL_OK, ZCL_INVALID_ENCODING, ZCL_UNSUPPORTED, ZCL_CRYPTO_FAILURE};
    CHECK(zcl_review_sighash_p2pkh(&owner, saved.data.id, 100, index, UINT32_C(0x01020304),
        box.digest, sizeof(box.digest)) == statuses[ordinal]);
    CHECK(calls == (ordinal == 0 ? 3 : ordinal) && wipes == 1);
    for (size_t i = 0; i < span_count; ++i) CHECK(spans[i].cleared && spans[i].bytes == NULL);
    filled(box.before, sizeof(box.before), 0xa5);
    filled(box.after, sizeof(box.after), 0xa5);
    filled(box.digest, 32, ordinal == 0 ? 0x6a : 0xa5);
    filled(box.digest + 32, 32, 0xa5);
    CHECK(memcmp(&owner, &saved, sizeof(owner)) == 0);
}

int main(void)
{
    CHECK(assessment_fixture_init(&fixture));
    fixture.spending.inputs[1].previous_index = 0; /* Two distinct P2PKH funding rows. */
    fixture.spending.outputs[0].value = 28000;
    CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &wire_length) == ZCL_OK);
    CHECK(zcl_transaction_parse(wire, wire_length, &expected) == ZCL_OK);
    uint64_t id = 0;
    CHECK(zcl_review_open(&owner, wire, wire_length, ZCL_MAINNET, fixture.sources, 2, 500, 100, &id) == ZCL_OK);
    memcpy(&saved, &owner, sizeof(saved));
    for (size_t index = 0; index < 2; ++index)
        for (unsigned ordinal = 0; ordinal <= 3; ++ordinal) run(ordinal, index);
    calls = 0; wipes = 0;
    uint8_t digest[32];
    memset(digest, 0xa5, sizeof(digest));
    CHECK(zcl_review_sighash_p2pkh(&owner, id, 100, 0, 0, digest, 31) == ZCL_BUFFER_TOO_SMALL);
    CHECK(zcl_review_sighash_p2pkh(&owner, id, 100, SIZE_MAX, 0, digest, 32) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_review_sighash_p2pkh(&owner, id + 1, UINT64_MAX, 0, 0, digest, 32) == ZCL_CANCELLED);
    CHECK(calls == 0 && wipes == 0 && memcmp(&owner, &saved, sizeof(owner)) == 0);
    filled(digest, sizeof(digest), 0xa5);
    zcl_review_clear(&owner); /* Real owner zeroizer is not remapped. */
    CHECK(puts("Review hash faults stop processing, preserve output and clear live private spans") >= 0);
    return 0;
}
