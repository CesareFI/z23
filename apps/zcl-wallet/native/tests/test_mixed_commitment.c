/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "commitment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "legacy_source_vectors.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Mixed commitment at %d\n", __LINE__); abort(); } } while (0)
static commitment_fixture fixture, before;
static struct { uint64_t before; zcl_source_commitment value; uint64_t after; } result, unchanged;

static void run(zcl_status expected)
{
    memset(&result, 0xa5, sizeof(result)); memcpy(&unchanged, &result, sizeof(result));
    memcpy(&before, &fixture, sizeof(fixture));
    CHECK(zcl_source_commitment_check(&fixture.request, &result.value) == expected);
    CHECK(memcmp(&fixture, &before, sizeof(fixture)) == 0);
    CHECK(result.before == unchanged.before && result.after == unchanged.after);
    if (expected != ZCL_OK) CHECK(memcmp(&result, &unchanged, sizeof(result)) == 0);
    else {
        CHECK(memcmp(result.value.source.transaction_id, fixture.request.transaction_id, 32) == 0);
        CHECK(memcmp(result.value.header.hash, fixture.request.header_id, 32) == 0);
    }
}

static void prepare(size_t row, uint32_t width)
{
    CHECK(commitment_fixture_init(&fixture, 7, width, width - 1));
    memcpy(fixture.funding.wire[0], legacy_vectors[row].wire, legacy_vectors[row].length);
    fixture.request.source = fixture.funding.wire[0]; fixture.request.source_length = legacy_vectors[row].length;
    CHECK(commitment_fixture_bind(&fixture));
    CHECK(memcmp(fixture.request.transaction_id, legacy_vectors[row].id, 32) == 0);
}

static void profiles(void)
{
    const uint32_t widths[] = {1,3,UINT32_MAX};
    for (size_t row = 0; row < sizeof(legacy_vectors) / sizeof(legacy_vectors[0]); ++row) {
        for (size_t width = 0; width < sizeof(widths) / sizeof(widths[0]); ++width) {
            prepare(row, widths[width]);
            for (uint32_t index = 0; index < legacy_vectors[row].outputs; ++index) {
                fixture.request.output_index = index; run(ZCL_OK);
                CHECK(result.value.source.joinsplit_count == legacy_vectors[row].joins);
            }
            CHECK(zcl_v4_source_commitment_check(&fixture.request, &result.value) == ZCL_UNSUPPORTED);
            fixture.request.transaction_id[0] ^= 1; run(ZCL_INVALID_ENCODING);
            fixture.request.transaction_id[0] ^= 1;
            fixture.request.output_index = UINT32_MAX; run(ZCL_OUT_OF_RANGE);
        }
    }
}

static void refusals(void)
{
    prepare(1, 3);
    fixture.request.header_id[0] ^= 1; run(ZCL_INVALID_ENCODING); fixture.request.header_id[0] ^= 1;
    fixture.branch.siblings[1][0] ^= 1; run(ZCL_INVALID_ENCODING); fixture.branch.siblings[1][0] ^= 1;
    --fixture.request.source_length; run(ZCL_INVALID_ENCODING); ++fixture.request.source_length;
    fixture.funding.wire[0][legacy_vectors[1].join_offset + 304] ^= 4;
    CHECK(commitment_fixture_bind(&fixture)); run(ZCL_INVALID_ENCODING);
}

static void ambiguous_length(void)
{
    prepare(0, 1);
    /* Historical64-byte envelope accepted structurally: one input with empty
     * script, one output with four script bytes and locktime. No validity claim. */
    uint8_t *wire = fixture.funding.wire[0]; memset(wire, 0, 64);
    wire[0] = 1; wire[4] = 1; wire[46] = 1; wire[55] = 4;
    fixture.request.source_length = 64;
    CHECK(commitment_fixture_bind(&fixture));
    zcl_source_view view;
    CHECK(zcl_source_inspect(wire, 64, 0, &view) == ZCL_OK);
    run(ZCL_UNSUPPORTED);
}

int main(void)
{
    profiles(); refusals(); ambiguous_length();
    puts("Historical commitment profiles, exact identities and64-byte refusal passed"); return 0;
}
