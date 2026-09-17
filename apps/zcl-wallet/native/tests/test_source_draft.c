/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_draft_internal.h"
#include "transaction_source_internal.h"
#include "draft_fixture.h"
#include "source_assessment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Source draft at %d\n", __LINE__); abort(); } } while (0)
static source_assessment_fixture fixture;
static zcl_draft_request request, original;
static struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } output, saved;
static uint8_t expected_wire[ZCL_TX_WIRE_MAX], actual_wire[ZCL_TX_WIRE_MAX];

static void prepare(unsigned tail, zcl_network network)
{
    CHECK(draft_fixture_init(&request, &fixture.base, network));
    for (size_t i = 0; i < 2; ++i) {
        CHECK(source_assessment_extend(&fixture, i, tail));
        request.inputs[i].previous = fixture.base.sources[i];
    }
    request.lock_time = fixture.base.spending.lock_time = UINT32_MAX;
    request.expiry_height = fixture.base.spending.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    request.inputs[0].sequence = fixture.base.spending.inputs[0].sequence = UINT32_C(0x80000000);
}

static void construct(zcl_status expected)
{
    memset(&output, 0xa5, sizeof(output)); memcpy(&saved, &output, sizeof(output));
    memcpy(&original, &request, sizeof(request));
    CHECK(zcl_transaction_draft_full_sources(&request, &output.tx) == expected);
    CHECK(memcmp(&request, &original, sizeof(request)) == 0);
    CHECK(output.before == saved.before && output.after == saved.after);
    if (expected != ZCL_OK) CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
}

static void exact_profiles(void)
{
    for (unsigned network = 0; network < 2; ++network) {
        for (unsigned tail = 0; tail < 8; ++tail) {
            prepare(tail, (zcl_network)network);
            if (tail != 0) {
                memset(&output, 0xa5, sizeof(output)); memcpy(&saved, &output, sizeof(output));
                CHECK(zcl_transaction_draft(&request, &output.tx) != ZCL_OK);
                CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
            }
            construct(ZCL_OK);
            CHECK(memcmp(&output.tx, &fixture.base.spending, sizeof(output.tx)) == 0);
            size_t expected = 0, actual = 0;
            CHECK(zcl_transaction_serialize(&fixture.base.spending, expected_wire, sizeof(expected_wire), &expected) == ZCL_OK);
            memset(&fixture, 0xcc, sizeof(fixture)); memset(&request, 0xcc, sizeof(request));
            CHECK(zcl_transaction_serialize(&output.tx, actual_wire, sizeof(actual_wire), &actual) == ZCL_OK);
            CHECK(expected == actual && memcmp(expected_wire, actual_wire, actual) == 0);
        }
    }
}

static void selections(void)
{
    prepare(7, ZCL_MAINNET);
    request.inputs[1].previous = request.inputs[0].previous;
    request.maximum_fee = 4500;
    construct(ZCL_OK);
    CHECK(memcmp(output.tx.inputs[0].previous_txid, output.tx.inputs[1].previous_txid, 32) == 0);
    CHECK(output.tx.inputs[0].previous_index == 0 && output.tx.inputs[1].previous_index == 1);
    request.inputs[1].output_index = 0;
    construct(ZCL_INVALID_ENCODING); /* Duplicate outpoint, not merely shared source bytes. */
    prepare(7, ZCL_TESTNET);
    CHECK(assessment_fixture_rebind(&fixture.base, 1));
    request.inputs[1].previous = fixture.base.sources[1];
    construct(ZCL_OK); /* Mixed full and narrow sources. */
    prepare(7, ZCL_MAINNET);
    construct(ZCL_OK);
    uint8_t identity[32]; memcpy(identity, output.tx.inputs[0].previous_txid, sizeof(identity));
    fixture.wire[0][request.inputs[0].previous.length - 1] ^= 1;
    construct(ZCL_OK); /* Supplied opaque bytes select a different, still unverified outpoint. */
    CHECK(memcmp(identity, output.tx.inputs[0].previous_txid, sizeof(identity)) != 0);
}

static void refusals(void)
{
    prepare(7, ZCL_MAINNET);
    for (size_t i = 0; i < 2; ++i) {
        const size_t length = request.inputs[i].previous.length;
        for (size_t cut = 0; cut < length; ++cut) {
            request.inputs[i].previous.length = cut;
            construct(ZCL_INVALID_ENCODING);
        }
        request.inputs[i].previous.length = length;
    }
    request.inputs[1].output_index = UINT32_MAX;
    construct(ZCL_OUT_OF_RANGE);
    prepare(7, ZCL_MAINNET); request.maximum_fee = 499;
    construct(ZCL_OUT_OF_RANGE);
    prepare(7, ZCL_MAINNET); request.outputs[0].destination.network = ZCL_TESTNET;
    construct(ZCL_UNSUPPORTED);
    prepare(7, ZCL_MAINNET); request.outputs[1].value = 2001;
    construct(ZCL_OUT_OF_RANGE);
}

int main(void)
{
    exact_profiles(); selections(); refusals();
    puts("Full-source draft preserves exact selections, fee bounds, owned output and legacy admission");
    return 0;
}
