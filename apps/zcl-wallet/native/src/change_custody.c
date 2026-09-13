/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_custody_internal.h"
#include <string.h>

zcl_status zcl_change_custody_prepare(const uint8_t *record, size_t record_len,
    const uint8_t *entropy, size_t entropy_len, zcl_change_custody *wallet)
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

zcl_status zcl_change_custody_encode(const zcl_change_custody *wallet, uint32_t index, uint8_t *state)
{
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
    uint8_t blinding[32] = {0};
    zcl_status status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_change_state_decode(wallet->record, 80, wallet->entropy, wallet->entropy_len,
            blinding, sizeof(blinding), state, state_len, index);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}
