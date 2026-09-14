/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "transaction_context.h"
#include "zcl_keys.h"
#include <string.h>

static bool final_at(const zcl_review_data *review, const zcl_review_block *block)
{
    const uint32_t lock = review->context.lock_time;
    if (lock == 0) return true;
    const uint64_t comparison = lock < UINT32_C(500000000) ? block->height : block->lock_time_cutoff;
    if (lock < comparison) return true;
    for (size_t i = 0; i < review->assessment.input_count; ++i)
        if (review->context.inputs[i].sequence != UINT32_MAX) return false;
    return true;
}

static zcl_status context_branch(const zcl_review_data *review, const zcl_review_block *block, uint32_t *branch)
{
    if (block->network != review->assessment.network) return ZCL_UNSUPPORTED;
    if (block->lock_time_cutoff > INT64_MAX) return ZCL_OUT_OF_RANGE;
    const zcl_status status = zcl_transaction_v4_branch(block->network, block->height, branch);
    if (status != ZCL_OK) return status;
    if (review->context.expiry_height != 0 && block->height > review->context.expiry_height)
        return ZCL_OUT_OF_RANGE;
    return final_at(review, block) ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

static zcl_status contextual_hash(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_block *block, uint8_t *digest)
{
    struct { zcl_review_block block; uint32_t branch; uint8_t digest[32]; } work;
    memset(&work, 0, sizeof(work));
    work.block = *block;
    zcl_status status = context_branch(&owner->data, &work.block, &work.branch);
    if (status == ZCL_OK)
        status = zcl_review_sighash_p2pkh(owner, id, now_ms, input_index, work.branch, work.digest, sizeof(work.digest));
    if (status == ZCL_OK) memcpy(digest, work.digest, sizeof(work.digest));
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_sighash_context(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_block *block, uint8_t *digest, size_t capacity)
{
    if (owner == NULL || block == NULL || digest == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (capacity < 32) return ZCL_BUFFER_TOO_SMALL;
    if (owner->data.assessment.input_count == 0 || owner->data.assessment.input_count > ZCL_TX_INPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    if (input_index >= owner->data.assessment.input_count) return ZCL_OUT_OF_RANGE;
    return contextual_hash(owner, id, now_ms, input_index, block, digest);
}
