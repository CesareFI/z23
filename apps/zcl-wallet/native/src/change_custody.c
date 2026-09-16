/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_custody_internal.h"
#include <string.h>

void zcl_change_custody_clear(zcl_change_custody *wallet)
{
    if (wallet != NULL) zcl_secure_zero(wallet, sizeof(*wallet));
}

zcl_status zcl_change_custody_prepare(const uint8_t *record, size_t record_len,
    const uint8_t *entropy, size_t entropy_len, zcl_change_custody *wallet)
{
    if (wallet == NULL || record == NULL || entropy == NULL) return ZCL_INVALID_ARGUMENT;
    if (record_len < 124 || record_len > sizeof(wallet->record)) return ZCL_OUT_OF_RANGE;
    memcpy(wallet->record, record, record_len);
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(wallet->record, record_len, &parsed);
    if (status == ZCL_OK && entropy_len != parsed.info.entropy_len) status = ZCL_OUT_OF_RANGE;
    if (status == ZCL_OK) {
        wallet->record_len = record_len;
        wallet->entropy = entropy;
        wallet->entropy_len = entropy_len;
        wallet->network = parsed.info.network;
    }
    zcl_secure_zero(&parsed, sizeof(parsed));
    return status;
}

zcl_status zcl_change_custody_encode(const zcl_change_custody *wallet, uint32_t index, uint8_t *state)
{
    if (wallet == NULL || state == NULL) return ZCL_INVALID_ARGUMENT;
    uint8_t blinding[32] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_change_state_encode(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            blinding, sizeof(blinding), index, state, 80);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

zcl_status zcl_change_custody_decode(const zcl_change_custody *wallet,
    const uint8_t *state, size_t state_len, uint32_t *index)
{
    if (wallet == NULL || state == NULL || index == NULL) return ZCL_INVALID_ARGUMENT;
    uint8_t blinding[32] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_change_state_decode(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            blinding, sizeof(blinding), state, state_len, index);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

zcl_status zcl_change_custody_address(const zcl_change_custody *wallet,
    uint32_t index, uint8_t *address, size_t capacity)
{
    if (wallet == NULL || address == NULL) return ZCL_INVALID_ARGUMENT;
    uint8_t blinding[64] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_wallet_recovered_change(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            index, blinding, sizeof(blinding), address, capacity);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}
