/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
/* Public immutable baseline after initialization; each iteration owns its
 * source descriptors and current transaction. No production global is added. */
static assessment_fixture baseline;
static bool initialized;

static uint64_t row_sum(const zcl_assessed_output *rows, size_t count, zcl_network network)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        if (rows[i].value > ZCL_MAX_MONEY - sum) abort();
        sum += rows[i].value;
        if (rows[i].destination.network != network) abort();
        if (rows[i].destination.kind != ZCL_P2PKH && rows[i].destination.kind != ZCL_P2SH) abort();
    }
    return sum;
}

static void check_counts(const zcl_transaction_assessment *report, const zcl_transparent_tx *tx,
    size_t count)
{
    if (report->input_count != tx->input_count || report->output_count != tx->output_count) abort();
    if (report->input_count > ZCL_TX_INPUT_MAX || report->output_count > ZCL_TX_OUTPUT_MAX) abort();
    if (report->input_count == 0 || report->output_count == 0 || report->input_count != count) abort();
}

static void check_totals(const zcl_transaction_assessment *report, zcl_network network, uint64_t ceiling)
{
    if (report->network != network || report->maximum_fee != ceiling || report->fee > ceiling) abort();
    if (report->input_total != row_sum(report->inputs, report->input_count, network)) abort();
    if (report->output_total != row_sum(report->outputs, report->output_count, network)) abort();
    if (report->output_total > report->input_total || report->fee != report->input_total - report->output_total) abort();
}

static void assess(const zcl_transparent_tx *tx, zcl_network network,
                    const zcl_previous_transaction *previous, size_t count, uint64_t ceiling)
{
    struct { uint64_t before; zcl_transaction_assessment report; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_transaction_assessment old;
    memcpy(&old, &box.report, sizeof(old));
    zcl_transparent_tx original;
    memcpy(&original, tx, sizeof(original));
    const zcl_status status = zcl_transaction_assess(tx, network, previous, count, ceiling, &box.report);
    if (memcmp(&original, tx, sizeof(original)) != 0) abort();
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (status != ZCL_OK) {
        if (memcmp(&old, &box.report, sizeof(old)) != 0) abort();
        return;
    }
    const zcl_transaction_assessment *report = &box.report;
    check_counts(report, tx, count);
    check_totals(report, network, ceiling);
    if (report->serialized_size > ZCL_TX_WIRE_MAX) abort();
    uint8_t id[32];
    if (zcl_transaction_id(tx, id, sizeof(id)) != ZCL_OK || memcmp(id, report->transaction_id, sizeof(id)) != 0) abort();
}

static void dynamic_previous(const uint8_t *data, size_t size)
{
    zcl_transparent_tx previous;
    if (zcl_transaction_parse(data, size, &previous) != ZCL_OK) return;
    uint8_t id[32];
    if (zcl_transaction_id(&previous, id, sizeof(id)) != ZCL_OK) abort();
    zcl_transparent_tx spending = {0};
    spending.input_count = previous.output_count < ZCL_TX_INPUT_MAX ? previous.output_count : ZCL_TX_INPUT_MAX;
    spending.output_count = previous.output_count;
    zcl_previous_transaction sources[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < spending.input_count; ++i) {
        memcpy(spending.inputs[i].previous_txid, id, sizeof(id));
        spending.inputs[i].previous_index = (uint32_t)i;
        spending.inputs[i].sequence = UINT32_MAX;
        sources[i].wire = data;
        sources[i].length = size;
    }
    memcpy(spending.outputs, previous.outputs, sizeof(spending.outputs));
    const zcl_network network = id[0] % 2 == 0 ? ZCL_MAINNET : ZCL_TESTNET;
    assess(&spending, network, sources, spending.input_count, ZCL_MAX_MONEY);
    spending.outputs[0].value /= 2;
    assess(&spending, network, sources, spending.input_count, id[1]);
    /* Invalid IDs/counts/amounts/scripts cannot yield partial review data. */
    spending.inputs[0].previous_txid[id[2] % 32] ^= 1;
    assess(&spending, network, sources, spending.input_count, ZCL_MAX_MONEY);
    spending.inputs[0].previous_txid[id[2] % 32] ^= 1;
    spending.outputs[0].script_len = id[3];
    assess(&spending, network, sources, spending.input_count, ZCL_MAX_MONEY);
    assess(&spending, network, sources, SIZE_MAX, ZCL_MAX_MONEY);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!initialized) {
        if (!assessment_fixture_init(&baseline)) abort();
        initialized = true;
    }
    zcl_previous_transaction sources[2] = {baseline.sources[0], baseline.sources[1]};
    sources[0].wire = data;
    sources[0].length = size;
    assess(&baseline.spending, ZCL_TESTNET, sources, 2, 500);
    dynamic_previous(data, size);
    return 0;
}
