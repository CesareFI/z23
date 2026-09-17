/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_transaction_parse
#undef zcl_transaction_id
#undef zcl_transaction_assess
#undef zcl_v4_source_assess
#undef zcl_v4_source_inspect
#undef zcl_secure_zero
#include "draft_fixture.h"
#include "transaction_draft_internal.h"
#include "transaction_source_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Draft retirement at %d\n", __LINE__); abort(); } } while (0)
static assessment_fixture fixture;
static zcl_draft_request request, saved_request;
static zcl_transparent_tx output;
static uintptr_t parsed_identity, candidate_identity, assessment_identity, sources_identity;
static unsigned parse_calls, parsed_clears, candidate_clears, assessment_calls, assessment_clears, sources_clears;
static unsigned fail_parse, fail_id;
static bool fail_assessment;
static bool full_sources;

zcl_status zcl_draft_test_parse(const uint8_t *bytes, size_t length, zcl_transparent_tx *transaction);
zcl_status zcl_draft_test_id(const zcl_transparent_tx *transaction, uint8_t *bytes, size_t capacity);
zcl_status zcl_draft_test_assess(const zcl_transparent_tx *transaction, zcl_network network,
    const zcl_previous_transaction *previous, size_t count, uint64_t fee, zcl_transaction_assessment *assessment);
void zcl_draft_test_zero(void *buffer, size_t length);
zcl_status zcl_draft_test_source(const uint8_t *wire, size_t length, uint32_t index, zcl_v4_source *source);

zcl_status zcl_draft_test_source(const uint8_t *wire, size_t length, uint32_t index, zcl_v4_source *source)
{
    CHECK(full_sources && parsed_identity == 0 && parse_calls == parsed_clears);
    parsed_identity = (uintptr_t)source;
    ++parse_calls;
    memset(source, 0xa5, sizeof(*source));
    if (fail_parse == parse_calls) return ZCL_INVALID_ENCODING;
    if (fail_id == parse_calls) return ZCL_CRYPTO_FAILURE;
    return zcl_v4_source_inspect(wire, length, index, source);
}

zcl_status zcl_draft_test_parse(const uint8_t *bytes, size_t length, zcl_transparent_tx *transaction)
{
    CHECK(parsed_identity == 0 && parse_calls == parsed_clears);
    parsed_identity = (uintptr_t)transaction;
    ++parse_calls;
    memset(transaction, 0xa5, sizeof(*transaction));
    if (fail_parse == parse_calls) return ZCL_INVALID_ENCODING;
    return zcl_transaction_parse(bytes, length, transaction);
}

zcl_status zcl_draft_test_id(const zcl_transparent_tx *transaction, uint8_t *bytes, size_t capacity)
{
    CHECK((uintptr_t)transaction == parsed_identity && parsed_clears + 1 == parse_calls);
    if (fail_id == parse_calls) {
        memset(bytes, 0x5a, capacity);
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_transaction_id(transaction, bytes, capacity);
}

zcl_status zcl_draft_test_assess(const zcl_transparent_tx *transaction, zcl_network network,
    const zcl_previous_transaction *previous, size_t count, uint64_t fee, zcl_transaction_assessment *assessment)
{
    CHECK(parsed_identity == 0 && parse_calls == parsed_clears);
    CHECK(assessment_identity == 0 && sources_identity == 0);
    candidate_identity = (uintptr_t)transaction;
    assessment_identity = (uintptr_t)assessment;
    sources_identity = (uintptr_t)previous;
    ++assessment_calls;
    memset(assessment, 0x6a, sizeof(*assessment));
    if (fail_assessment) return ZCL_CRYPTO_FAILURE;
    return zcl_transaction_assess(transaction, network, previous, count, fee, assessment);
}

void zcl_draft_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && buffer != &output && buffer != &request);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if ((uintptr_t)buffer == parsed_identity) {
        CHECK(length == (full_sources ? sizeof(zcl_v4_source) : sizeof(zcl_transparent_tx)));
        parsed_identity = 0;
        ++parsed_clears;
    } else if ((uintptr_t)buffer == assessment_identity) {
        CHECK(length == sizeof(zcl_transaction_assessment));
        assessment_identity = 0;
        ++assessment_clears;
    } else if ((uintptr_t)buffer == sources_identity) {
        CHECK(length == ZCL_TX_INPUT_MAX * sizeof(zcl_previous_transaction));
        sources_identity = 0;
        ++sources_clears;
    } else {
        CHECK(length == sizeof(zcl_transparent_tx));
        CHECK(candidate_identity == 0 || candidate_identity == (uintptr_t)buffer);
        CHECK(parsed_identity == 0 && assessment_identity == 0 && sources_identity == 0);
        candidate_identity = 0;
        ++candidate_clears;
    }
}

static void reset(void)
{
    CHECK(parsed_identity == 0 && candidate_identity == 0 && assessment_identity == 0 && sources_identity == 0);
    CHECK(draft_fixture_init(&request, &fixture, ZCL_MAINNET));
    parse_calls = parsed_clears = candidate_clears = 0;
    assessment_calls = assessment_clears = sources_clears = 0;
    fail_parse = fail_id = 0;
    fail_assessment = false;
    memset(&output, 0xa5, sizeof(output));
}

static void check_result(zcl_status expected, unsigned candidates)
{
    memcpy(&saved_request, &request, sizeof(request));
    const zcl_status status = full_sources
        ? zcl_transaction_draft_full_sources(&request, &output) : zcl_transaction_draft(&request, &output);
    CHECK(status == expected);
    CHECK(memcmp(&request, &saved_request, sizeof(request)) == 0);
    CHECK(parsed_identity == 0 && candidate_identity == 0 && assessment_identity == 0 && sources_identity == 0);
    CHECK(parse_calls == parsed_clears && candidate_clears == candidates);
    CHECK(assessment_calls == assessment_clears && assessment_calls == sources_clears);
    if (expected == ZCL_OK) {
        CHECK(output.input_count == request.input_count && output.output_count == request.output_count);
        CHECK(output.outputs[0].value == request.outputs[0].value);
    } else {
        const uint8_t *bytes = (const uint8_t *)&output;
        for (size_t i = 0; i < sizeof(output); ++i) CHECK(bytes[i] == 0xa5);
    }
}

static void failures(void)
{
    for (unsigned row = 1; row <= 2; ++row) {
        reset(); fail_parse = row;
        check_result(ZCL_INVALID_ENCODING, 1);
        CHECK(parse_calls == row && assessment_calls == 0);
        reset(); fail_id = row;
        check_result(ZCL_CRYPTO_FAILURE, 1);
        CHECK(parse_calls == row && assessment_calls == 0);
        reset(); request.inputs[row - 1].output_index = UINT32_MAX;
        check_result(ZCL_OUT_OF_RANGE, 1);
        CHECK(parse_calls == row && assessment_calls == 0);
    }
    reset(); fail_assessment = true;
    check_result(ZCL_CRYPTO_FAILURE, 1);
    CHECK(parse_calls == 2 && assessment_calls == 1);
    reset(); request.outputs[1].destination.network = ZCL_TESTNET;
    check_result(ZCL_UNSUPPORTED, 1);
    CHECK(parse_calls == 2 && assessment_calls == 0);
    reset(); request.outputs[1].destination.kind = (zcl_address_kind)0;
    check_result(ZCL_UNSUPPORTED, 1);
    reset(); request.maximum_fee = 499;
    check_result(ZCL_OUT_OF_RANGE, 1);
    CHECK(assessment_calls == 1);
}

static void admission(void)
{
    reset();
    CHECK(zcl_transaction_draft(NULL, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_draft(&request, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_draft_bind_funding(NULL, &output.inputs[0]) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_draft_bind_funding(&request.inputs[0], NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_draft_assess(NULL, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_draft_assess(&request, NULL) == ZCL_INVALID_ARGUMENT);
    request.input_count = ZCL_TX_INPUT_MAX + 1;
    CHECK(zcl_draft_assess(&request, &output) == ZCL_RESOURCE_EXHAUSTED);
    check_result(ZCL_RESOURCE_EXHAUSTED, 0);
    CHECK(parse_calls == 0 && assessment_calls == 0);
    reset(); request.output_count = 0;
    check_result(ZCL_INVALID_ENCODING, 0);
    reset(); request.network = (zcl_network)2;
    check_result(ZCL_UNSUPPORTED, 0);
}

int main(void)
{
    reset();
    check_result(ZCL_OK, 1);
    CHECK(parse_calls == 2 && assessment_calls == 1);
    failures();
    admission();
    full_sources = true;
    reset(); check_result(ZCL_OK, 1); failures(); admission();
    puts("Draft scratch retirement checks passed");
    return 0;
}
