/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <string.h>

typedef struct {
    zcl_review_wallet_input claims[ZCL_TX_INPUT_MAX];
    zcl_signature signatures[ZCL_TX_INPUT_MAX];
    zcl_review_block block;
} signing_work;

static zcl_status preflight(zcl_review_owner *owner, uint64_t id, const zcl_review_clock *clock,
    const signing_work *work, size_t count)
{
    for (size_t index = 0; index < count; ++index) {
        uint64_t now = UINT64_MAX;
        zcl_status status = clock->read(clock->context, &now);
        if (status == ZCL_OK)
            status = zcl_review_input_wallet_check(owner, id, now, index, &work->claims[index]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status sign_inputs(zcl_review_owner *owner, uint64_t id, const zcl_review_clock *clock,
    signing_work *work, size_t count)
{
    for (size_t index = 0; index < count; ++index) {
        const zcl_status status = zcl_review_input_wallet_sign(owner, id, clock, index,
            &work->block, &work->claims[index], &work->signatures[index]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status signed_transaction(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, const zcl_review_block *block,
    const zcl_review_wallet_input *claims, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    signing_work work;
    memset(&work, 0, sizeof(work));
    work.block = *block;
    memcpy(work.claims, claims, count * sizeof(*claims));
    zcl_status status = preflight(owner, id, clock, &work, count);
    if (status == ZCL_OK) status = sign_inputs(owner, id, clock, &work, count);
    /* Every private-key operation is over. Retire borrowed claim pointers
     * before public signature verification/serialization and final clocks. */
    zcl_secure_zero(work.claims, sizeof(work.claims));
    if (status == ZCL_OK)
        status = zcl_review_p2pkh_complete(owner, id, clock, &work.block, work.signatures,
            count, wire, capacity, length);
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

static zcl_status signing_count(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, size_t count)
{
    uint64_t now = UINT64_MAX;
    zcl_status status = clock->read(clock->context, &now);
    if (status == ZCL_OK) status = zcl_review_live(owner, id, now);
    if (status != ZCL_OK) return status;
    if (count == 0 || count > ZCL_TX_INPUT_MAX || count != owner->data.assessment.input_count)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

zcl_status zcl_review_wallet_transaction_sign(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, const zcl_review_block *block,
    const zcl_review_wallet_input *claims, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    if (owner == NULL || clock == NULL || clock->read == NULL || block == NULL ||
        claims == NULL || wire == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = signing_count(owner, id, clock, count);
    return status == ZCL_OK ? signed_transaction(owner, id, clock, block, claims, count,
        wire, capacity, length) : status;
}
