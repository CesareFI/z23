/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction.h"
#include "zcl_keys.h"
#include <string.h>

zcl_status zcl_transaction_prevout(const zcl_tx_input *input,
                                   const uint8_t *previous_wire, size_t previous_length,
                                   zcl_tx_output *output)
{
    if (input == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_transparent_tx previous = {0};
    uint8_t id[32] = {0};
    zcl_status status = zcl_transaction_parse(previous_wire, previous_length, &previous);
    if (status != ZCL_OK) goto cleanup;
    status = zcl_transaction_id(&previous, id, sizeof(id));
    if (status != ZCL_OK) goto cleanup;
    if (memcmp(id, input->previous_txid, sizeof(id)) != 0) {
        status = ZCL_INVALID_ENCODING;
        goto cleanup;
    }
    if (input->previous_index >= previous.output_count) {
        status = ZCL_OUT_OF_RANGE;
        goto cleanup;
    }
    *output = previous.outputs[input->previous_index];
cleanup:
    zcl_secure_zero(&previous, sizeof(previous));
    zcl_secure_zero(id, sizeof(id));
    return status;
}
