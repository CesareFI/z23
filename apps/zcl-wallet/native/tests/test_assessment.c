/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Assessment check failed at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;

static void refused(const zcl_transparent_tx *tx, zcl_network network,
                     const zcl_previous_transaction *previous, size_t count,
                     uint64_t ceiling, zcl_status expected)
{
    struct { uint64_t before; zcl_transaction_assessment report; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_transaction_assessment old;
    memcpy(&old, &box.report, sizeof(old));
    const zcl_status status = zcl_transaction_assess(tx, network, previous, count, ceiling, &box.report);
    CHECK(status != ZCL_OK && (expected == ZCL_OK || status == expected));
    CHECK(memcmp(&old, &box.report, sizeof(old)) == 0);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
}

static void default_refusal(zcl_status expected)
{
    refused(&fixture.spending, ZCL_MAINNET, fixture.sources, fixture.spending.input_count, 500, expected);
}

static void check_destinations(const zcl_transaction_assessment *report, zcl_network network)
{
    CHECK(report->inputs[0].value == 10000 && report->inputs[1].value == 1000);
    CHECK(report->outputs[0].value == 9000 && report->outputs[1].value == 1500);
    CHECK(report->inputs[0].destination.kind == ZCL_P2PKH && report->inputs[1].destination.kind == ZCL_P2SH);
    CHECK(report->outputs[0].destination.kind == ZCL_P2PKH && report->outputs[1].destination.kind == ZCL_P2SH);
    for (size_t i = 0; i < 20; ++i) {
        CHECK(report->inputs[0].destination.hash[i] == 0x11 && report->inputs[1].destination.hash[i] == 0x44);
        CHECK(report->outputs[0].destination.hash[i] == 0x55 && report->outputs[1].destination.hash[i] == 0x66);
    }
    for (size_t i = 0; i < 2; ++i) {
        CHECK(report->inputs[i].destination.network == network);
        CHECK(report->outputs[i].destination.network == network);
    }
    for (size_t i = 2; i < ZCL_TX_INPUT_MAX; ++i) CHECK(report->inputs[i].value == 0);
    for (size_t i = 2; i < ZCL_TX_OUTPUT_MAX; ++i) CHECK(report->outputs[i].value == 0);
}

static void exact_accounting(void)
{
    for (int network = 0; network < 2; ++network) {
        CHECK(assessment_fixture_init(&fixture));
        struct { uint64_t before; zcl_transaction_assessment report; uint64_t after; } box;
        memset(&box, 0xa5, sizeof(box));
        zcl_transaction_assessment *report = &box.report;
        CHECK(zcl_transaction_assess(&fixture.spending, (zcl_network)network,
            fixture.sources, 2, 500, report) == ZCL_OK);
        CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
        CHECK(report->network == (zcl_network)network && report->maximum_fee == 500);
        CHECK(report->input_count == 2 && report->output_count == 2);
        CHECK(report->input_total == 11000 && report->output_total == 10500 && report->fee == 500);
        check_destinations(report, (zcl_network)network);
        uint8_t wire[ZCL_TX_WIRE_MAX], id[32];
        size_t length = 0;
        CHECK(zcl_transaction_serialize(&fixture.spending, wire, sizeof(wire), &length) == ZCL_OK);
        CHECK(report->serialized_size == length);
        CHECK(zcl_transaction_id(&fixture.spending, id, sizeof(id)) == ZCL_OK);
        CHECK(memcmp(report->transaction_id, id, sizeof(id)) == 0);
        fixture.spending.outputs[0].value = 8000;
        fixture.spending.inputs[0].script[0] = 0x51;
        fixture.spending.inputs[0].script_len = 1;
        CHECK(zcl_transaction_id(&fixture.spending, id, sizeof(id)) == ZCL_OK);
        CHECK(memcmp(report->transaction_id, id, sizeof(id)) != 0);
        CHECK(report->outputs[0].value == 9000 && report->fee == 500);
    }
}

static void fees_and_funding(void)
{
    CHECK(assessment_fixture_init(&fixture));
    refused(&fixture.spending, ZCL_MAINNET, fixture.sources, 2, 499, ZCL_OUT_OF_RANGE);
    refused(&fixture.spending, ZCL_MAINNET, fixture.sources, 2, ZCL_MAX_MONEY + 1, ZCL_OUT_OF_RANGE);
    fixture.spending.outputs[1].value = 2001;
    default_refusal(ZCL_OUT_OF_RANGE); /* Output total11001, input total11000. */
    CHECK(assessment_fixture_init(&fixture));
    fixture.previous[0].outputs[0].value = ZCL_MAX_MONEY;
    fixture.previous[0].outputs[1].value = 0;
    fixture.previous[1].outputs[1].value = 1;
    CHECK(assessment_fixture_rebind(&fixture, 0) && assessment_fixture_rebind(&fixture, 1));
    default_refusal(ZCL_OUT_OF_RANGE); /* Each previous transaction is valid; combined inputs overflow money. */
    CHECK(assessment_fixture_init(&fixture));
    fixture.spending.inputs[1] = fixture.spending.inputs[0];
    default_refusal(ZCL_INVALID_ENCODING);
    /* Distinct outputs from the same previous transaction remain valid. */
    fixture.spending.inputs[1].previous_index = 1;
    fixture.sources[1] = fixture.sources[0];
    fixture.spending.outputs[1].value = 5500;
    zcl_transaction_assessment report;
    CHECK(zcl_transaction_assess(&fixture.spending, ZCL_MAINNET, fixture.sources, 2, 500, &report) == ZCL_OK);
    CHECK(report.input_total == 15000 && report.output_total == 14500 && report.fee == 500);
}

static void malformed_sources_and_destinations(void)
{
    CHECK(assessment_fixture_init(&fixture));
    for (size_t previous = 0; previous < 2; ++previous) {
        const size_t length = fixture.sources[previous].length;
        for (size_t cut = 0; cut < length; ++cut) {
            fixture.sources[previous].length = cut;
            default_refusal(ZCL_OK);
        }
        fixture.sources[previous].length = length;
        for (size_t i = 0; i < length; ++i) {
            fixture.wire[previous][i] ^= 1;
            default_refusal(ZCL_OK);
            fixture.wire[previous][i] ^= 1;
        }
    }
    zcl_previous_transaction saved = fixture.sources[0];
    fixture.sources[0] = fixture.sources[1]; fixture.sources[1] = saved;
    default_refusal(ZCL_INVALID_ENCODING);
    CHECK(assessment_fixture_init(&fixture));
    fixture.previous[1].outputs[1].script[0] ^= 1;
    CHECK(assessment_fixture_rebind(&fixture, 1));
    default_refusal(ZCL_UNSUPPORTED); /* Valid matching bytes, unsupported input template. */
    CHECK(assessment_fixture_init(&fixture));
    fixture.spending.outputs[1].script_len = 0;
    default_refusal(ZCL_UNSUPPORTED); /* No missing/hidden destination row. */
    CHECK(assessment_fixture_init(&fixture));
    fixture.spending.inputs[1].previous_index = 2;
    default_refusal(ZCL_OUT_OF_RANGE);
}

static void maximum_counts_and_money(void)
{
    CHECK(assessment_fixture_init(&fixture));
    fixture.previous[0].output_count = ZCL_TX_INPUT_MAX;
    const zcl_tx_output destination = fixture.previous[0].outputs[0];
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture.previous[0].outputs[i] = destination;
        fixture.previous[0].outputs[i].value = i == 0 ? ZCL_MAX_MONEY : 0;
    }
    CHECK(assessment_fixture_rebind(&fixture, 0));
    fixture.spending.input_count = ZCL_TX_INPUT_MAX;
    fixture.spending.output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture.spending.inputs[i] = fixture.spending.inputs[0];
        fixture.spending.inputs[i].previous_index = (uint32_t)i;
        fixture.sources[i] = fixture.sources[0];
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.spending.outputs[i] = destination;
        fixture.spending.outputs[i].value = i == 0 ? ZCL_MAX_MONEY : 0;
    }
    zcl_transaction_assessment report;
    CHECK(zcl_transaction_assess(&fixture.spending, ZCL_MAINNET, fixture.sources, 8, 0, &report) == ZCL_OK);
    CHECK(report.input_total == ZCL_MAX_MONEY && report.output_total == ZCL_MAX_MONEY && report.fee == 0);
    CHECK(report.input_count == 8 && report.output_count == 16 && report.maximum_fee == 0);
    fixture.spending.outputs[0].value = 0;
    CHECK(zcl_transaction_assess(&fixture.spending, ZCL_MAINNET, fixture.sources, 8, ZCL_MAX_MONEY, &report) == ZCL_OK);
    CHECK(report.output_total == 0 && report.fee == ZCL_MAX_MONEY);
    refused(&fixture.spending, ZCL_MAINNET, fixture.sources, 8, ZCL_MAX_MONEY - 1, ZCL_OUT_OF_RANGE);
}

static void arguments(void)
{
    CHECK(assessment_fixture_init(&fixture));
    refused(NULL, ZCL_MAINNET, fixture.sources, 2, 500, ZCL_INVALID_ARGUMENT);
    refused(&fixture.spending, (zcl_network)2, fixture.sources, 2, 500, ZCL_UNSUPPORTED);
    refused(&fixture.spending, ZCL_MAINNET, NULL, 2, 500, ZCL_INVALID_ARGUMENT);
    const size_t counts[] = {0, 1, 3, SIZE_MAX};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i)
        refused(&fixture.spending, ZCL_MAINNET, fixture.sources, counts[i], 500, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_assess(&fixture.spending, ZCL_MAINNET, fixture.sources, 2, 500, NULL) == ZCL_INVALID_ARGUMENT);
    fixture.sources[1].wire = NULL;
    default_refusal(ZCL_INVALID_ARGUMENT);
    fixture.sources[1].wire = fixture.wire[1]; fixture.sources[1].length = SIZE_MAX;
    default_refusal(ZCL_RESOURCE_EXHAUSTED);
}

int main(void)
{
    exact_accounting(); fees_and_funding(); malformed_sources_and_destinations();
    maximum_counts_and_money(); arguments();
    puts("Bounded transaction assessment checks passed");
    return 0;
}
