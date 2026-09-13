/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction.h"
#include <string.h>

zcl_status zcl_transaction_prevout(const zcl_tx_input *input,
                                   const uint8_t *previous_wire, size_t previous_length,
                                   zcl_tx_output *output)
{
    if (input == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_transparent_tx previous;
    zcl_status status = zcl_transaction_parse(previous_wire, previous_length, &previous);
    if (status != ZCL_OK) return status;
    uint8_t id[32] = {0};
    status = zcl_transaction_id(&previous, id, sizeof(id));
    if (status != ZCL_OK) return status;
    if (memcmp(id, input->previous_txid, sizeof(id)) != 0) return ZCL_INVALID_ENCODING;
    if (input->previous_index >= previous.output_count) return ZCL_OUT_OF_RANGE;
    *output = previous.outputs[input->previous_index];
    return ZCL_OK;
}
