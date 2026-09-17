/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include "source_assessment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "source_vectors.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Source assessment at %d\n", __LINE__); abort(); } } while (0)
static source_assessment_fixture fixture;

static void assess(zcl_network network, uint64_t ceiling, zcl_status expected)
{
    struct { uint64_t before; zcl_transaction_assessment value; uint64_t after; } box, original;
    memset(&box, 0xa5, sizeof(box)); memcpy(&original, &box, sizeof(box));
    CHECK(zcl_v4_source_assess(&fixture.base.spending, network, fixture.base.sources,
        fixture.base.spending.input_count, ceiling, &box.value) == expected);
    CHECK(box.before == original.before && box.after == original.after);
    if (expected != ZCL_OK) { CHECK(memcmp(&box, &original, sizeof(box)) == 0); return; }
    CHECK(box.value.network == network && box.value.input_total == 11000);
    CHECK(box.value.output_total == 10500 && box.value.fee == 500 && box.value.maximum_fee == ceiling);
    CHECK(box.value.inputs[0].destination.kind == ZCL_P2PKH && box.value.inputs[1].destination.kind == ZCL_P2SH);
    CHECK(box.value.inputs[0].destination.hash[0] == 0x11 && box.value.inputs[1].destination.hash[19] == 0x44);
}

static void profiles(void)
{
    for (unsigned tail = 0; tail < 8; ++tail) {
        CHECK(source_assessment_init(&fixture, tail));
        assess(ZCL_MAINNET, 500, ZCL_OK); assess(ZCL_TESTNET, 500, ZCL_OK);
        assess(ZCL_MAINNET, 499, ZCL_OUT_OF_RANGE);
        zcl_transaction_assessment old;
        const zcl_status status = zcl_transaction_assess(&fixture.base.spending, ZCL_MAINNET,
            fixture.base.sources, 2, 500, &old);
        CHECK(tail == 0 ? status == ZCL_OK : status != ZCL_OK);
    }
    CHECK(source_assessment_init(&fixture, 7));
    CHECK(assessment_fixture_rebind(&fixture.base, 1)); /* Mixed source profiles. */
    assess(ZCL_MAINNET, 500, ZCL_OK);
}

static void source_binding(void)
{
    CHECK(source_assessment_init(&fixture, 7));
    for (size_t index = 0; index < 2; ++index) {
        const size_t length = fixture.base.sources[index].length;
        /* Opaque signature bytes still participate in exact full identity. */
        fixture.wire[index][length - 1] ^= 1;
        assess(ZCL_MAINNET, 500, ZCL_INVALID_ENCODING);
        fixture.wire[index][length - 1] ^= 1;
        fixture.base.sources[index].length = length - 1;
        assess(ZCL_MAINNET, 500, ZCL_INVALID_ENCODING);
        fixture.base.sources[index].length = length;
        fixture.base.spending.inputs[index].previous_txid[7] ^= 1;
        assess(ZCL_MAINNET, 500, ZCL_INVALID_ENCODING);
        fixture.base.spending.inputs[index].previous_txid[7] ^= 1;
    }
    fixture.base.spending.inputs[1].previous_index = 2;
    assess(ZCL_MAINNET, 500, ZCL_OUT_OF_RANGE);
    CHECK(source_assessment_init(&fixture, 7));
    fixture.base.previous[1].outputs[1].script[0] ^= 1;
    CHECK(source_assessment_extend(&fixture, 1, 7));
    assess(ZCL_MAINNET, 500, ZCL_UNSUPPORTED);
    CHECK(source_assessment_init(&fixture, 7));
    fixture.base.spending.outputs[1].value = 2001;
    assess(ZCL_MAINNET, 500, ZCL_OUT_OF_RANGE);
}

static void reference_matching(void)
{
    for (size_t n = 0; n < sizeof(source_vectors) / sizeof(source_vectors[0]); ++n) {
        zcl_tx_input input = {0};
        memcpy(input.previous_txid, source_vectors[n].id, 32);
        for (uint32_t i = 0; i < source_vectors[n].outputs; ++i) {
            input.previous_index = i;
            zcl_v4_source view;
            CHECK(zcl_v4_source_inspect(source_vectors[n].wire, source_vectors[n].length, i, &view) == ZCL_OK);
            zcl_tx_output output, before;
            CHECK(zcl_v4_source_prevout(&input, source_vectors[n].wire, source_vectors[n].length, &output) == ZCL_OK);
            CHECK(memcmp(&output, &view.output, sizeof(output)) == 0);
            memcpy(&before, &output, sizeof(before)); input.previous_txid[31] ^= 1;
            CHECK(zcl_v4_source_prevout(&input, source_vectors[n].wire, source_vectors[n].length, &output) == ZCL_INVALID_ENCODING);
            CHECK(memcmp(&output, &before, sizeof(output)) == 0);
            input.previous_txid[31] ^= 1;
        }
    }
}

int main(void)
{
    profiles(); source_binding(); reference_matching();
    puts("Full v4 funding identity, offline fee assessment and legacy isolation passed");
    return 0;
}
