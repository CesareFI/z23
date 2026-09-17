/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_custody_internal.h"
#include <string.h>

static zcl_status consumed_index(const zcl_change_custody *wallet,
                                 const zcl_change_storage_snapshot *snapshot, uint32_t requested)
{
    if (snapshot->file_bytes < 80 || snapshot->file_bytes % 80 != 0 || snapshot->tail_len != 80)
        return ZCL_INVALID_ENCODING;
    /* A full journal remains readable: it consumes indexes0..65534. */
    if (snapshot->file_bytes > ZCL_CHANGE_STORAGE_MAX_BYTES) return ZCL_OUT_OF_RANGE;
    uint32_t next = 0;
    zcl_status status = zcl_change_custody_decode(wallet, snapshot->tail, snapshot->tail_len, &next);
    if (status != ZCL_OK) return status;
    if (next != snapshot->file_bytes / 80 - 1) return ZCL_INVALID_ENCODING;
    return requested < next ? ZCL_OK : ZCL_NOT_FOUND;
}

zcl_status zcl_wallet_change_reserved_address(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len,
    uint32_t index, uint8_t *address, size_t capacity)
{
    if (address == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity < 35) return ZCL_BUFFER_TOO_SMALL;
    if (index >= ZCL_CHANGE_STORAGE_MAX_RECORDS - 1) return ZCL_OUT_OF_RANGE;
    zcl_change_custody wallet = {0};
    zcl_change_storage_snapshot snapshot = {0};
    uint8_t candidate[35] = {0};
    zcl_status status = zcl_change_custody_prepare(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK)
        status = zcl_storage_change_observe(directory, directory_len, wallet.record, wallet.record_len, &snapshot);
    if (status == ZCL_OK) status = consumed_index(&wallet, &snapshot, index);
    if (status == ZCL_OK) status = zcl_change_custody_address(&wallet, index, candidate, sizeof(candidate));
    if (status == ZCL_OK) memcpy(address, candidate, sizeof(candidate));
    zcl_secure_zero(candidate, sizeof(candidate));
    zcl_secure_zero(&snapshot, sizeof(snapshot));
    zcl_change_custody_clear(&wallet);
    return status;
}
