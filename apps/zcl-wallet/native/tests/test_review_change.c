/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "review_wallet_fixture.h"
#include <stdlib.h>
#include <string.h>

static review_wallet_fixture fixture;
static zcl_review_owner saved;
typedef struct { uint64_t times[2]; unsigned calls, fail_at, omit_at; } change_clock;

static zcl_status sample(void *context, uint64_t *now)
{
    change_clock *clock = context;
    if (clock == NULL || now == NULL || clock->calls >= 2) abort();
    const uint64_t time = clock->times[clock->calls++];
    if (clock->calls != clock->omit_at) *now = time;
    return clock->calls == clock->fail_at ? ZCL_IO_FAILURE : ZCL_OK;
}

static int change_output(size_t count)
{
    zcl_review_clear(&fixture.review);
    const zcl_tx_output destination = fixture.funding.previous[1].outputs[1];
    const zcl_tx_output recipient = fixture.funding.spending.outputs[0];
    fixture.funding.spending.output_count = count;
    for (size_t i = 0; i < count; ++i) {
        fixture.funding.spending.outputs[i] = i == count - 1 ? destination : recipient;
        fixture.funding.spending.outputs[i].value = i == count - 1 ? 1500 : i == 0 ? 9000 : 0;
    }
    return review_wallet_fixture_review(&fixture, fixture.network, 100);
}

static int consumed(void)
{
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(&fixture.store, &fixture.wallet, &head) == ZCL_OK);
    CHECK(change_append(&fixture.store, &fixture.wallet, &head, 1) == ZCL_OK);
    return 0;
}

static zcl_status check(uint64_t id, size_t output_index, const zcl_review_wallet_input *claim,
    change_clock *clock)
{
    const zcl_review_clock source = {sample, clock};
    return zcl_review_output_change_check(&fixture.review, id, &source, output_index, claim);
}

static int matrix(zcl_network network, size_t entropy_len)
{
    CHECK(review_wallet_fixture_open(&fixture, network, entropy_len, true) == 0);
    CHECK(change_output(2) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    change_clock clock = {{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_NOT_FOUND && clock.calls == 1);
    CHECK(consumed() == 0);
    saved = fixture.review;
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_OK && clock.calls == 2);
    CHECK(memcmp(&saved, &fixture.review, sizeof(saved)) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[1], 80, 80) == 0);
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 0, &claim, &clock) == ZCL_NOT_FOUND && clock.calls == 1);
    claim.chain = 0;
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_UNSUPPORTED && clock.calls == 1);
    return review_wallet_fixture_close(&fixture);
}

static int maximum_outputs(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 32, true) == 0);
    CHECK(change_output(ZCL_TX_OUTPUT_MAX) == 0 && consumed() == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    /* Destroy borrowed transaction bytes: classification must use the review. */
    memset(&fixture.funding, 0, sizeof(fixture.funding));
    memset(fixture.draft, 0, sizeof(fixture.draft));
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        change_clock clock = {{100,100},0,0,0};
        CHECK(check(fixture.id, i, &claim, &clock) == (i == ZCL_TX_OUTPUT_MAX - 1 ? ZCL_OK : ZCL_NOT_FOUND));
    }
    const size_t invalid[] = {ZCL_TX_OUTPUT_MAX, SIZE_MAX};
    for (size_t i = 0; i < 2; ++i) {
        change_clock clock = {{100,100},0,0,0};
        CHECK(check(fixture.id, invalid[i], &claim, &clock) == ZCL_OUT_OF_RANGE && clock.calls == 1);
    }
    return review_wallet_fixture_close(&fixture);
}

static int lifetime(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, true) == 0);
    CHECK(change_output(2) == 0 && consumed() == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    saved = fixture.review;
    const change_clock cases[] = {
        {{90100,100},0,0,0}, {{100,90100},0,0,0}, {{99,100},0,0,0}, {{100,99},0,0,0},
        {{100,100},0,1,0}, {{100,100},0,2,0}, {{100,100},0,0,1}, {{100,100},0,0,2}
    };
    const zcl_status statuses[] = {ZCL_TIMED_OUT,ZCL_TIMED_OUT,ZCL_CANCELLED,ZCL_CANCELLED,
        ZCL_IO_FAILURE,ZCL_IO_FAILURE,ZCL_TIMED_OUT,ZCL_TIMED_OUT};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        fixture.review = saved;
        change_clock clock = cases[i];
        CHECK(check(fixture.id, 1, &claim, &clock) == statuses[i]);
        CHECK(clock.calls == (i % 2 == 0 ? 1u : 2u));
        CHECK(fixture.review.data.id == (statuses[i] == ZCL_IO_FAILURE ? fixture.id : 0));
    }
    fixture.review = saved;
    change_clock clock = {{100,90099},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_OK && fixture.review.data.last_ms == 90099);
    clock = (change_clock){{90099,90099},0,0,0};
    CHECK(check(fixture.id + 1, 1, &claim, &clock) == ZCL_CANCELLED && clock.calls == 1);
    return review_wallet_fixture_close(&fixture);
}

static int wrong_wallet(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 16, true) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    change_clock clock = {{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_UNSUPPORTED); /* Original P2SH output. */
    CHECK(change_output(2) == 0 && consumed() == 0);
    uint8_t altered[140]; memcpy(altered, fixture.wallet.wallet, sizeof(altered));
    altered[fixture.wallet.wallet_len - 1] ^= 1; claim.record = altered;
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_ALREADY_EXISTS && clock.calls == 1);
    claim = review_wallet_fixture_claim(&fixture, 1, 0);
    uint8_t wrong[16] = {1}; claim.entropy = wrong;
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_INVALID_ENCODING && clock.calls == 1);
    claim = review_wallet_fixture_claim(&fixture, 1, UINT32_MAX);
    clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_OUT_OF_RANGE && clock.calls == 1);
    return review_wallet_fixture_close(&fixture);
}

static int receive_is_not_change(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, true) == 0);
    CHECK(consumed() == 0);
    zcl_tx_output *output = &fixture.funding.spending.outputs[1];
    CHECK(zcl_address_script(&fixture.review.data.assessment.inputs[0].destination,
        output->script, sizeof(output->script), &output->script_len) == ZCL_OK);
    zcl_review_clear(&fixture.review);
    CHECK(review_wallet_fixture_review(&fixture, fixture.network, 100) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    change_clock clock = {{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_UNSUPPORTED && clock.calls == 1);
    claim.chain = 1; clock = (change_clock){{100,100},0,0,0};
    CHECK(check(fixture.id, 1, &claim, &clock) == ZCL_NOT_FOUND && clock.calls == 1);
    const zcl_review_clock source = {sample, &clock}, empty = {NULL,NULL};
    CHECK(zcl_review_output_change_check(NULL, fixture.id, &source, 1, &claim) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_output_change_check(&fixture.review, fixture.id, NULL, 1, &claim) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_output_change_check(&fixture.review, fixture.id, &empty, 1, &claim) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_output_change_check(&fixture.review, fixture.id, &source, 1, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(clock.calls == 1);
    return review_wallet_fixture_close(&fixture);
}

int main(void)
{
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4) CHECK(matrix((zcl_network)network, length) == 0);
    CHECK(maximum_outputs() == 0 && lifetime() == 0);
    CHECK(wrong_wallet() == 0 && receive_is_not_change() == 0);
    puts("Reviewed change matches only a consumed wallet path at live completion");
    return 0;
}
