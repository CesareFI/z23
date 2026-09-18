/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include "mixed_source_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "legacy_source_vectors.h"
#include "source_vectors.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Mixed source at %d\n", __LINE__); abort(); } } while (0)
static source_assessment_fixture fixture, before;
static struct { uint64_t before; zcl_transaction_assessment value; uint64_t after; } result, unchanged;

static void assess(zcl_network network, uint64_t ceiling, zcl_status expected)
{
    memset(&result, 0xa5, sizeof(result)); memcpy(&unchanged, &result, sizeof(result));
    memcpy(&before, &fixture, sizeof(fixture));
    CHECK(zcl_source_assess(&fixture.base.spending, network, fixture.base.sources,
        fixture.base.spending.input_count, ceiling, &result.value) == expected);
    CHECK(memcmp(&fixture, &before, sizeof(fixture)) == 0);
    CHECK(result.before == unchanged.before && result.after == unchanged.after);
    if (expected != ZCL_OK) CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    else CHECK(result.value.input_total == 11000 && result.value.output_total == 10500 && result.value.fee == 500);
}

static void profiles(void)
{
    for (unsigned first = 0; first <= 12; ++first) {
        for (unsigned second = 0; second <= 12; ++second) {
            CHECK(mixed_source_init(&fixture, first, second));
            assess(ZCL_MAINNET, 500, ZCL_OK); assess(ZCL_TESTNET, 500, ZCL_OK);
            CHECK(result.value.inputs[0].destination.hash[0] == 0x11 && result.value.inputs[1].destination.hash[19] == 0x44);
            CHECK(result.value.inputs[0].destination.kind == ZCL_P2PKH && result.value.inputs[1].destination.kind == ZCL_P2SH);
            assess(ZCL_MAINNET, 499, ZCL_OUT_OF_RANGE);
            if (first < 5 || second < 5) {
                CHECK(zcl_v4_source_assess(&fixture.base.spending, ZCL_MAINNET, fixture.base.sources, 2, 500, &result.value) == ZCL_UNSUPPORTED);
                CHECK(zcl_transaction_assess(&fixture.base.spending, ZCL_MAINNET, fixture.base.sources, 2, 500, &result.value) != ZCL_OK);
            }
        }
    }
}

static void immutable(void)
{
    CHECK(mixed_source_init(&fixture, 2, 4)); assess(ZCL_MAINNET, 500, ZCL_OK);
    zcl_transaction_assessment saved; memcpy(&saved, &result.value, sizeof(saved));
    memset(&fixture, 0, sizeof(fixture)); CHECK(memcmp(&saved, &result.value, sizeof(saved)) == 0);
}

static void refusals(void)
{
    CHECK(mixed_source_init(&fixture, 2, 12));
    fixture.base.spending.inputs[1].previous_txid[0] ^= 1; assess(ZCL_MAINNET, 500, ZCL_INVALID_ENCODING);
    fixture.base.spending.inputs[1].previous_txid[0] ^= 1;
    fixture.base.spending.inputs[1].previous_index = 2; assess(ZCL_MAINNET, 500, ZCL_OUT_OF_RANGE);
    fixture.base.spending.inputs[1].previous_index = 1;
    --fixture.base.sources[1].length; assess(ZCL_MAINNET, 500, ZCL_INVALID_ENCODING); ++fixture.base.sources[1].length;
    fixture.base.spending.outputs[1].value = 2001; assess(ZCL_MAINNET, 500, ZCL_OUT_OF_RANGE);
    fixture.base.spending.outputs[1].value = 1500;
    fixture.base.previous[1].outputs[1].script[0] ^= 1;
    CHECK(mixed_source_extend(&fixture, 1, 4)); assess(ZCL_MAINNET, 500, ZCL_UNSUPPORTED);
}

static void reference_wire(const uint8_t *wire, size_t length, const uint8_t expected_id[32])
{
    zcl_source_view view;
    CHECK(zcl_source_inspect(wire, length, 0, &view) == ZCL_OK);
    CHECK(memcmp(view.transaction_id, expected_id, 32) == 0);
    zcl_tx_input input = {0}; memcpy(input.previous_txid, expected_id, 32);
    zcl_tx_output output, unchanged_output;
    CHECK(zcl_source_prevout(&input, wire, length, &output) == ZCL_OK);
    CHECK(output.value == view.output.value && output.script_len == view.output.script_len);
    CHECK(memcmp(output.script, view.output.script, output.script_len) == 0);
    memcpy(&unchanged_output, &output, sizeof(output)); input.previous_txid[0] ^= 1;
    CHECK(zcl_source_prevout(&input, wire, length, &output) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(&output, &unchanged_output, sizeof(output)) == 0);
}

static void references(void)
{
    for (size_t i = 0; i < sizeof(legacy_vectors) / sizeof(legacy_vectors[0]); ++i)
        reference_wire(legacy_vectors[i].wire, legacy_vectors[i].length, legacy_vectors[i].id);
    for (size_t i = 0; i < sizeof(source_vectors) / sizeof(source_vectors[0]); ++i)
        reference_wire(source_vectors[i].wire, source_vectors[i].length, source_vectors[i].id);
}

static void argument_bounds(void)
{
    zcl_source_view view, unchanged_view;
    memset(&view, 0xa5, sizeof(view)); memcpy(&unchanged_view, &view, sizeof(view));
    CHECK(zcl_source_inspect(NULL, 0, 0, &view) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_source_inspect(legacy_vectors[0].wire, SIZE_MAX, 0, &view) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(zcl_source_inspect(legacy_vectors[0].wire, 100001, 0, &view) == ZCL_RESOURCE_EXHAUSTED);
    for (size_t length = 0; length < 4; ++length)
        CHECK(zcl_source_inspect(legacy_vectors[0].wire, length, 0, &view) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(&view, &unchanged_view, sizeof(view)) == 0);
}

static void maximum_inputs(void)
{
    static source_assessment_fixture funding[ZCL_TX_INPUT_MAX];
    static zcl_transparent_tx spending;
    zcl_previous_transaction sources[ZCL_TX_INPUT_MAX] = {{0}};
    memset(&spending, 0, sizeof(spending));
    spending.input_count = ZCL_TX_INPUT_MAX; spending.output_count = 1;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        CHECK(mixed_source_init(&funding[i], (unsigned)i, 5));
        funding[i].base.previous[0].inputs[0].previous_index = (uint32_t)i + 100;
        CHECK(mixed_source_extend(&funding[i], 0, (unsigned)i));
        spending.inputs[i] = funding[i].base.spending.inputs[0];
        sources[i] = funding[i].base.sources[0];
    }
    spending.outputs[0] = funding[0].base.previous[0].outputs[0]; spending.outputs[0].value = 79500;
    CHECK(zcl_source_assess(&spending, ZCL_MAINNET, sources, ZCL_TX_INPUT_MAX, 500, &result.value) == ZCL_OK);
    CHECK(result.value.input_count == 8 && result.value.input_total == 80000 && result.value.fee == 500);
    memcpy(&unchanged, &result, sizeof(result));
    funding[7].wire[0][0] ^= 8;
    CHECK(zcl_source_assess(&spending, ZCL_MAINNET, sources, ZCL_TX_INPUT_MAX, 500, &result.value) == ZCL_UNSUPPORTED);
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    funding[7].wire[0][0] ^= 8; spending.inputs[7] = spending.inputs[0];
    CHECK(zcl_source_assess(&spending, ZCL_MAINNET, sources, ZCL_TX_INPUT_MAX, 500, &result.value) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
}

static void total_bounds(void)
{
    CHECK(mixed_source_init(&fixture, 0, 4));
    fixture.base.previous[0].outputs[0].value = ZCL_MAX_MONEY;
    fixture.base.previous[0].outputs[1].value = 0;
    fixture.base.previous[1].outputs[1].value = 1;
    CHECK(mixed_source_extend(&fixture, 0, 0) && mixed_source_extend(&fixture, 1, 4));
    assess(ZCL_MAINNET, 500, ZCL_OUT_OF_RANGE);
}

int main(void)
{
    references(); profiles(); refusals(); immutable(); argument_bounds(); maximum_inputs(); total_bounds();
    puts("Mixed historical/v4 source matching and169 assessment pairs passed"); return 0;
}
