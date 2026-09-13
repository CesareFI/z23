/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"

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
    zcl_transparent_tx transaction;
    zcl_status status = zcl_transaction_parse(wire, length, &transaction);
    if (status != ZCL_OK) return status;
    if (!unsigned_inputs(&transaction)) return ZCL_UNSUPPORTED;
    status = zcl_transaction_assess(&transaction, network, previous, previous_count,
        maximum_fee, &candidate->assessment);
    if (status != ZCL_OK) return status;
    /* Serialize the owned parsed value, never re-read a borrowed wire span. */
    return zcl_transaction_serialize(&transaction, candidate->wire, sizeof(candidate->wire),
        &candidate->wire_length);
}
