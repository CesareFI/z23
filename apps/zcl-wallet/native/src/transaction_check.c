/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_internal.h"
#include <string.h>

_Static_assert(ZCL_TX_WIRE_MAX == 29 + ZCL_TX_INPUT_MAX * (41 + ZCL_TX_INPUT_SCRIPT_MAX)
    + ZCL_TX_OUTPUT_MAX * (9 + ZCL_TX_OUTPUT_SCRIPT_MAX), "wire capacity drift");
_Static_assert(ZCL_TX_INPUT_SCRIPT_MAX < 253 && ZCL_TX_OUTPUT_SCRIPT_MAX < 253,
    "bounded scripts require single-byte CompactSize");
_Static_assert(ZCL_TX_INPUT_MAX < 253 && ZCL_TX_OUTPUT_MAX < 253,
    "bounded vectors require single-byte CompactSize");

static bool null_outpoint(const zcl_tx_input *input)
{
    static const uint8_t zero[32] = {0};
    return input->previous_index == UINT32_MAX
        && memcmp(input->previous_txid, zero, sizeof(zero)) == 0;
}

static bool duplicate_input(const zcl_transparent_tx *tx, size_t index)
{
    const zcl_tx_input *input = &tx->inputs[index];
    for (size_t i = 0; i < index; ++i) {
        if (tx->inputs[i].previous_index == input->previous_index
            && memcmp(tx->inputs[i].previous_txid, input->previous_txid, 32) == 0)
            return true;
    }
    return false;
}

static zcl_status check_inputs(const zcl_transparent_tx *tx, size_t *length)
{
    for (size_t i = 0; i < tx->input_count; ++i) {
        const zcl_tx_input *input = &tx->inputs[i];
        if (input->script_len > ZCL_TX_INPUT_SCRIPT_MAX) return ZCL_RESOURCE_EXHAUSTED;
        if (null_outpoint(input) || duplicate_input(tx, i)) return ZCL_INVALID_ENCODING;
        *length += 41 + input->script_len; /* <=8 bounded entries; no wrap. */
    }
    return ZCL_OK;
}

static zcl_status check_outputs(const zcl_transparent_tx *tx, size_t *length)
{
    uint64_t total = 0;
    for (size_t i = 0; i < tx->output_count; ++i) {
        const zcl_tx_output *output = &tx->outputs[i];
        if (output->script_len > ZCL_TX_OUTPUT_SCRIPT_MAX) return ZCL_RESOURCE_EXHAUSTED;
        if (output->value > ZCL_MAX_MONEY - total) return ZCL_OUT_OF_RANGE;
        total += output->value;
        *length += 9 + output->script_len; /* <=16 bounded entries. */
    }
    return ZCL_OK;
}

zcl_status zcl_transaction_check(const zcl_transparent_tx *tx, size_t *wire_size)
{
    if (tx == NULL || wire_size == NULL) return ZCL_INVALID_ARGUMENT;
    if (tx->input_count == 0 || tx->output_count == 0) return ZCL_INVALID_ENCODING;
    if (tx->input_count > ZCL_TX_INPUT_MAX || tx->output_count > ZCL_TX_OUTPUT_MAX)
        return ZCL_RESOURCE_EXHAUSTED;
    if (tx->expiry_height >= ZCL_TX_EXPIRY_LIMIT) return ZCL_OUT_OF_RANGE;
    size_t length = 29; /* Header/group, two counts, lock/expiry, zero shielded tail. */
    zcl_status status = check_inputs(tx, &length);
    if (status != ZCL_OK) return status;
    status = check_outputs(tx, &length);
    if (status != ZCL_OK) return status;
    *wire_size = length;
    return ZCL_OK;
}
