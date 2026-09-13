/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction_assess.h"
#include "transaction_internal.h"

static zcl_status assessment_arguments(const zcl_transparent_tx *tx, zcl_network network,
                                        const zcl_previous_transaction *previous,
                                        size_t previous_count, uint64_t maximum_fee,
                                        const zcl_transaction_assessment *output, size_t *size)
{
    if (previous == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    if (maximum_fee > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    const zcl_status status = zcl_transaction_check(tx, size);
    if (status != ZCL_OK) return status;
    return previous_count == tx->input_count ? ZCL_OK : ZCL_INVALID_ARGUMENT;
}

static zcl_status assess_output(const zcl_tx_output *output, zcl_network network,
                                zcl_assessed_output *item, uint64_t *total)
{
    zcl_address address;
    zcl_status status = zcl_address_from_script(output->script, output->script_len, network, &address);
    if (status != ZCL_OK) return status;
    uint64_t sum = 0;
    status = zcl_amount_add(*total, output->value, &sum);
    if (status != ZCL_OK) return status;
    item->destination = address;
    item->value = output->value;
    *total = sum;
    return ZCL_OK;
}

static zcl_status assess_inputs(const zcl_transparent_tx *tx,
                                const zcl_previous_transaction *previous,
                                zcl_transaction_assessment *candidate)
{
    for (size_t i = 0; i < tx->input_count; ++i) {
        zcl_tx_output output;
        zcl_status status = zcl_transaction_prevout(&tx->inputs[i], previous[i].wire,
            previous[i].length, &output);
        if (status != ZCL_OK) return status;
        status = assess_output(&output, candidate->network, &candidate->inputs[i], &candidate->input_total);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status assess_outputs(const zcl_transparent_tx *tx, zcl_transaction_assessment *candidate)
{
    for (size_t i = 0; i < tx->output_count; ++i) {
        const zcl_status status = assess_output(&tx->outputs[i], candidate->network,
            &candidate->outputs[i], &candidate->output_total);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

zcl_status zcl_transaction_assess(const zcl_transparent_tx *transaction,
                                  zcl_network network,
                                  const zcl_previous_transaction *previous,
                                  size_t previous_count, uint64_t maximum_fee,
                                  zcl_transaction_assessment *assessment)
{
    size_t size = 0;
    zcl_status status = assessment_arguments(transaction, network, previous,
        previous_count, maximum_fee, assessment, &size);
    if (status != ZCL_OK) return status;
    zcl_transaction_assessment candidate = {0};
    candidate.network = network;
    candidate.serialized_size = size;
    candidate.input_count = transaction->input_count;
    candidate.output_count = transaction->output_count;
    candidate.maximum_fee = maximum_fee;
    status = assess_inputs(transaction, previous, &candidate);
    if (status != ZCL_OK) return status;
    status = assess_outputs(transaction, &candidate);
    if (status != ZCL_OK) return status;
    status = zcl_amount_subtract(candidate.input_total, candidate.output_total, &candidate.fee);
    if (status != ZCL_OK) return status;
    if (candidate.fee > maximum_fee) return ZCL_OUT_OF_RANGE;
    status = zcl_transaction_id(transaction, candidate.transaction_id, sizeof(candidate.transaction_id));
    if (status != ZCL_OK) return status;
    *assessment = candidate;
    return ZCL_OK;
}
