/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <string.h>

zcl_status zcl_review_open(zcl_review_owner *owner, const uint8_t *wire, size_t length,
                           zcl_network network, const zcl_previous_transaction *previous,
                           size_t previous_count, uint64_t maximum_fee, uint64_t now_ms,
                           uint64_t *id)
{
    if (owner == NULL || id == NULL) return ZCL_INVALID_ARGUMENT;
    if (owner->data.id != 0) return ZCL_BUSY;
    if (owner->issued >= ZCL_REVIEW_ID_MAX) return ZCL_RESOURCE_EXHAUSTED;
    if (now_ms > UINT64_MAX - ZCL_REVIEW_LIFETIME_MS) return ZCL_OUT_OF_RANGE;
    zcl_review_data candidate = {0};
    const zcl_status status = zcl_review_prepare(wire, length, network, previous, previous_count,
        maximum_fee, &candidate);
    if (status == ZCL_OK) {
        candidate.id = owner->issued + 1;
        candidate.last_ms = now_ms;
        candidate.deadline_ms = now_ms + ZCL_REVIEW_LIFETIME_MS;
        owner->data = candidate;
        owner->issued = candidate.id;
        *id = candidate.id;
    }
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}

void zcl_review_clear(zcl_review_owner *owner)
{
    if (owner != NULL) zcl_secure_zero(&owner->data, sizeof(owner->data));
}

zcl_status zcl_review_live(zcl_review_owner *owner, uint64_t id, uint64_t now_ms)
{
    if (id == 0 || id > ZCL_REVIEW_ID_MAX || owner->data.id != id) return ZCL_CANCELLED;
    if (now_ms < owner->data.last_ms) {
        zcl_review_clear(owner);
        return ZCL_CANCELLED;
    }
    if (now_ms >= owner->data.deadline_ms) {
        zcl_review_clear(owner);
        return ZCL_TIMED_OUT;
    }
    owner->data.last_ms = now_ms;
    return ZCL_OK;
}

zcl_status zcl_review_snapshot_get(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
                                   zcl_review_snapshot *snapshot)
{
    if (owner == NULL || snapshot == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    zcl_review_snapshot candidate = {0};
    candidate.assessment = owner->data.assessment;
    candidate.context = owner->data.context;
    candidate.remaining_ms = owner->data.deadline_ms - now_ms;
    *snapshot = candidate;
    zcl_secure_zero(&candidate, sizeof(candidate));
    return ZCL_OK;
}

zcl_status zcl_review_copy_wire(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
                                uint8_t *wire, size_t capacity, size_t *length)
{
    if (owner == NULL || wire == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (capacity < owner->data.wire_length) return ZCL_BUFFER_TOO_SMALL;
    memcpy(wire, owner->data.wire, owner->data.wire_length);
    *length = owner->data.wire_length;
    return ZCL_OK;
}

zcl_status zcl_review_cancel(zcl_review_owner *owner, uint64_t id)
{
    if (owner == NULL) return ZCL_INVALID_ARGUMENT;
    if (id == 0 || id > ZCL_REVIEW_ID_MAX || owner->data.id != id) return ZCL_CANCELLED;
    zcl_review_clear(owner);
    return ZCL_OK;
}
