/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#include "review_wallet_fixture.h"
#include "change_custody_internal.h"
#include <stdlib.h>
#include <string.h>

/* Only this test and the wrapper under test have substituted providers. */
#undef zcl_change_custody_prepare
#undef zcl_address_encode
zcl_status zcl_change_custody_prepare(const uint8_t *, size_t, const uint8_t *, size_t, zcl_change_custody *);
zcl_status zcl_address_encode(const zcl_address *, uint8_t *, size_t, size_t *);
#undef CHECK
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review wallet fault at %d\n", __LINE__); abort(); } } while (0)

static review_wallet_fixture fixture;
static zcl_review_owner saved;
static zcl_review_wallet_input supplied;
static uint8_t directory[1024], record[140], entropy[32], expected[35];
static struct { const uint8_t *bytes; size_t length; } spans[5];
static size_t span_count;
static const uint8_t *blinding;
static uintptr_t record_copy;
static unsigned failure, calls, work_wipes, random_wipes, record_wipes, admission_wipes;
static uint32_t chain;
static bool output_check;
static unsigned samples;

static zcl_status sample(void *context, uint64_t *now)
{
    CHECK(context == NULL && now != NULL && samples < 2);
    CHECK(work_wipes == samples); /* All owned secrets retire before completion. */
    ++samples; *now = 100;
    return ZCL_OK;
}

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void observe(const void *bytes, size_t length)
{
    CHECK(bytes != NULL && length <= 4096 && span_count < 5);
    spans[span_count].bytes = bytes;
    spans[span_count].length = length;
    ++span_count;
}

zcl_status zcl_review_wallet_test_prepare(const uint8_t *input, size_t input_len,
    const uint8_t *secret, size_t secret_len, zcl_change_custody *wallet)
{
    CHECK(calls == 0 && work_wipes == 0);
    calls |= 1;
    CHECK(input == record && input_len == fixture.wallet.wallet_len);
    CHECK(secret != entropy && secret_len == fixture.entropy_len);
    filled(secret, secret_len, 0x37);
    filled(secret + secret_len, 32 - secret_len, 0);
    filled((const uint8_t *)wallet, sizeof(*wallet), 0);
    observe(secret, 32);
    observe(wallet, sizeof(*wallet));
    CHECK(zcl_change_custody_prepare(input, input_len, secret, secret_len, wallet) == ZCL_OK);
    if (failure == 1) { memset(wallet->record, 0x6a, 140); return ZCL_INVALID_ENCODING; }
    return ZCL_OK;
}

zcl_status zcl_review_wallet_test_encode(const zcl_address *address, uint8_t *output,
    size_t capacity, size_t *length)
{
    CHECK(calls == 1 && capacity == 35 && length != NULL && *length == 0);
    calls |= 2;
    CHECK(memcmp(address, &saved.data.assessment.inputs[chain].destination, sizeof(*address)) == 0);
    filled(output, capacity, 0);
    observe(output, capacity);
    memcpy(output, expected, capacity);
    *length = failure == 3 ? 34 : 35;
    /* Mutate all caller spans/scalars only after the wrapper owns its copies. */
    memset(directory, 0xa5, sizeof(directory));
    memset(record, 0xa5, sizeof(record));
    memset(entropy, 0xa5, sizeof(entropy));
    memset(&supplied, 0, sizeof(supplied));
    return failure == 2 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

static void check_directory(const uint8_t *path, size_t length)
{
    CHECK(path != directory && length == fixture_path_len());
    CHECK(memcmp(path, fixture.store.path, length) == 0);
    filled(path + length, 1024 - length, 0);
    observe(path, 1024);
}

zcl_status zcl_review_wallet_test_read(const uint8_t *path, size_t path_len,
    uint8_t *output, size_t capacity, size_t *length, bool *pending)
{
    CHECK(calls == 3 && chain == 0 && capacity == 140 && length != NULL && pending != NULL);
    calls |= 4;
    CHECK(*length == 0 && !*pending);
    filled(output, capacity, 0);
    CHECK(record_copy == 0 && record_wipes == 0);
    record_copy = (uintptr_t)output;
    check_directory(path, path_len);
    memcpy(output, fixture.wallet.wallet, fixture.wallet.wallet_len);
    *length = failure == 6 ? SIZE_MAX : fixture.wallet.wallet_len;
    *pending = failure == 5;
    if (failure == 7) output[fixture.wallet.wallet_len - 1] ^= 1;
    return failure == 4 ? ZCL_IO_FAILURE : ZCL_OK;
}

zcl_status zcl_review_wallet_test_random(uint8_t *output, size_t length)
{
    CHECK(calls == 7 && length == 32 && blinding == NULL);
    CHECK(record_copy == 0 && record_wipes == 1);
    calls |= 8;
    filled(output, length, 0);
    blinding = output;
    memset(output, 0x6a, length);
    return failure == 8 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

static void check_copies(const uint8_t *header, size_t header_len, const uint8_t *secret, size_t secret_len)
{
    CHECK(header != record && header_len <= fixture.wallet.wallet_len);
    CHECK(memcmp(header, fixture.wallet.wallet, header_len) == 0);
    CHECK(secret != entropy && secret_len == fixture.entropy_len);
    filled(secret, secret_len, 0x37);
}

static zcl_status derived_address(uint8_t *output, size_t capacity)
{
    CHECK(capacity == 35);
    filled(output, capacity, 0);
    observe(output, capacity);
    memcpy(output, expected, capacity);
    if (failure == 10) output[34] ^= 1;
    return failure == 9 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

zcl_status zcl_review_wallet_test_receive(const uint8_t *header, size_t header_len,
    const uint8_t *secret, size_t secret_len, const uint8_t *blind, size_t blind_len,
    uint8_t *output, size_t capacity)
{
    CHECK(calls == 15 && chain == 0 && header_len == 80);
    calls |= 16;
    check_copies(header, header_len, secret, secret_len);
    CHECK(blind == blinding && blind_len == 32);
    filled(blind, blind_len, 0x6a);
    return derived_address(output, capacity);
}

zcl_status zcl_review_wallet_test_change(const uint8_t *path, size_t path_len,
    const uint8_t *input, size_t input_len, const uint8_t *secret, size_t secret_len,
    uint32_t index, uint8_t *output, size_t capacity)
{
    CHECK(calls == 3 && chain == 1 && index == 0 && input_len == fixture.wallet.wallet_len);
    calls |= 32;
    check_directory(path, path_len);
    check_copies(input, input_len, secret, secret_len);
    return derived_address(output, capacity);
}

void zcl_review_wallet_test_zero(void *buffer, size_t length);
void zcl_review_wallet_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && length <= 4096);
    zcl_secure_zero(buffer, length);
    if ((uintptr_t)buffer == record_copy) {
        CHECK(length == 140 && record_wipes++ == 0);
        filled(buffer, length, 0);
        record_copy = 0;
        return;
    }
    if (buffer == blinding) {
        CHECK(length == 32 && random_wipes++ == 0);
        filled(blinding, 32, 0);
        blinding = NULL;
        return;
    }
    if (length == sizeof(zcl_wallet_record)) {
        CHECK(admission_wipes++ == 0);
        filled(buffer, length, 0);
        return;
    }
    CHECK(record_copy == 0 && work_wipes++ == 0 && length >= 32 + 1024 + 140 + 70);
    CHECK(span_count <= sizeof(spans) / sizeof(spans[0]));
    /* Inspect/retire while the whole work and its subobjects are still live. */
    for (size_t i = 0; i < span_count; ++i) {
        filled(spans[i].bytes, spans[i].length, 0);
        spans[i].bytes = NULL;
    }
}

static void reset(unsigned ordinal, uint32_t selected)
{
    failure = ordinal; chain = selected;
    calls = 0; work_wipes = 0; random_wipes = 0; record_wipes = 0; admission_wipes = 0; span_count = 0;
    samples = 0;
    CHECK(blinding == NULL && record_copy == 0);
    memset(spans, 0, sizeof(spans));
    memset(directory, 0, sizeof(directory));
    memcpy(directory, fixture.store.path, fixture_path_len());
    memcpy(record, fixture.wallet.wallet, sizeof(record));
    memset(entropy, 0x37, sizeof(entropy));
    supplied = review_wallet_fixture_claim(&fixture, chain, 0);
    supplied.directory = directory; supplied.record = record; supplied.entropy = entropy;
    size_t length = 0;
    CHECK(zcl_address_encode(&saved.data.assessment.inputs[chain].destination, expected, 35, &length) == ZCL_OK);
    CHECK(length == 35);
}

static zcl_status checked_operation(void)
{
    const zcl_review_clock clock = {sample, NULL};
    const zcl_status status = output_check
        ? zcl_review_output_change_check(&fixture.review, fixture.id, &clock, 1, &supplied)
        : zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, chain, &supplied);
    CHECK(samples == (output_check ? status == ZCL_OK ? 2u : 1u : 0u));
    return status;
}

static void run(unsigned ordinal, uint32_t selected)
{
    static const zcl_status statuses[] = {ZCL_OK, ZCL_INVALID_ENCODING, ZCL_CRYPTO_FAILURE,
        ZCL_INVALID_ENCODING, ZCL_IO_FAILURE, ZCL_NOT_FOUND, ZCL_ALREADY_EXISTS, ZCL_ALREADY_EXISTS,
        ZCL_CRYPTO_FAILURE, ZCL_CRYPTO_FAILURE, ZCL_NOT_FOUND};
    static const unsigned receive_calls[] = {31, 1, 3, 3, 7, 7, 7, 7, 15, 31, 31};
    CHECK(ordinal < sizeof(statuses) / sizeof(statuses[0]) && selected < 2);
    reset(ordinal, selected);
    CHECK(checked_operation() == statuses[ordinal]);
    const unsigned expected_calls = chain == 0 ? receive_calls[ordinal] : ordinal == 1 ? 1 : ordinal <= 3 && ordinal != 0 ? 3 : 35;
    CHECK(calls == expected_calls && work_wipes == 1 && admission_wipes == 1 && blinding == NULL);
    CHECK(random_wipes == (chain == 0 && (calls & 8) != 0 ? 1U : 0U));
    CHECK(record_copy == 0 && record_wipes == (chain == 0 && (calls & 4) != 0 ? 1U : 0U));
    CHECK(span_count <= sizeof(spans) / sizeof(spans[0]));
    for (size_t i = 0; i < span_count; ++i) CHECK(spans[i].bytes == NULL);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);
}

static void early_refusals(void)
{
    reset(0, 0);
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, SIZE_MAX, &supplied) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id + 1, UINT64_MAX, 0, &supplied) == ZCL_CANCELLED);
    CHECK(calls == 0 && work_wipes == 0);
    supplied.entropy_len = SIZE_MAX;
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, 0, &supplied) == ZCL_OUT_OF_RANGE);
    CHECK(calls == 0 && work_wipes == 1 && admission_wipes == 0);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);

    reset(0, 0);
    record[0] ^= 0xff;
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, 0, &supplied)
        == ZCL_UNSUPPORTED);
    CHECK(calls == 0 && work_wipes == 1 && admission_wipes == 1);

    reset(0, 0);
    supplied.entropy_len = supplied.entropy_len == 16 ? 20 : 16;
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, 0, &supplied)
        == ZCL_OUT_OF_RANGE);
    CHECK(calls == 0 && work_wipes == 1 && admission_wipes == 1);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);
}

int main(void)
{
    for (size_t length = 16; length <= 32; length += 4) {
        CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, length, false) == 0);
        zcl_tx_output *change = &fixture.funding.spending.outputs[1];
        CHECK(zcl_address_script(&fixture.review.data.assessment.inputs[1].destination,
            change->script, sizeof(change->script), &change->script_len) == ZCL_OK);
        zcl_review_clear(&fixture.review);
        CHECK(review_wallet_fixture_review(&fixture, ZCL_MAINNET, 100) == 0);
        memcpy(&saved, &fixture.review, sizeof(saved));
        for (unsigned ordinal = 0; ordinal <= 10; ++ordinal) run(ordinal, 0);
        const unsigned change_cases[] = {0, 1, 2, 3, 9, 10};
        for (size_t i = 0; i < sizeof(change_cases) / sizeof(change_cases[0]); ++i) run(change_cases[i], 1);
        output_check = true;
        for (size_t i = 0; i < sizeof(change_cases) / sizeof(change_cases[0]); ++i) run(change_cases[i], 1);
        output_check = false;
        early_refusals();
        CHECK(review_wallet_fixture_close(&fixture) == 0);
    }
    CHECK(puts("Review wallet failures stop on dirty output and clear live owned secret spans") >= 0);
    return 0;
}
