/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "review_wallet_fixture.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#undef CHECK
#define CHECK(v) do { if (!(v)) abort(); } while (0)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static review_wallet_fixture fixture;
static zcl_review_owner expected_owner, initial_owner;
static bool initialized;
static uint8_t record[140], entropy[32], journal[240];
static size_t journal_len;

static void initialize(void)
{
    if (initialized) return;
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, false) == 0);
    fixture.funding.spending.outputs[1] = fixture.funding.previous[1].outputs[1];
    fixture.funding.spending.outputs[1].value = 1500;
    CHECK(review_wallet_fixture_close(&fixture) == 0);
    initialized = true;
}

static void source_mutation(zcl_review_wallet_input *claim, const uint8_t *data, size_t size)
{
    const uint8_t mode = data[0] % 11;
    switch (mode) {
    case 1: claim->record_len = data[1] == 255 ? SIZE_MAX : data[1] % 142; break;
    case 2: claim->entropy_len = data[1] == 255 ? SIZE_MAX : data[1] % 34; break;
    case 3: claim->directory_len = data[1] % 2 == 0 ? 0 : SIZE_MAX; break;
    case 4: record[data[1] % 140] ^= (uint8_t)(data[2] | 1); break;
    case 5: entropy[data[1] % 32] ^= (uint8_t)(data[2] | 1); break;
    case 6: journal_len = data[1] % 161; break;
    case 7: journal[80 + data[1] % 80] ^= (uint8_t)(data[2] | 1); break;
    case 10:
        journal_len = size - 12;
        memcpy(journal, data + 12, journal_len);
        break;
    default: break;
    }
}

static const zcl_review_wallet_input *null_mutation(zcl_review_wallet_input *claim, const uint8_t *data)
{
    if (data[0] % 11 != 9) return claim;
    switch (data[1] % 4) {
    case 0: claim->directory = NULL; break;
    case 1: claim->record = NULL; break;
    case 2: claim->entropy = NULL; break;
    default: return NULL;
    }
    return claim;
}

static void prepare(zcl_review_wallet_input *claim, const uint8_t *data, size_t size)
{
    initialize();
    memset(&fixture.review, 0, sizeof(fixture.review));
    CHECK(fixture_open(&fixture.store) == 0);
    CHECK(fixture_create(&fixture.store, fixture.wallet.wallet, fixture.wallet.wallet_len) == ZCL_OK);
    const zcl_network network = data[8] % 2 == 0 ? ZCL_MAINNET : ZCL_TESTNET;
    CHECK(review_wallet_fixture_review(&fixture, network, 100) == 0);
    *claim = review_wallet_fixture_claim(&fixture, data[4] % 4, data[5] == 255 ? UINT32_MAX : data[5] % 4);
    memcpy(record, fixture.wallet.wallet, sizeof(record));
    memset(entropy, 0, sizeof(entropy));
    memset(journal, 0, sizeof(journal));
    memcpy(journal, fixture.wallet.state[0], 80);
    memcpy(journal + 80, fixture.wallet.state[1], 80);
    journal_len = 160;
    claim->record = record; claim->entropy = entropy;
    source_mutation(claim, data, size);
    CHECK(fixture_write(&fixture.store, ".change.index", journal, journal_len) == 0);
    if (data[0] % 11 == 8)
        CHECK(renameat(fixture.store.directory, "wallet.zcl", fixture.store.directory, ".wallet.pending") == 0);
    memcpy(&expected_owner, &fixture.review, sizeof(expected_owner));
}

static zcl_status lifetime_model(bool supplied, uint64_t id, uint64_t now)
{
    if (!supplied) return ZCL_INVALID_ARGUMENT;
    if (id != fixture.id) return ZCL_CANCELLED;
    if (now < 100) {
        memset(&expected_owner.data, 0, sizeof(expected_owner.data));
        return ZCL_CANCELLED;
    }
    if (now - 100 >= 90000) {
        memset(&expected_owner.data, 0, sizeof(expected_owner.data));
        return ZCL_TIMED_OUT;
    }
    expected_owner.data.last_ms = now;
    return ZCL_OK;
}

static bool sources_match(const zcl_review_wallet_input *claim)
{
    if (claim == NULL || claim->directory == NULL || claim->record == NULL || claim->entropy == NULL) return false;
    if (claim->directory_len != fixture_path_len() || claim->record_len != fixture.wallet.wallet_len) return false;
    if (claim->entropy_len != 16) return false;
    return memcmp(record, fixture.wallet.wallet, fixture.wallet.wallet_len) == 0 && memcmp(entropy, fixture.entropy, 16) == 0;
}

static bool key_matches(const zcl_review_wallet_input *claim, size_t input, const uint8_t *data)
{
    if (!sources_match(claim) || data[8] % 2 != 0 || data[0] % 11 == 8) return false;
    if (claim->index != 0 || claim->chain > 1 || input != claim->chain) return false;
    if (claim->chain == 0) return true;
    /* This primitive authenticates the head, not the entire journal history. */
    return journal_len == 160 && memcmp(journal + 80, fixture.wallet.state[1], 80) == 0;
}

static void preserved_files(bool expected_pending)
{
    uint8_t actual[140] = {0};
    size_t length = 0;
    bool pending = false;
    CHECK(fixture_read(&fixture.store, actual, sizeof(actual), &length, &pending) == ZCL_OK);
    CHECK(pending == expected_pending && length == fixture.wallet.wallet_len);
    CHECK(memcmp(actual, fixture.wallet.wallet, length) == 0);
    CHECK(change_bytes(&fixture.store, journal, journal_len, 0) == 0);
    struct stat info = {0};
    CHECK(fstatat(fixture.store.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(info.st_size >= 0 && (uint64_t)info.st_size == journal_len);
}

typedef struct { uint64_t times[3]; size_t calls; } signing_clock;

static zcl_status sample_signing(void *context, uint64_t *now)
{
    signing_clock *clock = context;
    CHECK(clock != NULL && now != NULL && clock->calls < 3);
    *now = clock->times[clock->calls++];
    return ZCL_OK;
}

static zcl_status advance_signing(uint64_t now)
{
    if (now < expected_owner.data.last_ms) {
        memset(&expected_owner.data, 0, sizeof(expected_owner.data)); return ZCL_CANCELLED;
    }
    if (now >= expected_owner.data.deadline_ms) {
        memset(&expected_owner.data, 0, sizeof(expected_owner.data)); return ZCL_TIMED_OUT;
    }
    expected_owner.data.last_ms = now;
    return ZCL_OK;
}

static void sign_claim(const zcl_review_wallet_input *candidate, uint64_t id, size_t input,
    const uint8_t *data, signing_clock *clock)
{
    fixture.review = initial_owner; expected_owner = initial_owner;
    zcl_status expected = lifetime_model(candidate != NULL, id, clock->times[0]);
    bool valid = expected == ZCL_OK && key_matches(candidate, input, data);
    size_t calls = candidate == NULL ? 0 : 1;
    if (valid) { ++calls; expected = advance_signing(clock->times[1]); valid = expected == ZCL_OK; }
    if (valid) { ++calls; expected = advance_signing(clock->times[2]); valid = expected == ZCL_OK; }
    const zcl_network network = initial_owner.data.assessment.network;
    const zcl_review_block block = {network, network == ZCL_MAINNET ? 1000000 : 100000, 0};
    const zcl_review_clock source = {sample_signing, clock};
    struct { uint8_t before[8]; zcl_signature signature; uint8_t after[8]; } output;
    memset(&output, 0xa5, sizeof(output));
    const zcl_status status = zcl_review_input_wallet_sign(&fixture.review, id, &source,
        input, &block, candidate, &output.signature);
    CHECK((status == ZCL_OK) == valid && clock->calls == calls);
    if (expected != ZCL_OK) CHECK(status == expected);
    CHECK(memcmp(&fixture.review, &expected_owner, sizeof(expected_owner)) == 0);
    for (size_t i = 0; i < 8; ++i) CHECK(output.before[i] == 0xa5 && output.after[i] == 0xa5);
    if (!valid) {
        const uint8_t *bytes = (const uint8_t *)&output.signature;
        for (size_t i = 0; i < sizeof(output.signature); ++i) CHECK(bytes[i] == 0xa5);
    }
}

static zcl_status sample_transaction(void *context, uint64_t *now)
{
    *now = *(const uint64_t *)context;
    return ZCL_OK;
}

static void check_change_claim(const zcl_review_wallet_input *candidate, uint64_t id,
    size_t output_index, const uint8_t *data, signing_clock *clock)
{
    fixture.review = initial_owner; expected_owner = initial_owner;
    zcl_status expected = lifetime_model(candidate != NULL, id, clock->times[0]);
    bool valid = expected == ZCL_OK && output_index == 1 && key_matches(candidate, 1, data);
    size_t calls = candidate == NULL ? 0 : 1;
    if (valid) { ++calls; expected = advance_signing(clock->times[1]); valid = expected == ZCL_OK; }
    const zcl_review_clock source = {sample_signing, clock};
    const zcl_status status = zcl_review_output_change_check(&fixture.review, id, &source,
        output_index, candidate);
    CHECK((status == ZCL_OK) == valid && clock->calls == calls);
    if (expected != ZCL_OK) CHECK(status == expected);
    CHECK(memcmp(&fixture.review, &expected_owner, sizeof(expected_owner)) == 0);
}

static void sign_transaction(const zcl_review_wallet_input *candidate, uint64_t id,
    uint64_t now, const uint8_t *data)
{
    fixture.review = initial_owner; expected_owner = initial_owner;
    zcl_review_wallet_input claims[2] = {{0}};
    if (candidate != NULL) {
        claims[0] = claims[1] = *candidate;
        claims[0].chain = 0; claims[1].chain = 1;
        claims[data[3] % 2].chain = candidate->chain;
    }
    const size_t count = data[11] == 255 ? SIZE_MAX : data[11] % 10;
    const zcl_status life = lifetime_model(candidate != NULL, id, now);
    const bool valid = life == ZCL_OK && count == 2 &&
        key_matches(&claims[0], 0, data) && key_matches(&claims[1], 1, data);
    const zcl_network network = initial_owner.data.assessment.network;
    const zcl_review_block block = {network, network == ZCL_MAINNET ? 1000000 : 100000, 0};
    const zcl_review_clock clock = {sample_transaction, &now};
    struct { uint8_t before[8], wire[ZCL_TX_WIRE_MAX], after[8]; } output;
    memset(&output, 0xa5, sizeof(output)); size_t length = SIZE_MAX;
    const zcl_status status = zcl_review_wallet_transaction_sign(&fixture.review, id,
        &clock, &block, candidate == NULL ? NULL : claims, count,
        output.wire, sizeof(output.wire), &length);
    CHECK((status == ZCL_OK) == valid);
    if (life != ZCL_OK) CHECK(status == life);
    CHECK(memcmp(&fixture.review, &expected_owner, sizeof(expected_owner)) == 0);
    for (size_t i = 0; i < 8; ++i) CHECK(output.before[i] == 0xa5 && output.after[i] == 0xa5);
    if (!valid) CHECK(length == SIZE_MAX);
    else CHECK(length <= sizeof(output.wire));
    for (size_t i = valid ? length : 0; i < sizeof(output.wire); ++i) CHECK(output.wire[i] == 0xa5);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 12 || size > 252) return 0;
    zcl_review_wallet_input claim = {0};
    prepare(&claim, data, size);
    const zcl_review_wallet_input *candidate = null_mutation(&claim, data);
    static const uint64_t times[] = {100, 90099, 90100, 99, UINT64_MAX};
    const uint64_t now = times[data[6] % 5];
    const uint64_t ids[] = {fixture.id, fixture.id + 1, 0, UINT64_MAX};
    const uint64_t id = ids[data[7] % 4];
    const size_t input = data[3] == 255 ? SIZE_MAX : data[3] % 10;
    initial_owner = fixture.review;
    const zcl_status life = lifetime_model(candidate != NULL, id, now);
    const zcl_status status = zcl_review_input_wallet_check(&fixture.review, id, now, input, candidate);
    if (life != ZCL_OK) CHECK(status == life);
    else CHECK((status == ZCL_OK) == key_matches(candidate, input, data));
    CHECK(memcmp(&expected_owner, &fixture.review, sizeof(expected_owner)) == 0);
    signing_clock clock = {{now, times[data[9] % 5], times[data[10] % 5]}, 0};
    sign_claim(candidate, id, input, data, &clock);
    sign_transaction(candidate, id, now, data);
    clock.calls = 0;
    check_change_claim(candidate, id, input, data, &clock);
    preserved_files(data[0] % 11 == 8);
    CHECK(review_wallet_fixture_close(&fixture) == 0);
    return 0;
}

#ifdef ZCL_REVIEW_WALLET_REGRESSION
int main(void)
{
    uint8_t data[252] = {0};
    for (uint8_t mode = 0; mode < 11; ++mode) {
        for (uint8_t clock = 0; clock < 5; ++clock) {
            for (uint8_t chain = 0; chain < 2; ++chain) {
                data[0] = mode; data[3] = data[4] = chain; data[6] = clock; data[11] = 2;
                CHECK(LLVMFuzzerTestOneInput(data, sizeof(data)) == 0);
            }
        }
    }
    CHECK(puts("Review wallet malformed claims preserve files and respect modeled lifetime") >= 0);
    return 0;
}
#endif
