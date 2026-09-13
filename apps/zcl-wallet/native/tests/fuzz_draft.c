/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "draft_fixture.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
/* Public test-only baselines, immutable after initialization. Each iteration
 * owns its bounded request and output; production adds no global or heap. */
static assessment_fixture baseline;
static assessment_fixture wide;
static zcl_draft_request standard;
static bool initialized;

static uint8_t byte_at(const uint8_t *data, size_t size, size_t offset)
{
    return offset < size ? data[offset] : 0;
}

static uint64_t word_at(const uint8_t *data, size_t size, size_t offset)
{
    if (offset > size || size - offset < 8) return 0;
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) value |= (uint64_t)data[offset + i] << (8 * i);
    return value;
}

static void initialize(void)
{
    if (!draft_fixture_init(&standard, &baseline, ZCL_MAINNET) || !assessment_fixture_init(&wide)) abort();
    wide.previous[0].output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        wide.previous[0].outputs[i] = wide.previous[0].outputs[0];
        wide.previous[0].outputs[i].value = 1000;
    }
    if (!assessment_fixture_rebind(&wide, 0)) abort();
    initialized = true;
}

static void check_roundtrip(const zcl_transparent_tx *tx)
{
    uint8_t wire[ZCL_TX_WIRE_MAX], again[ZCL_TX_WIRE_MAX];
    size_t length = 0, repeated = 0;
    zcl_transparent_tx parsed;
    if (zcl_transaction_serialize(tx, wire, sizeof(wire), &length) != ZCL_OK) abort();
    if (zcl_transaction_parse(wire, length, &parsed) != ZCL_OK) abort();
    if (zcl_transaction_serialize(&parsed, again, sizeof(again), &repeated) != ZCL_OK) abort();
    if (length != repeated || memcmp(wire, again, length) != 0) abort();
}

static void check_assessment(const zcl_draft_request *request, const zcl_transparent_tx *tx)
{
    zcl_previous_transaction previous[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < request->input_count; ++i) previous[i] = request->inputs[i].previous;
    zcl_transaction_assessment report;
    if (zcl_transaction_assess(tx, request->network, previous, request->input_count,
        request->maximum_fee, &report) != ZCL_OK) abort();
    if (report.input_total > ZCL_MAX_MONEY || report.output_total > report.input_total) abort();
    if (report.fee != report.input_total - report.output_total || report.fee > request->maximum_fee) abort();
    for (size_t i = 0; i < request->output_count; ++i) {
        const zcl_draft_output *expected = &request->outputs[i];
        const zcl_assessed_output *actual = &report.outputs[i];
        if (actual->value != expected->value || actual->destination.network != expected->destination.network) abort();
        if (actual->destination.kind != expected->destination.kind ||
            memcmp(actual->destination.hash, expected->destination.hash, 20) != 0) abort();
    }
}

static void check_fields(const zcl_draft_request *request, const zcl_transparent_tx *tx)
{
    if (tx->input_count != request->input_count || tx->output_count != request->output_count) abort();
    if (tx->input_count == 0 || tx->input_count > ZCL_TX_INPUT_MAX ||
        tx->output_count == 0 || tx->output_count > ZCL_TX_OUTPUT_MAX) abort();
    if (tx->lock_time != request->lock_time || tx->expiry_height != request->expiry_height) abort();
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        if (tx->inputs[i].script_len != 0) abort();
        for (size_t j = 0; j < ZCL_TX_INPUT_SCRIPT_MAX; ++j) if (tx->inputs[i].script[j] != 0) abort();
        if (i < tx->input_count && (tx->inputs[i].previous_index != request->inputs[i].output_index ||
            tx->inputs[i].sequence != request->inputs[i].sequence)) abort();
    }
}

static void exercise(const zcl_draft_request *request)
{
    struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_draft_request original;
    memcpy(&original, request, sizeof(original));
    const zcl_status status = zcl_transaction_draft(request, &box.tx);
    if (memcmp(&original, request, sizeof(original)) != 0) abort();
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (status != ZCL_OK) {
        const uint8_t *bytes = (const uint8_t *)&box.tx;
        for (size_t i = 0; i < sizeof(box.tx); ++i) if (bytes[i] != 0xa5) abort();
        return;
    }
    check_fields(request, &box.tx);
    check_assessment(request, &box.tx);
    check_roundtrip(&box.tx);
}

static void structured(const uint8_t *data, size_t size)
{
    zcl_draft_request request = {0};
    request.network = (zcl_network)(byte_at(data, size, 0) % 3);
    request.input_count = (size_t)(byte_at(data, size, 1) % 10) + 1;
    request.output_count = (size_t)(byte_at(data, size, 2) % 18) + 1;
    request.lock_time = (uint32_t)(word_at(data, size, 8) & UINT32_MAX);
    request.expiry_height = (uint32_t)(word_at(data, size, 16) & UINT32_MAX);
    request.maximum_fee = (byte_at(data, size, 3) & 1) != 0 ? word_at(data, size, 24) : ZCL_MAX_MONEY;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        request.inputs[i].previous = wide.sources[0];
        request.inputs[i].output_index = (byte_at(data, size, 4) & 1) != 0 ? byte_at(data, size, 32 + i) : (uint32_t)i;
        request.inputs[i].sequence = (uint32_t)(word_at(data, size, 40 + 8 * i) & UINT32_MAX);
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        zcl_draft_output *output = &request.outputs[i];
        output->destination.network = request.network;
        output->destination.kind = i % 2 == 0 ? ZCL_P2PKH : ZCL_P2SH;
        memset(output->destination.hash, byte_at(data, size, 104 + i), 20);
        const uint64_t value = word_at(data, size, 120 + 8 * i);
        output->value = (byte_at(data, size, 5) & 1) != 0 ? value : value % 1001;
    }
    exercise(&request);
    request.outputs[0].destination.kind = (zcl_address_kind)(byte_at(data, size, 6) % 4);
    request.outputs[0].destination.network = (zcl_network)(byte_at(data, size, 7) % 3);
    exercise(&request);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_TX_WIRE_MAX + 1) return 0;
    if (!initialized) initialize();
    zcl_draft_request request = standard;
    request.inputs[0].previous.wire = data;
    request.inputs[0].previous.length = size;
    exercise(&request);
    structured(data, size);
    return 0;
}
