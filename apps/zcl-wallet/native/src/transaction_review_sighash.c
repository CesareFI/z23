/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "transaction_sighash.h"
#include "zcl_keys.h"
#include <string.h>

static zcl_status reviewed_hash(const zcl_review_data *review, size_t index, uint32_t branch,
                               uint8_t *digest, size_t capacity)
{
    struct {
        zcl_transparent_tx transaction;
        uint8_t script[25];
        size_t script_length;
        uint8_t digest[32];
    } work;
    if (capacity < sizeof(work.digest)) return ZCL_BUFFER_TOO_SMALL;
    memset(&work, 0, sizeof(work));
    const zcl_assessed_output *input = &review->assessment.inputs[index];
    zcl_status status = zcl_transaction_parse(review->wire, review->wire_length, &work.transaction);
    if (status == ZCL_OK)
        status = zcl_address_script(&input->destination, work.script, sizeof(work.script), &work.script_length);
    if (status == ZCL_OK)
        status = zcl_transaction_sighash_all(&work.transaction, index, work.script,
            work.script_length, input->value, branch, work.digest, sizeof(work.digest));
    if (status == ZCL_OK) memcpy(digest, work.digest, 32);
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_sighash_p2pkh(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, uint32_t branch, uint8_t *digest, size_t capacity)
{
    if (owner == NULL || digest == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (capacity < 32) return ZCL_BUFFER_TOO_SMALL;
    if (input_index >= owner->data.assessment.input_count || input_index >= ZCL_TX_INPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    if (owner->data.assessment.inputs[input_index].destination.kind != ZCL_P2PKH)
        return ZCL_UNSUPPORTED;
    return reviewed_hash(&owner->data, input_index, branch, digest, capacity);
}
