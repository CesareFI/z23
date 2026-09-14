/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <string.h>

typedef struct {
    zcl_signature signatures[ZCL_TX_INPUT_MAX];
    zcl_review_block block;
    zcl_transparent_tx transaction;
} wire_work;

static zcl_status signed_input(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    wire_work *work, size_t index)
{
    uint8_t digest[32] = {0};
    zcl_tx_input *input = &work->transaction.inputs[index];
    if (input->script_len != 0) return ZCL_UNSUPPORTED;
    zcl_status status = zcl_review_sighash_context(owner, id, now_ms, index,
        &work->block, digest, sizeof(digest));
    if (status == ZCL_OK)
        status = zcl_signature_p2pkh(&work->signatures[index], digest, sizeof(digest),
            owner->data.assessment.inputs[index].destination.hash, 20,
            input->script, sizeof(input->script), &input->script_len);
    if (status == ZCL_OK && (input->script_len < 44 || input->script_len > ZCL_SIGNATURE_SCRIPT_MAX))
        status = ZCL_INVALID_ENCODING;
    zcl_secure_zero(digest, sizeof(digest));
    return status;
}

static zcl_status signed_inputs(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    wire_work *work, size_t count)
{
    zcl_status status = zcl_transaction_parse(owner->data.wire, owner->data.wire_length, &work->transaction);
    if (status != ZCL_OK) return status;
    if (work->transaction.input_count != count) return ZCL_INVALID_ENCODING;
    for (size_t index = 0; index < count; ++index) {
        status = signed_input(owner, id, now_ms, work, index);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status reviewed_wire(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    const zcl_review_block *block, const zcl_signature *signatures, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    wire_work work;
    memset(&work, 0, sizeof(work));
    work.block = *block;
    memcpy(work.signatures, signatures, count * sizeof(*signatures));
    zcl_status status = signed_inputs(owner, id, now_ms, &work, count);
    /* The separate publication frame rechecks the ID after private encoding.
     * No mutable borrowed source is consulted here. */
    if (status == ZCL_OK)
        status = zcl_review_wire_encode(owner, id, now_ms, &work.transaction, wire, capacity, length);
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_p2pkh_wire(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    const zcl_review_block *block, const zcl_signature *signatures, size_t signature_count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    if (owner == NULL || block == NULL || signatures == NULL || wire == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (signature_count == 0 || signature_count > ZCL_TX_INPUT_MAX
        || signature_count != owner->data.assessment.input_count) return ZCL_OUT_OF_RANGE;
    return reviewed_wire(owner, id, now_ms, block, signatures, signature_count, wire, capacity, length);
}
