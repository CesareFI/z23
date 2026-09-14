/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "review_wallet_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static review_wallet_fixture fixture;
static zcl_review_owner saved;

static int check(uint64_t id, uint64_t now, size_t input, const zcl_review_wallet_input *claim, zcl_status expected)
{
    CHECK(zcl_review_input_wallet_check(&fixture.review, id, now, input, claim) == expected);
    return 0;
}

static int consume(size_t next)
{
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(&fixture.store, &fixture.wallet, &head) == ZCL_OK);
    CHECK(change_append(&fixture.store, &fixture.wallet, &head, next) == ZCL_OK);
    return 0;
}

static int matrix(zcl_network network, size_t entropy_len)
{
    CHECK(review_wallet_fixture_open(&fixture, network, entropy_len, true) == 0);
    zcl_review_wallet_input receive = review_wallet_fixture_claim(&fixture, 0, 0);
    zcl_review_wallet_input change = review_wallet_fixture_claim(&fixture, 1, 0);
    memcpy(&saved, &fixture.review, sizeof(saved));
    CHECK(check(fixture.id, 100, 0, &receive, ZCL_OK) == 0);
    CHECK(check(fixture.id, 100, 1, &receive, ZCL_NOT_FOUND) == 0);
    CHECK(check(fixture.id, 100, 1, &change, ZCL_NOT_FOUND) == 0); /* Not consumed yet. */
    CHECK(consume(1) == 0);
    CHECK(check(fixture.id, 100, 1, &change, ZCL_OK) == 0);
    CHECK(check(fixture.id, 100, 0, &change, ZCL_NOT_FOUND) == 0);
    change.index = 1;
    CHECK(check(fixture.id, 100, 1, &change, ZCL_NOT_FOUND) == 0);
    CHECK(consume(2) == 0);
    CHECK(check(fixture.id, 100, 1, &change, ZCL_NOT_FOUND) == 0); /* Consumed, wrong key. */
    change.index = 0;
    CHECK(check(fixture.id, 100, 1, &change, ZCL_OK) == 0);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[1], 80, 80) == 0);
    CHECK(change_bytes(&fixture.store, fixture.wallet.state[2], 80, 160) == 0);
    for (size_t i = 0; i < sizeof(fixture.entropy); ++i) CHECK(fixture.entropy[i] == 0);
    return review_wallet_fixture_close(&fixture);
}

static int receive_without_state(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, false) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_OK) == 0);
    claim.chain = 1;
    CHECK(check(fixture.id, 100, 1, &claim, ZCL_NOT_FOUND) == 0);
    struct stat info = {0};
    CHECK(fstatat(fixture.store.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(renameat(fixture.store.directory, "wallet.zcl", fixture.store.directory, ".wallet.pending") == 0);
    claim.chain = 0;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_NOT_FOUND) == 0);
    uint8_t record[140] = {0};
    size_t length = 0;
    bool pending = false;
    CHECK(fixture_read(&fixture.store, record, sizeof(record), &length, &pending) == ZCL_OK && pending);
    CHECK(length == fixture.wallet.wallet_len && memcmp(record, fixture.wallet.wallet, length) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int maximum_inputs(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 32, false) == 0);
    zcl_review_clear(&fixture.review);
    zcl_transparent_tx *previous = &fixture.funding.previous[0];
    const zcl_tx_output destination = previous->outputs[0];
    previous->output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        previous->outputs[i] = destination;
        previous->outputs[i].value = i == 8 ? 10000 : 0;
    }
    CHECK(assessment_fixture_rebind(&fixture.funding, 0));
    zcl_transparent_tx *spending = &fixture.funding.spending;
    spending->input_count = ZCL_TX_INPUT_MAX;
    spending->output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        spending->inputs[i] = spending->inputs[0];
        spending->inputs[i].previous_index = (uint32_t)(8 + i);
        fixture.funding.sources[i] = fixture.funding.sources[0];
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        spending->outputs[i] = destination;
        spending->outputs[i].value = i == 0 ? 9500 : 0;
    }
    CHECK(review_wallet_fixture_review(&fixture, ZCL_TESTNET, 100) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    memcpy(&saved, &fixture.review, sizeof(saved));
    memset(&fixture.funding, 0, sizeof(fixture.funding)); /* Review owns the funding result. */
    memset(fixture.draft, 0, sizeof(fixture.draft));
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) CHECK(check(fixture.id, 100, i, &claim, ZCL_OK) == 0);
    CHECK(check(fixture.id, 100, 8, &claim, ZCL_OUT_OF_RANGE) == 0);
    CHECK(check(fixture.id, 100, SIZE_MAX, &claim, ZCL_OUT_OF_RANGE) == 0);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int wrong_sources(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, true) == 0);
    CHECK(consume(1) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    uint8_t wrong[16] = {1};
    claim.entropy = wrong;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_INVALID_ENCODING) == 0);
    claim.chain = 1;
    CHECK(check(fixture.id, 100, 1, &claim, ZCL_INVALID_ENCODING) == 0);
    claim.entropy = fixture.entropy;
    uint8_t altered[140];
    memcpy(altered, fixture.wallet.wallet, sizeof(altered));
    altered[fixture.wallet.wallet_len - 1] ^= 1;
    claim.record = altered;
    CHECK(check(fixture.id, 100, 1, &claim, ZCL_ALREADY_EXISTS) == 0);
    claim.chain = 0;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_ALREADY_EXISTS) == 0);
    claim.record = fixture.wallet.wallet;
    zcl_review_clear(&fixture.review);
    CHECK(review_wallet_fixture_review(&fixture, ZCL_TESTNET, 100) == 0);
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_UNSUPPORTED) == 0);
    claim.chain = 1;
    CHECK(check(fixture.id, 100, 1, &claim, ZCL_UNSUPPORTED) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int lifetime(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, false) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    CHECK(check(fixture.id, 90099, 0, &claim, ZCL_OK) == 0);
    CHECK(fixture.review.data.last_ms == 90099 && fixture.review.data.deadline_ms == 90100);
    CHECK(check(fixture.id, 90100, 0, &claim, ZCL_TIMED_OUT) == 0);
    CHECK(fixture.review.data.id == 0);
    const uint64_t old = fixture.id;
    CHECK(review_wallet_fixture_review(&fixture, ZCL_MAINNET, 100000) == 0);
    memcpy(&saved, &fixture.review, sizeof(saved));
    CHECK(check(old, UINT64_MAX, 0, &claim, ZCL_CANCELLED) == 0);
    CHECK(check(0, 0, 0, &claim, ZCL_CANCELLED) == 0);
    CHECK(check(UINT64_MAX, 0, 0, &claim, ZCL_CANCELLED) == 0);
    CHECK(memcmp(&fixture.review, &saved, sizeof(saved)) == 0);
    claim.chain = 2;
    CHECK(check(fixture.id, 100001, 0, &claim, ZCL_UNSUPPORTED) == 0);
    CHECK(fixture.review.data.last_ms == 100001 && fixture.review.data.deadline_ms == 190000);
    CHECK(check(fixture.id, 100000, 0, &claim, ZCL_CANCELLED) == 0);
    CHECK(fixture.review.data.id == 0);
    return review_wallet_fixture_close(&fixture);
}

static int bounds(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_TESTNET, 16, false) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    CHECK(zcl_review_input_wallet_check(NULL, fixture.id, 100, 0, &claim) == ZCL_INVALID_ARGUMENT);
    CHECK(check(fixture.id, UINT64_MAX, 0, NULL, ZCL_INVALID_ARGUMENT) == 0);
    claim.index = 1;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_UNSUPPORTED) == 0);
    claim.chain = 1; claim.index = UINT32_MAX;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    const size_t lengths[] = {0, 1, 15, 17, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        claim = review_wallet_fixture_claim(&fixture, 0, 0);
        claim.entropy_len = lengths[i];
        CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    }
    claim = review_wallet_fixture_claim(&fixture, 0, 0);
    claim.directory_len = SIZE_MAX;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    claim.directory_len = 0;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    claim = review_wallet_fixture_claim(&fixture, 0, 0);
    claim.record_len = SIZE_MAX;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    for (size_t length = 0; length < fixture.wallet.wallet_len; ++length) {
        claim.record_len = length;
        CHECK(check(fixture.id, 100, 0, &claim, ZCL_OUT_OF_RANGE) == 0);
    }
    return review_wallet_fixture_close(&fixture);
}

static int nulls_and_p2sh(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, false) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    claim.directory = NULL;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_INVALID_ARGUMENT) == 0);
    claim = review_wallet_fixture_claim(&fixture, 0, 0);
    claim.record = NULL;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_INVALID_ARGUMENT) == 0);
    claim = review_wallet_fixture_claim(&fixture, 0, 0);
    claim.entropy = NULL;
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_INVALID_ARGUMENT) == 0);
    claim = review_wallet_fixture_claim(&fixture, 0, 0);
    zcl_address destination = fixture.review.data.assessment.inputs[0].destination;
    destination.kind = ZCL_P2SH;
    zcl_tx_output *output = &fixture.funding.previous[0].outputs[0];
    CHECK(zcl_address_script(&destination, output->script, sizeof(output->script), &output->script_len) == ZCL_OK);
    CHECK(assessment_fixture_rebind(&fixture.funding, 0));
    zcl_review_clear(&fixture.review);
    CHECK(review_wallet_fixture_review(&fixture, ZCL_MAINNET, 100) == 0);
    CHECK(check(fixture.id, 100, 0, &claim, ZCL_UNSUPPORTED) == 0);
    return review_wallet_fixture_close(&fixture);
}

static int damaged_journal(void)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, true) == 0);
    CHECK(consume(1) == 0);
    zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 1, 0);
    uint8_t bytes[160];
    memcpy(bytes, fixture.wallet.state[0], 80);
    memcpy(bytes + 80, fixture.wallet.state[1], 80);
    for (size_t i = 0; i < 80; ++i) {
        bytes[80 + i] ^= 1;
        CHECK(unlinkat(fixture.store.directory, ".change.index", 0) == 0);
        CHECK(fixture_write(&fixture.store, ".change.index", bytes, sizeof(bytes)) == 0);
        CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, 1, &claim) != ZCL_OK);
        CHECK(change_bytes(&fixture.store, bytes, sizeof(bytes), 0) == 0);
        bytes[80 + i] ^= 1;
    }
    CHECK(unlinkat(fixture.store.directory, ".change.index", 0) == 0);
    CHECK(fixture_write(&fixture.store, ".change.index", bytes, 159) == 0);
    CHECK(zcl_review_input_wallet_check(&fixture.review, fixture.id, 100, 1, &claim) != ZCL_OK);
    CHECK(change_bytes(&fixture.store, bytes, 159, 0) == 0);
    return review_wallet_fixture_close(&fixture);
}

int main(void)
{
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4) CHECK(matrix((zcl_network)network, length) == 0);
    CHECK(receive_without_state() == 0);
    CHECK(maximum_inputs() == 0);
    CHECK(wrong_sources() == 0);
    CHECK(lifetime() == 0);
    CHECK(bounds() == 0);
    CHECK(nulls_and_p2sh() == 0);
    CHECK(damaged_journal() == 0);
    CHECK(puts("Live review input destinations match only their committed recovered wallet and supported key location") >= 0);
    return 0;
}
