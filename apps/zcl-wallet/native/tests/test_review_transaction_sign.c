/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "review_wallet_fixture.h"
#include <stdlib.h>
#include <string.h>

static review_wallet_fixture fixture;
static uint8_t expected[ZCL_TX_WIRE_MAX];
static size_t expected_length;
typedef struct { unsigned calls, fail_at, expire_at; } transaction_clock;

static zcl_status sample(void *context, uint64_t *now)
{
    transaction_clock *clock = context;
    if (clock == NULL || now == NULL || clock->calls >= 35) abort();
    ++clock->calls;
    *now = clock->calls == clock->expire_at ? 90100 : 100;
    return clock->calls == clock->fail_at ? ZCL_IO_FAILURE : ZCL_OK;
}

static zcl_review_block candidate(void)
{
    const zcl_review_block block = {fixture.network, fixture.network == ZCL_MAINNET ? 1000000 : 100000, 0};
    return block;
}

static void claims_for(zcl_review_wallet_input *claims)
{
    for (uint32_t i = 0; i < 2; ++i) claims[i] = review_wallet_fixture_claim(&fixture, i, 0);
}

static int setup(zcl_network network, size_t entropy_length)
{
    CHECK(review_wallet_fixture_open(&fixture, network, entropy_length, true) == 0);
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(&fixture.store, &fixture.wallet, &head) == ZCL_OK);
    CHECK(change_append(&fixture.store, &fixture.wallet, &head, 1) == ZCL_OK);
    zcl_review_wallet_input claims[2]; claims_for(claims);
    zcl_signature signatures[2];
    const zcl_review_block block = candidate();
    transaction_clock clock = {0};
    const zcl_review_clock source = {sample, &clock};
    for (size_t i = 0; i < 2; ++i)
        CHECK(zcl_review_input_wallet_sign(&fixture.review, fixture.id, &source, i,
            &block, &claims[i], &signatures[i]) == ZCL_OK);
    CHECK(zcl_review_p2pkh_complete(&fixture.review, fixture.id, &source, &block,
        signatures, 2, expected, sizeof(expected), &expected_length) == ZCL_OK);
    return 0;
}

static int call(const zcl_review_wallet_input *claims, size_t count, size_t capacity,
    transaction_clock *clock, zcl_status status)
{
    const zcl_review_block block = candidate();
    const zcl_review_clock source = {sample, clock};
    struct { uint8_t before[8], bytes[ZCL_TX_WIRE_MAX], after[8]; } output;
    memset(&output, 0xa5, sizeof(output)); size_t length = SIZE_MAX;
    CHECK(zcl_review_wallet_transaction_sign(&fixture.review, fixture.id, &source, &block,
        claims, count, output.bytes, capacity, &length) == status);
    for (size_t i = 0; i < 8; ++i) CHECK(output.before[i] == 0xa5 && output.after[i] == 0xa5);
    if (status == ZCL_OK) {
        CHECK(length == expected_length && memcmp(output.bytes, expected, length) == 0);
        CHECK(clock->calls == 11);
    } else CHECK(length == SIZE_MAX);
    for (size_t i = status == ZCL_OK ? length : 0; i < sizeof(output.bytes); ++i) CHECK(output.bytes[i] == 0xa5);
    return 0;
}

static int matrix(void)
{
    for (size_t network = 0; network < 2; ++network) {
        for (size_t entropy_length = 16; entropy_length <= 32; entropy_length += 4) {
            CHECK(setup((zcl_network)network, entropy_length) == 0);
            zcl_review_wallet_input claims[2]; claims_for(claims);
            transaction_clock clock = {0};
            CHECK(call(claims, 2, SIZE_MAX, &clock, ZCL_OK) == 0);
            clock = (transaction_clock){0};
            CHECK(call(claims, 2, expected_length, &clock, ZCL_OK) == 0);
            clock = (transaction_clock){0};
            CHECK(call(claims, 2, expected_length - 1, &clock, ZCL_BUFFER_TOO_SMALL) == 0);
            CHECK(change_bytes(&fixture.store, fixture.wallet.state[0], 80, 0) == 0);
            CHECK(change_bytes(&fixture.store, fixture.wallet.state[1], 80, 80) == 0);
            CHECK(review_wallet_fixture_close(&fixture) == 0);
        }
    }
    return 0;
}

static int refusals(void)
{
    CHECK(setup(ZCL_MAINNET, 16) == 0);
    zcl_review_wallet_input claims[2]; claims_for(claims);
    const size_t counts[] = {0,1,3,8,9,SIZE_MAX};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        transaction_clock clock = {0};
        CHECK(call(claims, counts[i], sizeof(expected), &clock, ZCL_OUT_OF_RANGE) == 0);
        CHECK(clock.calls == 1);
    }
    claims[1].chain = 0;
    transaction_clock clock = {0};
    CHECK(call(claims, 2, sizeof(expected), &clock, ZCL_NOT_FOUND) == 0);
    CHECK(clock.calls == 3); /* Both ownership checks, no signing yet. */
    CHECK(review_wallet_fixture_close(&fixture) == 0);
    for (unsigned phase = 1; phase <= 11; ++phase) {
        CHECK(setup(ZCL_TESTNET, 16) == 0); claims_for(claims);
        clock = (transaction_clock){0,phase,0};
        CHECK(call(claims, 2, sizeof(expected), &clock, ZCL_IO_FAILURE) == 0);
        CHECK(clock.calls == phase);
        clock = (transaction_clock){0,0,phase};
        CHECK(call(claims, 2, sizeof(expected), &clock, ZCL_TIMED_OUT) == 0);
        CHECK(clock.calls == phase && fixture.review.data.id == 0);
        CHECK(review_wallet_fixture_close(&fixture) == 0);
    }
    return 0;
}

int main(void)
{
    CHECK(matrix() == 0 && refusals() == 0);
    puts("Complete wallet signing: exact wire, all-input admission and atomic failure passed");
    return 0;
}
