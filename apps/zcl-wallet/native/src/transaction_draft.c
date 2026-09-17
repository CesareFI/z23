/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_draft_internal.h"
#include "zcl_keys.h"

static zcl_status draft_limits(const zcl_draft_request *request)
{
    if (request->input_count == 0 || request->output_count == 0) return ZCL_INVALID_ENCODING;
    if (request->input_count > ZCL_TX_INPUT_MAX || request->output_count > ZCL_TX_OUTPUT_MAX)
        return ZCL_RESOURCE_EXHAUSTED;
    if (request->expiry_height >= ZCL_TX_EXPIRY_LIMIT || request->maximum_fee > ZCL_MAX_MONEY)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status draft_arguments(const zcl_draft_request *request, const zcl_transparent_tx *output)
{
    if (request == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->network != ZCL_MAINNET && request->network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    return draft_limits(request);
}

static zcl_status draft_inputs(const zcl_draft_request *request, zcl_transparent_tx *candidate,
    bool full_sources)
{
    for (size_t i = 0; i < request->input_count; ++i) {
        const zcl_status status = full_sources
            ? zcl_draft_bind_full_funding(&request->inputs[i], &candidate->inputs[i])
            : zcl_draft_bind_funding(&request->inputs[i], &candidate->inputs[i]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status draft_outputs(const zcl_draft_request *request, zcl_transparent_tx *candidate)
{
    for (size_t i = 0; i < request->output_count; ++i) {
        const zcl_draft_output *source = &request->outputs[i];
        zcl_tx_output *output = &candidate->outputs[i];
        if (source->destination.network != request->network) return ZCL_UNSUPPORTED;
        const zcl_status status = zcl_address_script(&source->destination, output->script,
            sizeof(output->script), &output->script_len);
        if (status != ZCL_OK) return status;
        output->value = source->value;
    }
    return ZCL_OK;
}

static zcl_status draft(const zcl_draft_request *request, zcl_transparent_tx *transaction,
    bool full_sources)
{
    zcl_status status = draft_arguments(request, transaction);
    if (status != ZCL_OK) return status;
    zcl_transparent_tx candidate = {0};
    candidate.lock_time = request->lock_time;
    candidate.expiry_height = request->expiry_height;
    candidate.input_count = request->input_count;
    candidate.output_count = request->output_count;
    status = draft_inputs(request, &candidate, full_sources);
    if (status == ZCL_OK) status = draft_outputs(request, &candidate);
    if (status == ZCL_OK) status = full_sources
        ? zcl_draft_assess_full_sources(request, &candidate) : zcl_draft_assess(request, &candidate);
    if (status == ZCL_OK) *transaction = candidate;
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}

zcl_status zcl_transaction_draft(const zcl_draft_request *request, zcl_transparent_tx *transaction)
{
    return draft(request, transaction, false);
}

zcl_status zcl_transaction_draft_full_sources(const zcl_draft_request *request, zcl_transparent_tx *transaction)
{
    return draft(request, transaction, true);
}
