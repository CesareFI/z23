/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "draft_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Draft check failed at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_draft_request request;

static void reset(zcl_network network)
{
    CHECK(draft_fixture_init(&request, &fixture, network));
}

static void rebind(size_t index)
{
    CHECK(assessment_fixture_rebind(&fixture, index));
    request.inputs[index].previous = fixture.sources[index];
}

static void refused(const zcl_draft_request *input, zcl_status expected)
{
    struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_draft_request original;
    if (input != NULL) memcpy(&original, input, sizeof(original));
    const zcl_status status = zcl_transaction_draft(input, &box.tx);
    CHECK(status != ZCL_OK && (expected == ZCL_OK || status == expected));
    const uint8_t *bytes = (const uint8_t *)&box;
    for (size_t i = 0; i < sizeof(box); ++i) CHECK(bytes[i] == 0xa5);
    if (input != NULL) CHECK(memcmp(&original, input, sizeof(original)) == 0);
}

static void unsigned_rows(const zcl_transparent_tx *tx)
{
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        CHECK(tx->inputs[i].script_len == 0);
        for (size_t j = 0; j < ZCL_TX_INPUT_SCRIPT_MAX; ++j) CHECK(tx->inputs[i].script[j] == 0);
        if (i >= tx->input_count) {
            CHECK(tx->inputs[i].sequence == 0 && tx->inputs[i].previous_index == 0);
            for (size_t j = 0; j < 32; ++j) CHECK(tx->inputs[i].previous_txid[j] == 0);
        }
    }
    for (size_t i = tx->output_count; i < ZCL_TX_OUTPUT_MAX; ++i) {
        CHECK(tx->outputs[i].value == 0 && tx->outputs[i].script_len == 0);
        for (size_t j = 0; j < ZCL_TX_OUTPUT_SCRIPT_MAX; ++j) CHECK(tx->outputs[i].script[j] == 0);
    }
}

static void exact_fixture_and_owned_result(void)
{
    for (int network = 0; network < 2; ++network) {
        reset((zcl_network)network);
        zcl_transparent_tx tx;
        CHECK(zcl_transaction_draft(&request, &tx) == ZCL_OK);
        unsigned_rows(&tx);
        uint8_t actual[ZCL_TX_WIRE_MAX], expected[ZCL_TX_WIRE_MAX];
        size_t actual_len = 0, expected_len = 0;
        CHECK(zcl_transaction_serialize(&tx, actual, sizeof(actual), &actual_len) == ZCL_OK);
        CHECK(zcl_transaction_serialize(&fixture.spending, expected, sizeof(expected), &expected_len) == ZCL_OK);
        CHECK(actual_len == 177 && actual_len == expected_len);
        CHECK(memcmp(actual, expected, actual_len) == 0);
        uint8_t id[32];
        static const uint8_t expected_id[32] = {
            0x60,0x2c,0x67,0x3d,0xb0,0x50,0x3b,0x48,0xa4,0x00,0x41,0x43,0x47,0x00,0x9a,0xe9,
            0x68,0xb1,0xaa,0xba,0x9b,0x10,0x66,0x3b,0xde,0x50,0x8e,0x7f,0x82,0x18,0xdc,0x46
        }; /* Independently qualified OpenSSL SHA256d of the public draft fixture. */
        CHECK(zcl_transaction_id(&tx, id, sizeof(id)) == ZCL_OK);
        CHECK(memcmp(id, expected_id, sizeof(id)) == 0);
        memset(&request, 0, sizeof(request));
        memset(&fixture, 0, sizeof(fixture));
        CHECK(zcl_transaction_serialize(&tx, actual, sizeof(actual), &actual_len) == ZCL_OK);
        CHECK(actual_len == expected_len && memcmp(actual, expected, actual_len) == 0);
    }
}

static void argument_and_policy_bounds(void)
{
    reset(ZCL_MAINNET);
    refused(NULL, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_draft(&request, NULL) == ZCL_INVALID_ARGUMENT);
    request.network = (zcl_network)2;
    refused(&request, ZCL_UNSUPPORTED);
    reset(ZCL_MAINNET);
    const size_t invalid[] = {0, 17, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        request.input_count = invalid[i];
        refused(&request, invalid[i] == 0 ? ZCL_INVALID_ENCODING : ZCL_RESOURCE_EXHAUSTED);
        request.input_count = 2;
        request.output_count = invalid[i];
        refused(&request, invalid[i] == 0 ? ZCL_INVALID_ENCODING : ZCL_RESOURCE_EXHAUSTED);
        request.output_count = 2;
    }
    request.input_count = 9;
    refused(&request, ZCL_RESOURCE_EXHAUSTED);
    request.input_count = 2;
    request.expiry_height = ZCL_TX_EXPIRY_LIMIT;
    refused(&request, ZCL_OUT_OF_RANGE);
    request.expiry_height = 0;
    request.maximum_fee = UINT64_MAX;
    refused(&request, ZCL_OUT_OF_RANGE);
    request.maximum_fee = 499;
    refused(&request, ZCL_OUT_OF_RANGE);
    request.maximum_fee = 500;
    request.outputs[1].value = 2001;
    refused(&request, ZCL_OUT_OF_RANGE);
    request.outputs[1].value = UINT64_MAX;
    refused(&request, ZCL_OUT_OF_RANGE);
    request.outputs[1].value = ZCL_MAX_MONEY;
    refused(&request, ZCL_OUT_OF_RANGE);
}

static void malformed_sources_and_destinations(void)
{
    reset(ZCL_MAINNET);
    for (size_t row = 0; row < 2; ++row) {
        const size_t size = request.inputs[row].previous.length;
        for (size_t length = 0; length < size; ++length) {
            request.inputs[row].previous.length = length;
            refused(&request, ZCL_OK);
        }
        request.inputs[row].previous.length = size;
        request.inputs[row].previous.wire = NULL;
        refused(&request, ZCL_INVALID_ARGUMENT);
        request.inputs[row].previous = fixture.sources[row];
        request.inputs[row].previous.length = SIZE_MAX;
        refused(&request, ZCL_RESOURCE_EXHAUSTED);
        request.inputs[row].previous = fixture.sources[row];
        const uint32_t index = request.inputs[row].output_index;
        request.inputs[row].output_index = 2;
        refused(&request, ZCL_OUT_OF_RANGE);
        request.inputs[row].output_index = UINT32_MAX;
        refused(&request, ZCL_OUT_OF_RANGE);
        request.inputs[row].output_index = index;
    }
    request.outputs[1].destination.network = ZCL_TESTNET;
    refused(&request, ZCL_UNSUPPORTED);
    request.outputs[1].destination.network = ZCL_MAINNET;
    request.outputs[1].destination.kind = (zcl_address_kind)0;
    refused(&request, ZCL_UNSUPPORTED);
    reset(ZCL_MAINNET);
    fixture.previous[1].outputs[1].script_len = 1;
    fixture.previous[1].outputs[1].script[0] = 0x51;
    rebind(1);
    refused(&request, ZCL_UNSUPPORTED);
}

static void shared_sources_and_fee_boundaries(void)
{
    reset(ZCL_MAINNET);
    request.inputs[1] = request.inputs[0];
    refused(&request, ZCL_INVALID_ENCODING);
    request.inputs[1].output_index = 1;
    request.outputs[1].value = 5500;
    zcl_transparent_tx tx;
    CHECK(zcl_transaction_draft(&request, &tx) == ZCL_OK);
    CHECK(tx.inputs[0].previous_index == 0 && tx.inputs[1].previous_index == 1);
    CHECK(memcmp(tx.inputs[0].previous_txid, tx.inputs[1].previous_txid, 32) == 0);
    request.outputs[1].value = 6000;
    request.maximum_fee = 0;
    CHECK(zcl_transaction_draft(&request, &tx) == ZCL_OK);
    reset(ZCL_MAINNET);
    fixture.previous[0].outputs[0].value = ZCL_MAX_MONEY;
    fixture.previous[0].outputs[1].value = 0;
    fixture.previous[1].outputs[1].value = 1;
    rebind(0);
    rebind(1);
    refused(&request, ZCL_OUT_OF_RANGE); /* Individually valid funding, excessive combined input value. */
}

static void maximum_rows_and_raw_context(void)
{
    reset(ZCL_TESTNET);
    fixture.previous[0].output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.previous[0].outputs[i] = fixture.previous[0].outputs[0];
        fixture.previous[0].outputs[i].value = 100;
    }
    rebind(0);
    request.input_count = ZCL_TX_INPUT_MAX;
    request.output_count = ZCL_TX_OUTPUT_MAX;
    request.lock_time = UINT32_MAX;
    request.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    request.maximum_fee = 680; /* Eight100 inputs minus sum(0..15). */
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        request.inputs[i].previous = fixture.sources[0];
        request.inputs[i].output_index = (uint32_t)(7 - i);
        request.inputs[i].sequence = UINT32_C(0x80000000) + (uint32_t)i;
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        request.outputs[i] = request.outputs[0];
        request.outputs[i].value = i;
        request.outputs[i].destination.kind = i % 2 == 0 ? ZCL_P2PKH : ZCL_P2SH;
        memset(request.outputs[i].destination.hash, (int)i, 20);
    }
    zcl_transparent_tx tx;
    memset(&tx, 0xa5, sizeof(tx));
    CHECK(zcl_transaction_draft(&request, &tx) == ZCL_OK);
    CHECK(tx.input_count == 8 && tx.output_count == 16);
    CHECK(tx.lock_time == UINT32_MAX && tx.expiry_height == ZCL_TX_EXPIRY_LIMIT - 1);
    unsigned_rows(&tx);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(tx.inputs[i].previous_index == 7 - i);
        CHECK(tx.inputs[i].sequence == UINT32_C(0x80000000) + i);
    }
    for (size_t i = 0; i < 16; ++i) {
        zcl_address destination;
        CHECK(tx.outputs[i].value == i);
        CHECK(zcl_address_from_script(tx.outputs[i].script, tx.outputs[i].script_len, ZCL_TESTNET, &destination) == ZCL_OK);
        CHECK(destination.kind == request.outputs[i].destination.kind);
        CHECK(memcmp(destination.hash, request.outputs[i].destination.hash, 20) == 0);
    }
    request.maximum_fee = 679;
    refused(&request, ZCL_OUT_OF_RANGE);
    reset(ZCL_MAINNET);
    CHECK(zcl_transaction_draft(&request, &tx) == ZCL_OK);
    unsigned_rows(&tx); /* A smaller replacement retains no maximum-sized rows. */
}

int main(void)
{
    exact_fixture_and_owned_result();
    argument_and_policy_bounds();
    malformed_sources_and_destinations();
    shared_sources_and_fee_boundaries();
    maximum_rows_and_raw_context();
    puts("Unsigned draft checks passed");
    return 0;
}
