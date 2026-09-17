/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_draft_internal.h"
#include "zcl_keys.h"

zcl_status zcl_jni_draft_wire(const zcl_draft_request *request,
                             uint8_t *wire, size_t capacity, size_t *length)
{
    zcl_transparent_tx transaction = {0};
    zcl_status status = zcl_transaction_draft(request, &transaction);
    if (status == ZCL_OK)
        status = zcl_transaction_serialize(&transaction, wire, capacity, length);
    zcl_secure_zero(&transaction, sizeof(transaction));
    return status;
}
