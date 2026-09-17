/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_draft_internal.h"
#include "transaction_source_internal.h"
#include "zcl_keys.h"
#include <string.h>

zcl_status zcl_draft_bind_funding(const zcl_draft_funding *funding, zcl_tx_input *input)
{
    if (funding == NULL || input == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_transparent_tx previous = {0};
    zcl_status status = zcl_transaction_parse(funding->previous.wire, funding->previous.length, &previous);
    if (status == ZCL_OK && funding->output_index >= previous.output_count) status = ZCL_OUT_OF_RANGE;
    if (status == ZCL_OK)
        status = zcl_transaction_id(&previous, input->previous_txid, sizeof(input->previous_txid));
    zcl_secure_zero(&previous, sizeof(previous));
    if (status == ZCL_OK) {
        input->previous_index = funding->output_index;
        input->sequence = funding->sequence;
    }
    return status;
}

zcl_status zcl_draft_bind_full_funding(const zcl_draft_funding *funding, zcl_tx_input *input)
{
    if (funding == NULL || input == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_v4_source source = {0};
    const zcl_status status = zcl_v4_source_inspect(funding->previous.wire,
        funding->previous.length, funding->output_index, &source);
    if (status == ZCL_OK) {
        memcpy(input->previous_txid, source.transaction_id, sizeof(input->previous_txid));
        input->previous_index = funding->output_index;
        input->sequence = funding->sequence;
    }
    zcl_secure_zero(&source, sizeof(source));
    return status;
}

static zcl_status assess(const zcl_draft_request *request, const zcl_transparent_tx *transaction,
    bool full_sources)
{
    if (request == NULL || transaction == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->input_count > ZCL_TX_INPUT_MAX) return ZCL_RESOURCE_EXHAUSTED;
    zcl_previous_transaction previous[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < request->input_count; ++i) previous[i] = request->inputs[i].previous;
    zcl_transaction_assessment assessment = {0};
    const zcl_status status = full_sources
        ? zcl_v4_source_assess(transaction, request->network, previous, request->input_count, request->maximum_fee, &assessment)
        : zcl_transaction_assess(transaction, request->network, previous, request->input_count, request->maximum_fee, &assessment);
    zcl_secure_zero(&assessment, sizeof(assessment));
    zcl_secure_zero(previous, sizeof(previous));
    return status;
}

zcl_status zcl_draft_assess(const zcl_draft_request *request, const zcl_transparent_tx *transaction)
{
    return assess(request, transaction, false);
}

zcl_status zcl_draft_assess_full_sources(const zcl_draft_request *request, const zcl_transparent_tx *transaction)
{
    return assess(request, transaction, true);
}
