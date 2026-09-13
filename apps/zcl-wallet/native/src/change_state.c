/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_state_internal.h"
#include <string.h>

static zcl_status state_prefix(const uint8_t *record, size_t length, uint32_t *next_index)
{
    if (record == NULL) return ZCL_INVALID_ARGUMENT;
    if (length != ZCL_CHANGE_STATE_BYTES) return ZCL_OUT_OF_RANGE;
    static const uint8_t start[8] = {'Z', 'C', 'L', 'I', 1, 1, 0, 0};
    static const uint8_t reserved[4] = {0};
    if (memcmp(record, start, sizeof(start)) != 0) return ZCL_UNSUPPORTED;
    if (memcmp(record + 12, reserved, sizeof(reserved)) != 0) return ZCL_INVALID_ENCODING;
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) value |= (uint32_t)record[8 + i] << (8 * i);
    if (value > ZCL_CHANGE_INDEX_EXHAUSTED) return ZCL_OUT_OF_RANGE;
    *next_index = value;
    return ZCL_OK;
}

static bool matching_tag(const uint8_t *first, const uint8_t *second)
{
    volatile uint8_t difference = 0;
    for (size_t i = 0; i < 64; ++i)
        difference = (uint8_t)(difference | (uint8_t)(first[i] ^ second[i]));
    const bool equal = difference == 0;
    difference = 0;
    return equal;
}

zcl_status zcl_change_state_encode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint32_t next_index, uint8_t *record, size_t capacity)
{
    if (record == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity < ZCL_CHANGE_STATE_BYTES) return ZCL_BUFFER_TOO_SMALL;
    if (next_index > ZCL_CHANGE_INDEX_EXHAUSTED) return ZCL_OUT_OF_RANGE;
    uint8_t candidate[80] = {'Z', 'C', 'L', 'I', 1, 1}, key[64] = {0};
    for (size_t i = 0; i < 4; ++i)
        candidate[8 + i] = (uint8_t)((next_index >> (8 * i)) & UINT32_C(0xff));
    zcl_status status = zcl_change_state_key(header, header_len, entropy, entropy_len,
        blinding, blinding_len, key, sizeof(key));
    if (status == ZCL_OK)
        status = zcl_hmac_sha512(key, sizeof(key), candidate, 16, candidate + 16, 64);
    if (status == ZCL_OK) memcpy(record, candidate, sizeof(candidate));
    zcl_secure_zero(key, sizeof(key));
    return status;
}

zcl_status zcl_change_state_decode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    const uint8_t *record, size_t record_len, uint32_t *next_index)
{
    if (next_index == NULL) return ZCL_INVALID_ARGUMENT;
    uint32_t candidate = 0;
    zcl_status status = state_prefix(record, record_len, &candidate);
    if (status != ZCL_OK) return status;
    uint8_t key[64] = {0}, expected[64] = {0};
    status = zcl_change_state_key(header, header_len, entropy, entropy_len,
        blinding, blinding_len, key, sizeof(key));
    if (status == ZCL_OK)
        status = zcl_hmac_sha512(key, sizeof(key), record, 16, expected, sizeof(expected));
    if (status == ZCL_OK && !matching_tag(expected, record + 16)) status = ZCL_INVALID_ENCODING;
    if (status == ZCL_OK) *next_index = candidate;
    zcl_secure_zero(key, sizeof(key));
    zcl_secure_zero(expected, sizeof(expected));
    return status;
}
