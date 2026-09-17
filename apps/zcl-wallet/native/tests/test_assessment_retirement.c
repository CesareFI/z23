/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_transaction_prevout
#undef zcl_address_from_script
#undef zcl_transaction_id
#undef zcl_secure_zero
#include "assessment_fixture.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Assessment retirement at %d: %s\n", __LINE__, #v); abort(); } } while (0)
static assessment_fixture fixture;
static uintptr_t output_id, address_id, report_id;
static unsigned outputs, output_clears, addresses, address_clears, ids, report_clears;
static unsigned fail_output, fail_address;
static bool fail_id;

zcl_status zcl_assess_test_prevout(const zcl_tx_input *input, const uint8_t *wire, size_t length, zcl_tx_output *output);
zcl_status zcl_assess_test_address(const uint8_t *script, size_t length, zcl_network network, zcl_address *output);
zcl_status zcl_assess_test_id(const zcl_transparent_tx *tx, uint8_t *id, size_t capacity);
void zcl_assess_test_zero(void *buffer, size_t length);

static void filled(const void *buffer, size_t length, uint8_t value)
{
    CHECK(buffer != NULL);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

zcl_status zcl_assess_test_prevout(const zcl_tx_input *input, const uint8_t *wire, size_t length, zcl_tx_output *output)
{
    CHECK(output != NULL && output_id == 0 && outputs == output_clears);
    output_id = (uintptr_t)output;
    if (++outputs == fail_output) { memset(output, 0x5a, sizeof(*output)); return ZCL_IO_FAILURE; }
    return zcl_transaction_prevout(input, wire, length, output);
}

zcl_status zcl_assess_test_address(const uint8_t *script, size_t length, zcl_network network, zcl_address *output)
{
    CHECK(output != NULL && address_id == 0 && addresses == address_clears);
    address_id = (uintptr_t)output;
    if (++addresses == fail_address) { memset(output, 0x5a, sizeof(*output)); return ZCL_UNSUPPORTED; }
    return zcl_address_from_script(script, length, network, output);
}

zcl_status zcl_assess_test_id(const zcl_transparent_tx *tx, uint8_t *id, size_t capacity)
{
    CHECK(id != NULL && capacity == 32 && report_id == 0 && ids == 0);
    report_id = (uintptr_t)id;
    ++ids;
    if (fail_id) { memset(id, 0x5a, capacity); return ZCL_CRYPTO_FAILURE; }
    return zcl_transaction_id(tx, id, capacity);
}

void zcl_assess_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    filled(buffer, length, 0);
    const uintptr_t identity = (uintptr_t)buffer;
    if (identity == output_id) {
        CHECK(length == sizeof(zcl_tx_output));
        output_id = 0; ++output_clears;
    } else if (identity == address_id) {
        CHECK(length == sizeof(zcl_address));
        address_id = 0; ++address_clears;
    } else {
        CHECK(length == sizeof(zcl_transaction_assessment));
        if (report_id != 0) CHECK(report_id >= identity &&
            report_id - identity == offsetof(zcl_transaction_assessment, transaction_id));
        report_id = 0; ++report_clears;
    }
}

static void retired(void)
{
    CHECK(output_id == 0 && address_id == 0 && report_id == 0);
    CHECK(outputs == output_clears && addresses == address_clears);
}

static void reset(void)
{
    retired();
    outputs = output_clears = addresses = address_clears = ids = report_clears = 0;
    fail_output = fail_address = 0;
    fail_id = false;
}

static void assess(uint64_t ceiling, zcl_status wanted)
{
    struct { uint64_t before; zcl_transaction_assessment report; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_transaction_assess(&fixture.spending, ZCL_TESTNET, fixture.sources, 2, ceiling, &box.report) == wanted);
    retired();
    CHECK(report_clears == 1);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
    if (wanted != ZCL_OK) { filled(&box, sizeof(box), 0xa5); return; }
    CHECK(ids == 1 && outputs == 2 && addresses == 4);
    CHECK(box.report.network == ZCL_TESTNET && box.report.input_count == 2 && box.report.output_count == 2);
    CHECK(box.report.input_total == 11000 && box.report.output_total == 10500 && box.report.fee == 500);
    CHECK(box.report.maximum_fee == ceiling);
    uint8_t id[32];
    CHECK(zcl_transaction_id(&fixture.spending, id, sizeof(id)) == ZCL_OK);
    CHECK(memcmp(box.report.transaction_id, id, sizeof(id)) == 0);
}

static void provider_failures(void)
{
    CHECK(assessment_fixture_init(&fixture));
    reset(); assess(500, ZCL_OK);
    for (unsigned point = 1; point <= 2; ++point) {
        reset(); fail_output = point;
        assess(500, ZCL_IO_FAILURE);
        CHECK(outputs == point && addresses == point - 1 && ids == 0);
    }
    for (unsigned point = 1; point <= 4; ++point) {
        reset(); fail_address = point;
        assess(500, ZCL_UNSUPPORTED);
        CHECK(addresses == point && outputs == (point <= 2 ? point : 2) && ids == 0);
    }
    reset(); fail_id = true;
    assess(500, ZCL_CRYPTO_FAILURE);
    CHECK(outputs == 2 && addresses == 4 && ids == 1);
}

static void funding_and_fee_refusals(void)
{
    CHECK(assessment_fixture_init(&fixture));
    reset(); assess(499, ZCL_OUT_OF_RANGE);
    CHECK(ids == 0 && outputs == 2 && addresses == 4);
    fixture.spending.outputs[1].value = 2001;
    reset(); assess(500, ZCL_OUT_OF_RANGE);
    CHECK(ids == 0 && outputs == 2 && addresses == 4);
    CHECK(assessment_fixture_init(&fixture));
    fixture.previous[0].outputs[0].value = ZCL_MAX_MONEY;
    fixture.previous[0].outputs[1].value = 0;
    fixture.previous[1].outputs[1].value = 1;
    CHECK(assessment_fixture_rebind(&fixture, 0) && assessment_fixture_rebind(&fixture, 1));
    reset(); assess(500, ZCL_OUT_OF_RANGE);
    CHECK(ids == 0 && outputs == 2 && addresses == 2);
}

int main(void)
{
    provider_failures(); funding_and_fee_refusals();
    retired();
    CHECK(puts("Assessment retirement: dirty providers, bounded totals/fees and complete-only output passed") >= 0);
    return 0;
}
