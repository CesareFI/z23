/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction_assess.h"
#include "transaction_internal.h"
#include "transaction_source_internal.h"
#include "zcl_keys.h"

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
    zcl_address address = {0};
    zcl_status status = zcl_address_from_script(output->script, output->script_len, network, &address);
    uint64_t sum = 0;
    if (status == ZCL_OK) status = zcl_amount_add(*total, output->value, &sum);
    if (status == ZCL_OK) {
        item->destination = address;
        item->value = output->value;
        *total = sum;
    }
    zcl_secure_zero(&address, sizeof(address));
    return status;
}

static zcl_status assess_inputs(const zcl_transparent_tx *tx,
                                const zcl_previous_transaction *previous,
                                zcl_transaction_assessment *candidate, bool full_sources)
{
    for (size_t i = 0; i < tx->input_count; ++i) {
        zcl_tx_output output = {0};
        zcl_status status = full_sources
            ? zcl_v4_source_prevout(&tx->inputs[i], previous[i].wire, previous[i].length, &output)
            : zcl_transaction_prevout(&tx->inputs[i], previous[i].wire, previous[i].length, &output);
        if (status == ZCL_OK)
            status = assess_output(&output, candidate->network, &candidate->inputs[i], &candidate->input_total);
        zcl_secure_zero(&output, sizeof(output));
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

static zcl_status assess(const zcl_transparent_tx *transaction,
                                  zcl_network network,
                                  const zcl_previous_transaction *previous,
                                  size_t previous_count, uint64_t maximum_fee,
                                  zcl_transaction_assessment *assessment, bool full_sources)
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
    status = assess_inputs(transaction, previous, &candidate, full_sources);
    if (status != ZCL_OK) goto cleanup;
    status = assess_outputs(transaction, &candidate);
    if (status != ZCL_OK) goto cleanup;
    status = zcl_amount_subtract(candidate.input_total, candidate.output_total, &candidate.fee);
    if (status != ZCL_OK) goto cleanup;
    if (candidate.fee > maximum_fee) {
        status = ZCL_OUT_OF_RANGE;
        goto cleanup;
    }
    status = zcl_transaction_id(transaction, candidate.transaction_id, sizeof(candidate.transaction_id));
    if (status != ZCL_OK) goto cleanup;
    *assessment = candidate;
cleanup:
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}

zcl_status zcl_transaction_assess(const zcl_transparent_tx *transaction,
    zcl_network network, const zcl_previous_transaction *previous,
    size_t previous_count, uint64_t maximum_fee, zcl_transaction_assessment *assessment)
{
    return assess(transaction, network, previous, previous_count, maximum_fee, assessment, false);
}

zcl_status zcl_v4_source_assess(const zcl_transparent_tx *transaction,
    zcl_network network, const zcl_previous_transaction *previous,
    size_t previous_count, uint64_t maximum_fee, zcl_transaction_assessment *assessment)
{
    return assess(transaction, network, previous, previous_count, maximum_fee, assessment, true);
}
