/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <string.h>

static zcl_status complete_wire(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, const zcl_review_block *block,
    const zcl_signature *signatures, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    struct { uint8_t bytes[ZCL_TX_WIRE_MAX]; size_t length; } work;
    memset(&work, 0, sizeof(work));
    uint64_t now = UINT64_MAX;
    zcl_status status = clock->read(clock->context, &now);
    const size_t bounded = capacity < sizeof(work.bytes) ? capacity : sizeof(work.bytes);
    if (status == ZCL_OK)
        status = zcl_review_p2pkh_wire(owner, id, now, block, signatures, count,
            work.bytes, bounded, &work.length);
    if (status != ZCL_OK) goto cleanup;
    if (work.length == 0 || work.length > bounded) { status = ZCL_INVALID_ENCODING; goto cleanup; }
    /* Providers and serialization may outlast the review. A second trusted
     * sample is mandatory after their last use and before caller publication.
     * UINT64_MAX also refuses a clock that incorrectly reports OK unwritten. */
    now = UINT64_MAX;
    status = clock->read(clock->context, &now);
    if (status == ZCL_OK) status = zcl_review_live(owner, id, now);
    if (status == ZCL_OK) {
        memcpy(wire, work.bytes, work.length);
        *length = work.length;
    }
cleanup:
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_p2pkh_complete(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, const zcl_review_block *block,
    const zcl_signature *signatures, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    if (owner == NULL || clock == NULL || clock->read == NULL || block == NULL ||
        signatures == NULL || wire == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    return complete_wire(owner, id, clock, block, signatures, count, wire, capacity, length);
}
