/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "review_wallet_fixture.h"
#include <string.h>

static int record_profile(review_wallet_fixture *fixture)
{
    uint8_t header[80] = {0}, blind[32] = {1}, iv[12] = {0}, cipher[48] = {0};
    CHECK(zcl_wallet_header_create(fixture->entropy, fixture->entropy_len, fixture->network,
        blind, 32, header, 80) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, 80, iv, 12, cipher, fixture->entropy_len + 16,
        fixture->wallet.wallet, sizeof(fixture->wallet.wallet), &fixture->wallet.wallet_len) == ZCL_OK);
    for (uint32_t index = 0; index < 3; ++index)
        CHECK(zcl_change_state_encode(header, 80, fixture->entropy, fixture->entropy_len, blind, 32,
            index, fixture->wallet.state[index], 80) == ZCL_OK);
    return 0;
}

static int funding_profile(review_wallet_fixture *fixture, size_t source)
{
    CHECK(source < 2);
    uint8_t text[35] = {0}, blind[32] = {1};
    size_t length = 0;
    zcl_status status = source == 0
        ? zcl_receive_from_entropy(fixture->entropy, fixture->entropy_len, fixture->network,
            0, blind, 32, text, 35, &length)
        : zcl_change_from_entropy(fixture->entropy, fixture->entropy_len, fixture->network,
            0, blind, 32, text, 35, &length);
    CHECK(status == ZCL_OK && length == 35);
    zcl_address destination = {0};
    CHECK(zcl_address_parse(text, length, fixture->network, &destination) == ZCL_OK);
    zcl_tx_output *output = &fixture->funding.previous[source].outputs[source];
    CHECK(zcl_address_script(&destination, output->script, sizeof(output->script), &output->script_len) == ZCL_OK);
    CHECK(assessment_fixture_rebind(&fixture->funding, source));
    return 0;
}

int review_wallet_fixture_review(review_wallet_fixture *fixture, zcl_network network, uint64_t now_ms)
{
    CHECK(zcl_transaction_serialize(&fixture->funding.spending, fixture->draft,
        sizeof(fixture->draft), &fixture->draft_length) == ZCL_OK);
    CHECK(zcl_review_open(&fixture->review, fixture->draft, fixture->draft_length, network,
        fixture->funding.sources, fixture->funding.spending.input_count, 500, now_ms, &fixture->id) == ZCL_OK);
    return 0;
}

int review_wallet_fixture_open(review_wallet_fixture *fixture, zcl_network network,
    size_t entropy_len, bool with_state)
{
    CHECK(fixture != NULL && entropy_len >= 16 && entropy_len <= 32 && entropy_len % 4 == 0);
    memset(fixture, 0, sizeof(*fixture));
    fixture->store.directory = -1;
    fixture->network = network;
    fixture->entropy_len = entropy_len;
    CHECK(assessment_fixture_init(&fixture->funding));
    CHECK(record_profile(fixture) == 0 && funding_profile(fixture, 0) == 0 && funding_profile(fixture, 1) == 0);
    CHECK(fixture_open(&fixture->store) == 0);
    const zcl_status status = with_state ? change_create(&fixture->store, &fixture->wallet)
        : fixture_create(&fixture->store, fixture->wallet.wallet, fixture->wallet.wallet_len);
    CHECK(status == ZCL_OK);
    return review_wallet_fixture_review(fixture, network, 100);
}

int review_wallet_fixture_close(review_wallet_fixture *fixture)
{
    zcl_review_clear(&fixture->review);
    zcl_secure_zero(fixture->entropy, sizeof(fixture->entropy));
    return fixture_close(&fixture->store);
}

zcl_review_wallet_input review_wallet_fixture_claim(const review_wallet_fixture *fixture,
    uint32_t chain, uint32_t index)
{
    zcl_review_wallet_input claim = {0};
    claim.directory = (const uint8_t *)fixture->store.path;
    claim.directory_len = fixture_path_len();
    claim.record = fixture->wallet.wallet;
    claim.record_len = fixture->wallet.wallet_len;
    claim.entropy = fixture->entropy;
    claim.entropy_len = fixture->entropy_len;
    claim.chain = chain;
    claim.index = index;
    return claim;
}
