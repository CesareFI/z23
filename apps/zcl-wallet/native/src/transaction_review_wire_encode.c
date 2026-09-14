/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "transaction_internal.h"
#include "zcl_keys.h"
#include <string.h>

zcl_status zcl_review_wire_encode(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    const zcl_transparent_tx *transaction, uint8_t *wire, size_t capacity, size_t *length)
{
    struct { uint8_t bytes[ZCL_TX_WIRE_MAX]; size_t expected, length; } work;
    memset(&work, 0, sizeof(work));
    zcl_status status = zcl_transaction_check(transaction, &work.expected);
    if (status != ZCL_OK) goto cleanup;
    if (work.expected == 0 || work.expected > sizeof(work.bytes)) { status = ZCL_INVALID_ENCODING; goto cleanup; }
    if (capacity < work.expected) { status = ZCL_BUFFER_TOO_SMALL; goto cleanup; }
    status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) goto cleanup;
    status = zcl_transaction_serialize(transaction, work.bytes, sizeof(work.bytes), &work.length);
    if (status != ZCL_OK) goto cleanup;
    if (work.length != work.expected) { status = ZCL_INVALID_ENCODING; goto cleanup; }
    status = zcl_review_live(owner, id, now_ms);
    if (status == ZCL_OK) {
        memcpy(wire, work.bytes, work.length);
        *length = work.length;
    }
cleanup:
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
