/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "change_custody_internal.h"
#include <string.h>

typedef struct {
    zcl_review_wallet_input claim;
    zcl_change_custody wallet;
    uint8_t directory[ZCL_STORAGE_PATH_MAX];
    uint8_t entropy[ZCL_ENTROPY_MAX];
    uint8_t expected[35];
    uint8_t derived[35];
} review_wallet_work;

static zcl_status claim_bounds(const zcl_review_wallet_input *claim)
{
    if (claim->directory_len == 0 || claim->directory_len > ZCL_STORAGE_PATH_MAX) return ZCL_OUT_OF_RANGE;
    if (claim->record_len < 124 || claim->record_len > ZCL_WALLET_RECORD_MAX) return ZCL_OUT_OF_RANGE;
    if (claim->entropy_len < 16 || claim->entropy_len > ZCL_ENTROPY_MAX || claim->entropy_len % 4 != 0)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status claim_profile(const zcl_review_wallet_input *claim)
{
    if (claim->directory == NULL || claim->record == NULL || claim->entropy == NULL) return ZCL_INVALID_ARGUMENT;
    if (claim->chain > 1) return ZCL_UNSUPPORTED;
    if (claim->chain == 0 && claim->index != 0) return ZCL_UNSUPPORTED;
    if (claim->index >= ZCL_CHANGE_STORAGE_MAX_RECORDS - 1) return ZCL_OUT_OF_RANGE;
    return claim_bounds(claim);
}

static zcl_status prepare_claim(review_wallet_work *work, const zcl_address *destination)
{
    zcl_review_wallet_input *claim = &work->claim;
    zcl_status status = claim_profile(claim);
    if (status != ZCL_OK) return status;
    memcpy(work->entropy, claim->entropy, claim->entropy_len);
    memcpy(work->directory, claim->directory, claim->directory_len);
    status = zcl_change_custody_prepare(claim->record, claim->record_len, work->entropy,
        claim->entropy_len, &work->wallet);
    if (status != ZCL_OK) return status;
    if (work->wallet.network != destination->network) return ZCL_UNSUPPORTED;
    claim->directory = work->directory;
    claim->record = work->wallet.record;
    claim->entropy = work->entropy;
    return ZCL_OK;
}

static zcl_status committed_wallet(const zcl_review_wallet_input *claim)
{
    uint8_t record[ZCL_WALLET_RECORD_MAX] = {0};
    size_t length = 0;
    bool pending = false;
    zcl_status status = zcl_storage_read(claim->directory, claim->directory_len,
        record, sizeof(record), &length, &pending);
    if (status == ZCL_OK && pending) status = ZCL_NOT_FOUND;
    if (status == ZCL_OK && (length != claim->record_len || memcmp(record, claim->record, length) != 0))
        status = ZCL_ALREADY_EXISTS;
    zcl_secure_zero(record, sizeof(record));
    return status;
}

static zcl_status receive_owner(const zcl_review_wallet_input *claim, uint8_t *address, size_t capacity)
{
    zcl_status status = committed_wallet(claim);
    if (status != ZCL_OK) return status;
    uint8_t blinding[32] = {0};
    status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_wallet_recovered_address(claim->record, ZCL_WALLET_HEADER_BYTES,
            claim->entropy, claim->entropy_len, blinding, sizeof(blinding), address, capacity);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

static zcl_status claimed_address(const zcl_review_wallet_input *claim, uint8_t *address, size_t capacity)
{
    if (claim->chain == 0) return receive_owner(claim, address, capacity);
    return zcl_wallet_change_reserved_address(claim->directory, claim->directory_len,
        claim->record, claim->record_len, claim->entropy, claim->entropy_len, claim->index, address, capacity);
}

static zcl_status check_wallet(const zcl_review_wallet_input *claim, const zcl_address *destination)
{
    review_wallet_work work;
    memset(&work, 0, sizeof(work));
    work.claim = *claim;
    zcl_status status = prepare_claim(&work, destination);
    size_t length = 0;
    if (status == ZCL_OK)
        status = zcl_address_encode(destination, work.expected, sizeof(work.expected), &length);
    if (status == ZCL_OK && length != sizeof(work.expected)) status = ZCL_INVALID_ENCODING;
    if (status == ZCL_OK) status = claimed_address(&work.claim, work.derived, sizeof(work.derived));
    if (status == ZCL_OK && memcmp(work.derived, work.expected, sizeof(work.expected)) != 0)
        status = ZCL_NOT_FOUND;
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_input_wallet_check(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_wallet_input *claim)
{
    if (owner == NULL || claim == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (input_index >= owner->data.assessment.input_count || input_index >= ZCL_TX_INPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    const zcl_address *destination = &owner->data.assessment.inputs[input_index].destination;
    if (destination->kind != ZCL_P2PKH) return ZCL_UNSUPPORTED;
    return check_wallet(claim, destination);
}
