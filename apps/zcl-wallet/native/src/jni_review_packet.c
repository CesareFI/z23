/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_review_internal.h"

_Static_assert(ZCL_REVIEW_PACKET_MAX <= INT32_MAX, "Review packets fit jsize");
_Static_assert(ZCL_MAX_MONEY <= INT64_MAX, "Checked money fits jlong");

/* Internal callers supply exactly5 or8 words from20 or32-byte hashes. */
static void hash_words(const uint8_t *hash, size_t count, jlong *values)
{
    for (size_t word = 0; word < count; ++word) {
        uint32_t value = 0;
        for (size_t byte = 0; byte < 4; ++byte)
            value = (value << 8) | hash[word * 4 + byte];
        values[word] = (jlong)value;
    }
}

static bool header_bounds(const zcl_review_snapshot *snapshot)
{
    const zcl_transaction_assessment *report = &snapshot->assessment;
    if (report->network != ZCL_MAINNET && report->network != ZCL_TESTNET) return false;
    if (report->input_count == 0 || report->input_count > ZCL_TX_INPUT_MAX) return false;
    if (report->output_count == 0 || report->output_count > ZCL_TX_OUTPUT_MAX) return false;
    if (snapshot->remaining_ms == 0 || snapshot->remaining_ms > ZCL_REVIEW_LIFETIME_MS) return false;
    return report->serialized_size <= ZCL_TX_WIRE_MAX;
}

static bool money_bounds(const zcl_transaction_assessment *report)
{
    if (report->input_total > ZCL_MAX_MONEY || report->output_total > report->input_total) return false;
    if (report->maximum_fee > ZCL_MAX_MONEY || report->fee > report->maximum_fee) return false;
    return report->fee == report->input_total - report->output_total;
}

static void header_values(const zcl_review_snapshot *snapshot, jlong *values)
{
    const zcl_transaction_assessment *report = &snapshot->assessment;
    values[0] = (jlong)ZCL_OK;
    values[1] = (jlong)snapshot->remaining_ms;
    values[2] = (jlong)report->network;
    values[3] = (jlong)snapshot->context.lock_time;
    values[4] = (jlong)snapshot->context.expiry_height;
    values[5] = (jlong)report->serialized_size;
    values[6] = (jlong)report->input_count;
    values[7] = (jlong)report->output_count;
    values[8] = (jlong)report->input_total;
    values[9] = (jlong)report->output_total;
    values[10] = (jlong)report->fee;
    values[11] = (jlong)report->maximum_fee;
    hash_words(report->transaction_id, 8, &values[12]);
}

static zcl_status destination_values(const zcl_assessed_output *row, zcl_network network, jlong *values)
{
    if (row->value > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    if (row->destination.network != network) return ZCL_UNSUPPORTED;
    if (row->destination.kind != ZCL_P2PKH && row->destination.kind != ZCL_P2SH) return ZCL_UNSUPPORTED;
    values[0] = (jlong)row->value;
    values[1] = (jlong)row->destination.kind;
    hash_words(row->destination.hash, 5, &values[2]);
    return ZCL_OK;
}

static zcl_status input_values(const zcl_review_snapshot *snapshot, jlong *values)
{
    for (size_t i = 0; i < snapshot->assessment.input_count; ++i) {
        const zcl_review_input *input = &snapshot->context.inputs[i];
        jlong *row = &values[i * 17];
        hash_words(input->previous_txid, 8, row);
        row[8] = (jlong)input->previous_index;
        row[9] = (jlong)input->sequence;
        const zcl_status status = destination_values(&snapshot->assessment.inputs[i],
            snapshot->assessment.network, &row[10]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status output_values(const zcl_transaction_assessment *report, jlong *values)
{
    for (size_t i = 0; i < report->output_count; ++i) {
        const zcl_status status = destination_values(&report->outputs[i], report->network, &values[i * 7]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

zcl_status zcl_jni_review_values(const zcl_review_snapshot *snapshot,
    jlong values[ZCL_REVIEW_PACKET_MAX], size_t *length)
{
    if (snapshot == NULL || values == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    if (!header_bounds(snapshot) || !money_bounds(&snapshot->assessment)) return ZCL_OUT_OF_RANGE;
    header_values(snapshot, values);
    zcl_status status = input_values(snapshot, &values[20]);
    if (status != ZCL_OK) return status;
    const size_t outputs = 20 + 17 * snapshot->assessment.input_count;
    status = output_values(&snapshot->assessment, &values[outputs]);
    if (status == ZCL_OK) *length = outputs + 7 * snapshot->assessment.output_count;
    return status;
}
