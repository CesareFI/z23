/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_change_reservation.h"
#include <string.h>

typedef struct {
    uint8_t record[140];
    size_t record_len;
    const uint8_t *entropy; /* Borrowed only for this synchronous call. */
    size_t entropy_len;
    zcl_network network;
} change_wallet;

static zcl_status prepare_wallet(const uint8_t *record, size_t record_len,
    const uint8_t *entropy, size_t entropy_len, change_wallet *wallet)
{
    if (record == NULL || entropy == NULL) return ZCL_INVALID_ARGUMENT;
    if (record_len < 124 || record_len > sizeof(wallet->record)) return ZCL_OUT_OF_RANGE;
    memcpy(wallet->record, record, record_len);
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(wallet->record, record_len, &parsed);
    if (status != ZCL_OK) return status;
    if (entropy_len != parsed.info.entropy_len) return ZCL_OUT_OF_RANGE;
    wallet->record_len = record_len;
    wallet->entropy = entropy;
    wallet->entropy_len = entropy_len;
    wallet->network = parsed.info.network;
    return ZCL_OK;
}

static zcl_status encode_state(const change_wallet *wallet, uint32_t index, uint8_t *state)
{
    uint8_t blinding[32] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_change_state_encode(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            blinding, sizeof(blinding), index, state, 80);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

static zcl_status read_index(const change_wallet *wallet, const zcl_change_storage_snapshot *snapshot,
    uint32_t *index)
{
    if (snapshot->file_bytes < 80 || snapshot->file_bytes % 80 != 0 || snapshot->tail_len != 80)
        return ZCL_INVALID_ENCODING;
    if (snapshot->file_bytes >= ZCL_CHANGE_STORAGE_MAX_BYTES) return ZCL_OUT_OF_RANGE;
    uint8_t blinding[32] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_change_state_decode(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            blinding, sizeof(blinding), snapshot->tail, snapshot->tail_len, index);
    if (status == ZCL_OK && *index != snapshot->file_bytes / 80 - 1) status = ZCL_INVALID_ENCODING;
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

static zcl_status derive_change(const change_wallet *wallet, uint32_t index, uint8_t *address)
{
    uint8_t blinding[64] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_wallet_recovered_change(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            index, blinding, sizeof(blinding), address, 35);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

zcl_status zcl_wallet_change_create(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len)
{
    change_wallet wallet = {0};
    uint8_t initial[80] = {0};
    zcl_status status = prepare_wallet(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK) status = encode_state(&wallet, 0, initial);
    if (status == ZCL_OK)
        status = zcl_storage_create_with_change(directory, directory_len,
            wallet.record, wallet.record_len, initial, sizeof(initial));
    return status;
}

zcl_status zcl_wallet_change_reserve(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len,
    zcl_change_reservation *reservation)
{
    if (reservation == NULL) return ZCL_INVALID_ARGUMENT;
    change_wallet wallet = {0};
    zcl_change_storage_snapshot snapshot = {0};
    zcl_change_reservation candidate = {0};
    uint8_t next[80] = {0};
    zcl_status status = prepare_wallet(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK)
        status = zcl_storage_change_observe(directory, directory_len,
            wallet.record, wallet.record_len, &snapshot);
    if (status == ZCL_OK) status = read_index(&wallet, &snapshot, &candidate.index);
    if (status == ZCL_OK) status = derive_change(&wallet, candidate.index, candidate.address);
    /* read_index proved index<=65534 before this addition or derivation. */
    if (status == ZCL_OK) status = encode_state(&wallet, candidate.index + 1, next);
    if (status == ZCL_OK)
        status = zcl_storage_change_append(directory, directory_len, wallet.record, wallet.record_len,
            &snapshot, next, sizeof(next));
    if (status == ZCL_OK) {
        candidate.network = wallet.network;
        *reservation = candidate;
    }
    return status;
}
