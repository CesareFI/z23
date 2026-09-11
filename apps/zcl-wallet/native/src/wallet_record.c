/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"

#include <string.h>

static zcl_status validate_buffers(const uint8_t *iv, size_t iv_len,
                                   const uint8_t *ciphertext, const uint8_t *record,
                                   const size_t *record_len)
{
    if (iv == NULL || ciphertext == NULL || record == NULL || record_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    return iv_len == ZCL_WALLET_IV_BYTES ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

zcl_status zcl_wallet_record_pack(const uint8_t *header, size_t header_len,
                                  const uint8_t *iv, size_t iv_len,
                                  const uint8_t *ciphertext, size_t ciphertext_len,
                                  uint8_t *record, size_t capacity, size_t *record_len)
{
    zcl_status status = validate_buffers(iv, iv_len, ciphertext, record, record_len);
    if (status != ZCL_OK)
        return status;
    zcl_wallet_info info = {0};
    status = zcl_wallet_header_parse(header, header_len, &info);
    if (status != ZCL_OK)
        return status;
    if (ciphertext_len != info.entropy_len + ZCL_WALLET_TAG_BYTES)
        return ZCL_OUT_OF_RANGE;
    size_t total = ZCL_WALLET_HEADER_BYTES + ZCL_WALLET_IV_BYTES + ciphertext_len;
    if (capacity < total)
        return ZCL_BUFFER_TOO_SMALL;
    uint8_t result[140] = {0};
    memcpy(result, header, header_len);
    memcpy(result + 80, iv, iv_len);
    memcpy(result + 92, ciphertext, ciphertext_len);
    memcpy(record, result, total);
    *record_len = total;
    return ZCL_OK;
}

zcl_status zcl_wallet_record_parse(const uint8_t *record, size_t record_len, zcl_wallet_record *output)
{
    if (record == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (record_len < 124 || record_len > ZCL_WALLET_RECORD_MAX)
        return ZCL_OUT_OF_RANGE;
    zcl_wallet_record result = {0};
    zcl_status status = zcl_wallet_header_parse(record, ZCL_WALLET_HEADER_BYTES, &result.info);
    if (status != ZCL_OK)
        return status;
    size_t ciphertext_len = result.info.entropy_len + ZCL_WALLET_TAG_BYTES;
    if (record_len != ZCL_WALLET_HEADER_BYTES + ZCL_WALLET_IV_BYTES + ciphertext_len)
        return ZCL_INVALID_ENCODING;
    memcpy(result.header, record, sizeof(result.header));
    memcpy(result.iv, record + 80, sizeof(result.iv));
    memcpy(result.ciphertext, record + 92, ciphertext_len);
    result.ciphertext_len = ciphertext_len;
    *output = result;
    return ZCL_OK;
}
