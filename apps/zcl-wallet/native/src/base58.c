/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include "zcl_keys.h"

#include "mbedtls/sha256.h"
#include <string.h>

enum { CHECKED_MAX = 132, TEXT_MAX = 184, CHECKSUM_LEN = 4 };
static const uint8_t alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static zcl_status checksum(const uint8_t *data, size_t length, uint8_t *output, size_t capacity)
{
    if (length > ZCL_BASE58_PAYLOAD_MAX || capacity < CHECKSUM_LEN)
        return ZCL_INVALID_ARGUMENT;
    uint8_t first[32] = {0}, second[32] = {0};
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(data, length, first, 0) == 0 &&
        mbedtls_sha256(first, sizeof(first), second, 0) == 0) {
        memcpy(output, second, 4);
        status = ZCL_OK;
    }
    zcl_secure_zero(first, sizeof(first));
    zcl_secure_zero(second, sizeof(second));
    return status;
}

static size_t leading_zeroes(const uint8_t *bytes, size_t length)
{
    size_t count = 0;
    while (count < length && bytes[count] == 0)
        ++count;
    return count;
}

/* Long division in base 256. carry*256+byte <= 58*256-1. */
static uint8_t divide_by_58(uint8_t *bytes, size_t length)
{
    uint32_t carry = 0;
    for (size_t index = 0; index < length; ++index) {
        uint32_t value = carry * UINT32_C(256) + bytes[index];
        bytes[index] = (uint8_t)(value / UINT32_C(58));
        carry = value % UINT32_C(58);
    }
    return (uint8_t)carry;
}

static zcl_status encode_checked(uint8_t *checked, size_t length,
                                 uint8_t *output, size_t capacity, size_t *written)
{
    uint8_t reversed[TEXT_MAX] = {0};
    zcl_status status = ZCL_OK;
    size_t zeroes = leading_zeroes(checked, length), count = 0;
    while (leading_zeroes(checked, length) < length) {
        if (count == sizeof(reversed)) {
            status = ZCL_OUT_OF_RANGE;
            goto cleanup;
        }
        reversed[count++] = alphabet[divide_by_58(checked, length)];
    }
    if (zeroes > sizeof(reversed) - count) {
        status = ZCL_OUT_OF_RANGE;
        goto cleanup;
    }
    for (size_t index = 0; index < zeroes; ++index)
        reversed[count++] = (uint8_t)'1';
    if (count > capacity) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    for (size_t index = 0; index < count; ++index)
        output[index] = reversed[count - index - 1];
    *written = count;
cleanup:
    zcl_secure_zero(reversed, sizeof(reversed));
    return status;
}

zcl_status zcl_base58check_encode(const uint8_t *payload, size_t payload_len,
                                 uint8_t *text, size_t text_capacity, size_t *text_len)
{
    if (payload == NULL || text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (payload_len == 0 || payload_len > ZCL_BASE58_PAYLOAD_MAX)
        return ZCL_OUT_OF_RANGE;
    uint8_t checked[CHECKED_MAX] = {0}, encoded[TEXT_MAX] = {0};
    memcpy(checked, payload, payload_len);
    zcl_status status = checksum(payload, payload_len, checked + payload_len, sizeof(checked) - payload_len);
    if (status != ZCL_OK)
        goto cleanup;
    size_t count = 0;
    status = encode_checked(checked, payload_len + CHECKSUM_LEN, encoded, sizeof(encoded), &count);
    if (status != ZCL_OK)
        goto cleanup;
    if (text_capacity < count) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    memcpy(text, encoded, count);
    *text_len = count;
cleanup:
    zcl_secure_zero(checked, sizeof(checked));
    zcl_secure_zero(encoded, sizeof(encoded));
    return status;
}

static int base58_digit(uint8_t byte)
{
    for (size_t index = 0; index < 58; ++index) {
        if (alphabet[index] == byte)
            return (int)index;
    }
    return -1;
}

/* Long multiply/add. byte*58+carry <= 255*58+57; carry <= 57. */
static zcl_status multiply_by_58(uint8_t *bytes, size_t length, uint32_t digit)
{
    if (length == 0 || length > CHECKED_MAX || digit >= 58)
        return ZCL_INVALID_ARGUMENT;
    uint32_t carry = digit;
    for (size_t index = length; index > 0; --index) {
        uint32_t value = (uint32_t)bytes[index - 1] * UINT32_C(58) + carry;
        bytes[index - 1] = (uint8_t)(value & UINT32_C(255));
        carry = value >> 8;
    }
    return carry == 0 ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

static zcl_status decode_checked(const uint8_t *text, size_t length,
                                 uint8_t *output, size_t capacity, size_t *written)
{
    uint8_t magnitude[CHECKED_MAX] = {0};
    zcl_status status = ZCL_OK;
    size_t zeroes = 0;
    while (zeroes < length && text[zeroes] == (uint8_t)'1')
        ++zeroes;
    for (size_t index = 0; index < length; ++index) {
        int digit = base58_digit(text[index]);
        if (digit < 0) {
            status = ZCL_INVALID_ENCODING;
            goto cleanup;
        }
        status = multiply_by_58(magnitude, sizeof(magnitude), (uint32_t)digit);
        if (status != ZCL_OK) goto cleanup;
    }
    size_t leading = leading_zeroes(magnitude, sizeof(magnitude));
    size_t significant = sizeof(magnitude) - leading;
    if (zeroes > CHECKED_MAX - significant) {
        status = ZCL_OUT_OF_RANGE;
        goto cleanup;
    }
    if (zeroes + significant > capacity) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    memset(output, 0, zeroes);
    memcpy(output + zeroes, magnitude + leading, significant);
    *written = zeroes + significant;
cleanup:
    zcl_secure_zero(magnitude, sizeof(magnitude));
    return status;
}

static zcl_status verify_checksum(const uint8_t *checked, size_t length)
{
    if (length < 5)
        return ZCL_INVALID_ENCODING;
    uint8_t expected[4] = {0};
    zcl_status status = checksum(checked, length - CHECKSUM_LEN, expected, sizeof(expected));
    if (status == ZCL_OK) {
        uint8_t difference = 0;
        for (size_t index = 0; index < CHECKSUM_LEN; ++index)
            difference = (uint8_t)(difference | (expected[index] ^ checked[length - CHECKSUM_LEN + index]));
        status = difference == 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
    }
    zcl_secure_zero(expected, sizeof(expected));
    return status;
}

zcl_status zcl_base58check_decode(const uint8_t *text, size_t text_len,
                                 uint8_t *payload, size_t payload_capacity, size_t *payload_len)
{
    if (text == NULL || payload == NULL || payload_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (text_len == 0 || text_len > ZCL_BASE58_TEXT_MAX)
        return ZCL_OUT_OF_RANGE;
    uint8_t checked[CHECKED_MAX] = {0};
    size_t length = 0;
    zcl_status status = decode_checked(text, text_len, checked, sizeof(checked), &length);
    if (status != ZCL_OK)
        goto cleanup;
    status = verify_checksum(checked, length);
    if (status != ZCL_OK)
        goto cleanup;
    size_t size = length - CHECKSUM_LEN;
    if (payload_capacity < size) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    memcpy(payload, checked, size);
    *payload_len = size;
cleanup:
    zcl_secure_zero(checked, sizeof(checked));
    return status;
}
