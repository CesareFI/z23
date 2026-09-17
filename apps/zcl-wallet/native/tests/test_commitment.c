/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "commitment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Source commitment at %d\n", __LINE__); abort(); } } while (0)
static commitment_fixture fixture, before;
static struct { uint64_t before; zcl_source_commitment value; uint64_t after; } output, unchanged;

static void run(zcl_status status)
{
    memset(&output, 0xa5, sizeof(output)); memcpy(&unchanged, &output, sizeof(output));
    memcpy(&before, &fixture, sizeof(fixture));
    CHECK(zcl_v4_source_commitment_check(&fixture.request, &output.value) == status);
    CHECK(memcmp(&fixture, &before, sizeof(fixture)) == 0);
    CHECK(output.before == unchanged.before && output.after == unchanged.after);
    if (status != ZCL_OK) CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
    else {
        CHECK(memcmp(output.value.source.transaction_id, fixture.request.transaction_id, 32) == 0);
        CHECK(memcmp(output.value.header.hash, fixture.request.header_id, 32) == 0);
    }
}

static void profiles(void)
{
    for (unsigned tail = 0; tail < 8; ++tail) {
        CHECK(commitment_fixture_init(&fixture, tail, 3, 2)); run(ZCL_OK);
        CHECK(output.value.source.output.value == 10000);
        fixture.request.output_index = 1; run(ZCL_OK); CHECK(output.value.source.output.value == 5000);
        fixture.request.output_index = 2; run(ZCL_OUT_OF_RANGE);
        fixture.request.output_index = UINT32_MAX; run(ZCL_OUT_OF_RANGE);
    }
    const uint32_t widths[] = {1,2,3,8,65,UINT32_MAX};
    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); ++i) {
        CHECK(commitment_fixture_init(&fixture, 7, widths[i], 0)); run(ZCL_OK);
        CHECK(commitment_fixture_init(&fixture, 7, widths[i], widths[i] - 1)); run(ZCL_OK);
    }
    fixture.request.network = ZCL_TESTNET; fixture.request.height = 6350; run(ZCL_OK);
    zcl_source_commitment saved; memcpy(&saved, &output.value, sizeof(saved));
    memset(&fixture, 0, sizeof(fixture)); CHECK(memcmp(&saved, &output.value, sizeof(saved)) == 0);
}

static void mismatches(void)
{
    CHECK(commitment_fixture_init(&fixture, 7, 3, 2));
    fixture.request.header_id[0] ^= 1; run(ZCL_INVALID_ENCODING); fixture.request.header_id[0] ^= 1;
    fixture.request.transaction_id[31] ^= 1; run(ZCL_INVALID_ENCODING); fixture.request.transaction_id[31] ^= 1;
    fixture.header[36] ^= 1; run(ZCL_INVALID_ENCODING); fixture.header[36] ^= 1;
    fixture.funding.wire[0][60] ^= 1; run(ZCL_INVALID_ENCODING); fixture.funding.wire[0][60] ^= 1;
    fixture.branch.siblings[1][0] ^= 1; run(ZCL_INVALID_ENCODING); fixture.branch.siblings[1][0] ^= 1;
    fixture.branch.transaction_index = 1; run(ZCL_INVALID_ENCODING);
    fixture.branch.transaction_index = 3; run(ZCL_OUT_OF_RANGE);
    fixture.branch.sibling_count = SIZE_MAX; run(ZCL_OUT_OF_RANGE);
    fixture.branch.transaction_index = 2; run(ZCL_RESOURCE_EXHAUSTED);
}

static void arguments(void)
{
    CHECK(commitment_fixture_init(&fixture, 0, 1, 0));
    CHECK(zcl_v4_source_commitment_check(NULL, &output.value) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_v4_source_commitment_check(&fixture.request, NULL) == ZCL_INVALID_ARGUMENT);
    fixture.request.network = (zcl_network)99; run(ZCL_UNSUPPORTED); fixture.request.network = ZCL_MAINNET;
    fixture.request.header_length = SIZE_MAX; run(ZCL_INVALID_ENCODING);
    fixture.request.header_length = sizeof(fixture.header); fixture.request.header = NULL; run(ZCL_INVALID_ARGUMENT);
    fixture.request.header = fixture.header; fixture.request.source = NULL; run(ZCL_INVALID_ARGUMENT);
    fixture.request.source = fixture.funding.wire[0]; fixture.request.source_length = SIZE_MAX; run(ZCL_RESOURCE_EXHAUSTED);
    fixture.request.branch = NULL; run(ZCL_INVALID_ARGUMENT);
}

static void internal_node_length(void)
{
    CHECK(commitment_fixture_init(&fixture, 0, 1, 0));
    /* Structurally valid64-byte envelope: zero inputs, two outputs,17 script
     * bytes. It is deliberately NOT a consensus-valid funding transaction. */
    uint8_t *wire = fixture.funding.wire[0]; memset(wire, 0, 64);
    const uint8_t prefix[] = {4,0,0,128,133,32,47,137,0,2}; memcpy(wire, prefix, sizeof(prefix));
    wire[18] = 17; fixture.request.source_length = 64;
    CHECK(commitment_fixture_bind(&fixture));
    zcl_v4_source inspected;
    CHECK(zcl_v4_source_inspect(wire, 64, 0, &inspected) == ZCL_OK);
    CHECK(zcl_merkle_branch_check(fixture.request.transaction_id, &fixture.branch, fixture.request.transaction_id) == ZCL_OK);
    run(ZCL_UNSUPPORTED);
}

int main(void)
{
    profiles(); mismatches(); arguments(); internal_node_length();
    puts("Source/outpoint/header commitment and ambiguity refusal checks passed"); return 0;
}
