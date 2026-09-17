/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_draft_internal.h"
#include "transaction_draft_internal.h"
#include "zcl_keys.h"

static zcl_status draft_wire(const zcl_draft_request *request,
    uint8_t *wire, size_t capacity, size_t *length, bool full_sources)
{
    zcl_transparent_tx transaction = {0};
    zcl_status status = full_sources ? zcl_transaction_draft_full_sources(request, &transaction)
        : zcl_transaction_draft(request, &transaction);
    if (status == ZCL_OK)
        status = zcl_transaction_serialize(&transaction, wire, capacity, length);
    zcl_secure_zero(&transaction, sizeof(transaction));
    return status;
}

zcl_status zcl_jni_draft_wire(const zcl_draft_request *request,
    uint8_t *wire, size_t capacity, size_t *length)
{
    return draft_wire(request, wire, capacity, length, false);
}

zcl_status zcl_jni_draft_full_wire(const zcl_draft_request *request,
    uint8_t *wire, size_t capacity, size_t *length)
{
    return draft_wire(request, wire, capacity, length, true);
}
