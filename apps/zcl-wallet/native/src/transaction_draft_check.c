/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_draft_internal.h"

zcl_status zcl_draft_bind_funding(const zcl_draft_funding *funding, zcl_tx_input *input)
{
    if (funding == NULL || input == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_transparent_tx previous;
    zcl_status status = zcl_transaction_parse(funding->previous.wire, funding->previous.length, &previous);
    if (status != ZCL_OK) return status;
    if (funding->output_index >= previous.output_count) return ZCL_OUT_OF_RANGE;
    status = zcl_transaction_id(&previous, input->previous_txid, sizeof(input->previous_txid));
    if (status != ZCL_OK) return status;
    input->previous_index = funding->output_index;
    input->sequence = funding->sequence;
    return ZCL_OK;
}

zcl_status zcl_draft_assess(const zcl_draft_request *request, const zcl_transparent_tx *transaction)
{
    if (request == NULL || transaction == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->input_count > ZCL_TX_INPUT_MAX) return ZCL_RESOURCE_EXHAUSTED;
    zcl_previous_transaction previous[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < request->input_count; ++i) previous[i] = request->inputs[i].previous;
    zcl_transaction_assessment assessment;
    return zcl_transaction_assess(transaction, request->network, previous,
        request->input_count, request->maximum_fee, &assessment);
}
