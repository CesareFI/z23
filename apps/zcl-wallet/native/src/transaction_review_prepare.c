/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <string.h>

static void review_context(const zcl_transparent_tx *transaction, zcl_review_context *context)
{
    context->lock_time = transaction->lock_time;
    context->expiry_height = transaction->expiry_height;
    for (size_t i = 0; i < transaction->input_count; ++i) {
        const zcl_tx_input *input = &transaction->inputs[i];
        memcpy(context->inputs[i].previous_txid, input->previous_txid, sizeof(input->previous_txid));
        context->inputs[i].previous_index = input->previous_index;
        context->inputs[i].sequence = input->sequence;
    }
}

static bool unsigned_inputs(const zcl_transparent_tx *tx)
{
    for (size_t i = 0; i < tx->input_count; ++i) {
        if (tx->inputs[i].script_len != 0) return false;
    }
    return true;
}

zcl_status zcl_review_prepare(const uint8_t *wire, size_t length, zcl_network network,
                              const zcl_previous_transaction *previous, size_t previous_count,
                              uint64_t maximum_fee, zcl_review_data *candidate)
{
    if (candidate == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_transparent_tx transaction = {0};
    zcl_status status = zcl_transaction_parse(wire, length, &transaction);
    if (status == ZCL_OK && !unsigned_inputs(&transaction)) status = ZCL_UNSUPPORTED;
    if (status == ZCL_OK)
        status = zcl_transaction_assess(&transaction, network, previous, previous_count,
            maximum_fee, &candidate->assessment);
    /* Serialize the owned parsed value, never re-read a borrowed wire span. */
    if (status == ZCL_OK)
        status = zcl_transaction_serialize(&transaction, candidate->wire, sizeof(candidate->wire),
            &candidate->wire_length);
    if (status == ZCL_OK) review_context(&transaction, &candidate->context);
    zcl_secure_zero(&transaction, sizeof(transaction));
    return status;
}
