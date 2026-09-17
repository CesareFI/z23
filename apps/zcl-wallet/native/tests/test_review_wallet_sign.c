/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "review_wallet_fixture.h"
#ifdef ZCL_SIGNATURE_ORACLE
#include "signature_oracle.h"
#include "sighash_oracle.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static review_wallet_fixture fixture;
static zcl_signature signatures[2];
static uint8_t signed_wire[ZCL_TX_WIRE_MAX];
typedef struct { uint64_t times[3]; unsigned calls, fail_at, omit_at; } signing_clock;

static zcl_status read_clock(void *context, uint64_t *now)
{
    signing_clock *clock = context;
    if (clock == NULL || now == NULL || clock->calls >= 3) abort();
    const unsigned index = clock->calls++;
    if (clock->omit_at != clock->calls) *now = clock->times[index];
    return clock->calls == clock->fail_at ? ZCL_IO_FAILURE : ZCL_OK;
}

static zcl_review_block block(void)
{
    const zcl_review_block value = {fixture.network, fixture.network == ZCL_MAINNET ? 1000000 : 100000, 0};
    return value;
}

static int sign_input(const zcl_review_wallet_input *claim, size_t index, signing_clock *clock,
    zcl_status expected, unsigned calls, zcl_signature *output)
{
    const zcl_review_clock source = {read_clock, clock};
    const zcl_review_block candidate = block();
    struct { uint8_t before[8]; zcl_signature value; uint8_t after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_review_input_wallet_sign(&fixture.review, fixture.id, &source, index, &candidate,
        claim, &box.value) == expected);
    CHECK(clock->calls == calls);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    if (expected == ZCL_OK) {
        CHECK(output != NULL); *output = box.value;
        CHECK(fixture.review.data.last_ms == clock->times[2] && fixture.review.data.deadline_ms == 90100);
    } else {
        const uint8_t *bytes = (const uint8_t *)&box.value;
        for (size_t i = 0; i < sizeof(box.value); ++i) CHECK(bytes[i] == 0xa5);
    }
    return 0;
}

static int consume(void)
{
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(&fixture.store, &fixture.wallet, &head) == ZCL_OK);
    CHECK(change_append(&fixture.store, &fixture.wallet, &head, 1) == ZCL_OK);
    return 0;
}

static int verify_signed(const uint8_t *wire, size_t length, size_t index)
{
    uint8_t digest[32] = {0};
    const zcl_review_block candidate = block();
    CHECK(zcl_review_sighash_context(&fixture.review, fixture.id, 100, index,
        &candidate, digest, sizeof(digest)) == ZCL_OK);
    const zcl_address *address = &fixture.review.data.assessment.inputs[index].destination;
#ifdef ZCL_SIGNATURE_ORACLE
    uint8_t script[25] = {0x76,0xa9,0x14}, independent[32] = {0};
    memcpy(script + 3, address->hash, 20); script[23] = 0x88; script[24] = 0xac;
    zcl_test_sighash_all(wire, length, index, script, sizeof(script), index == 0 ? 10000 : 1000,
        UINT32_C(0x930b540d), independent, sizeof(independent));
    CHECK(memcmp(digest, independent, 32) == 0);
    CHECK(zcl_test_signature_script_oracle(&signatures[index], independent, 32, address->hash, 20) == 1);
#else
    (void)wire; (void)length;
    uint8_t script[ZCL_SIGNATURE_SCRIPT_MAX] = {0};
    size_t script_length = 0;
    CHECK(zcl_signature_p2pkh(&signatures[index], digest, 32, address->hash, 20,
        script, sizeof(script), &script_length) == ZCL_OK);
#endif
    return 0;
}

static int matrix(zcl_network network, size_t entropy_length)
{
    CHECK(review_wallet_fixture_open(&fixture, network, entropy_length, true) == 0);
    CHECK(consume() == 0);
    for (uint32_t chain = 0; chain < 2; ++chain) {
        const zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, chain, 0);
        signing_clock clock = {{100,100,100}, 0, 0, 0};
        CHECK(sign_input(&claim, chain, &clock, ZCL_OK, 3, &signatures[chain]) == 0);
        zcl_signature repeated;
        clock = (signing_clock){{100,100,100}, 0, 0, 0};
        CHECK(sign_input(&claim, chain, &clock, ZCL_OK, 3, &repeated) == 0);
        CHECK(memcmp(&repeated, &signatures[chain], sizeof(repeated)) == 0);
    }
    const zcl_review_block candidate = block();
    signing_clock clock = {{100,100,100}, 0, 0, 0};
    const zcl_review_clock source = {read_clock, &clock};
    size_t length = 0;
    CHECK(zcl_review_p2pkh_complete(&fixture.review, fixture.id, &source, &candidate, signatures, 2,
        signed_wire, sizeof(signed_wire), &length) == ZCL_OK);
    CHECK(clock.calls == 2);
    CHECK(verify_signed(signed_wire, length, 0) == 0 && verify_signed(signed_wire, length, 1) == 0);
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(&fixture.store, &fixture.wallet, &head) == ZCL_OK);
    CHECK(head.file_bytes == 160 && head.tail_len == 80 && memcmp(head.tail, fixture.wallet.state[1], 80) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[1], 80, 80) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int ownership_refusals(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, true) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    signing_clock clock = {{100,100,100}, 0, 0, 0};
    CHECK(sign_input(&claim, 1, &clock, ZCL_NOT_FOUND, 1, NULL) == 0);
    claim.chain = 0;
    clock.calls = 0;
    CHECK(sign_input(&claim, 1, &clock, ZCL_NOT_FOUND, 1, NULL) == 0);
    claim.index = 1; clock.calls = 0;
    CHECK(sign_input(&claim, 0, &clock, ZCL_UNSUPPORTED, 1, NULL) == 0);
    claim.index = 0;
    uint8_t wrong_entropy[16] = {1}; claim.entropy = wrong_entropy; clock.calls = 0;
    CHECK(sign_input(&claim, 0, &clock, ZCL_INVALID_ENCODING, 1, NULL) == 0);
    claim.entropy = fixture.entropy;
    uint8_t altered[140]; memcpy(altered, fixture.wallet.wallet, sizeof(altered));
    altered[fixture.wallet.wallet_len - 1] ^= 1; claim.record = altered; clock.calls = 0;
    CHECK(sign_input(&claim, 0, &clock, ZCL_ALREADY_EXISTS, 1, NULL) == 0);
    claim.record = fixture.wallet.wallet;
    CHECK(renameat(fixture.store.directory, "wallet.zcl", fixture.store.directory, ".wallet.pending") == 0);
    clock.calls = 0;
    CHECK(sign_input(&claim, 0, &clock, ZCL_NOT_FOUND, 1, NULL) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int clock_refusals(void)
{
    for (unsigned phase = 1; phase <= 3; ++phase) {
        for (unsigned mode = 0; mode < 4; ++mode) {
            CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 16, false) == 0);
            const zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
            signing_clock clock = {{200,300,400}, 0, 0, 0};
            zcl_status expected = ZCL_TIMED_OUT;
            if (mode == 0) clock.times[phase - 1] = 90100;
            if (mode == 1) { clock.times[phase - 1] = 99; expected = ZCL_CANCELLED; }
            if (mode == 2) { clock.fail_at = phase; expected = ZCL_IO_FAILURE; }
            if (mode == 3) clock.omit_at = phase;
            CHECK(sign_input(&claim, 0, &clock, expected, phase, NULL) == 0);
            CHECK(review_wallet_fixture_close(&fixture) == 0);
        }
    }
    return 0;
}

static int argument_refusals(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 16, false) == 0);
    const zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    const zcl_review_block candidate = block();
    for (unsigned field = 0; field < 6; ++field) {
        signing_clock clock = {{100,100,100}, 0, 0, 0};
        const zcl_review_clock source = {field == 2 ? NULL : read_clock, &clock};
        zcl_signature output, before;
        memset(&output, 0xa5, sizeof(output)); before = output;
        CHECK(zcl_review_input_wallet_sign(field == 0 ? NULL : &fixture.review, fixture.id,
            field == 1 ? NULL : &source, 0, field == 3 ? NULL : &candidate,
            field == 4 ? NULL : &claim, field == 5 ? NULL : &output) == ZCL_INVALID_ARGUMENT);
        CHECK(clock.calls == 0 && memcmp(&output, &before, sizeof(output)) == 0);
    }
    signing_clock clock = {{100,100,100}, 0, 0, 0};
    CHECK(sign_input(&claim, SIZE_MAX, &clock, ZCL_OUT_OF_RANGE, 1, NULL) == 0);
    return review_wallet_fixture_close(&fixture);
}

int main(void)
{
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4) CHECK(matrix((zcl_network)network, length) == 0);
    CHECK(ownership_refusals() == 0 && clock_refusals() == 0 && argument_refusals() == 0);
    puts("Wallet review signing: exact receive/change signatures, completed wire and refusal atomicity passed");
    return 0;
}
