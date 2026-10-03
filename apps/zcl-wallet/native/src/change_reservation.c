/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_custody_internal.h"

static zcl_status read_index(const zcl_change_custody *wallet, const zcl_change_storage_snapshot *snapshot,
    uint32_t *index)
{
    if (snapshot->file_bytes < 80 || snapshot->file_bytes % 80 != 0 || snapshot->tail_len != 80)
        return ZCL_INVALID_ENCODING;
    if (snapshot->file_bytes >= ZCL_CHANGE_STORAGE_MAX_BYTES) return ZCL_OUT_OF_RANGE;
    zcl_status status = zcl_change_custody_decode(wallet, snapshot->tail, snapshot->tail_len, index);
    if (status == ZCL_OK && *index != snapshot->file_bytes / 80 - 1) status = ZCL_INVALID_ENCODING;
    return status;
}

static zcl_status create_wallet(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len,
    uint8_t *owned_entropy, size_t owned_capacity)
{
    zcl_change_custody wallet = {0};
    uint8_t initial[80] = {0};
    zcl_status status = zcl_change_custody_prepare(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK) status = zcl_change_custody_encode(&wallet, 0, initial);
    /* Authentication/derivation is complete. Only public ciphertext and the
     * authenticated initial record are needed during filesystem operations. */
    wallet.entropy = NULL;
    wallet.entropy_len = 0;
    if (owned_entropy != NULL) zcl_secure_zero(owned_entropy, owned_capacity);
    if (status == ZCL_OK)
        status = zcl_storage_create_with_change(directory, directory_len,
            wallet.record, wallet.record_len, initial, sizeof(initial));
    zcl_secure_zero(initial, sizeof(initial));
    zcl_change_custody_clear(&wallet);
    return status;
}

zcl_status zcl_wallet_change_create(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len)
{
    return create_wallet(directory, directory_len, wallet_record, wallet_len,
        entropy, entropy_len, NULL, 0);
}

zcl_status zcl_wallet_change_create_owned(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, uint8_t *entropy, size_t entropy_len, size_t capacity)
{
    if (entropy == NULL || capacity != 32) return ZCL_INVALID_ARGUMENT;
    return create_wallet(directory, directory_len, wallet_record, wallet_len,
        entropy, entropy_len, entropy, capacity);
}

zcl_status zcl_wallet_change_reserve(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len,
    zcl_change_reservation *reservation)
{
    if (reservation == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_change_custody wallet = {0};
    zcl_change_storage_snapshot snapshot = {0};
    zcl_change_reservation candidate = {0};
    uint8_t next[80] = {0};
    zcl_status status = zcl_change_custody_prepare(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK)
        status = zcl_storage_change_observe(directory, directory_len,
            wallet.record, wallet.record_len, &snapshot);
    if (status == ZCL_OK) status = read_index(&wallet, &snapshot, &candidate.index);
    if (status == ZCL_OK)
        status = zcl_change_custody_address(&wallet, candidate.index, candidate.address, sizeof(candidate.address));
    /* read_index proved index<=65534 before this addition or derivation. */
    if (status == ZCL_OK) status = zcl_change_custody_encode(&wallet, candidate.index + 1, next);
    if (status == ZCL_OK)
        status = zcl_storage_change_append(directory, directory_len, wallet.record, wallet.record_len,
            &snapshot, next, sizeof(next));
    if (status == ZCL_OK) {
        candidate.network = wallet.network;
        *reservation = candidate;
    }
    zcl_secure_zero(next, sizeof(next));
    zcl_secure_zero(&candidate, sizeof(candidate));
    zcl_secure_zero(&snapshot, sizeof(snapshot));
    zcl_change_custody_clear(&wallet);
    return status;
}
