/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include "mbedtls/sha256.h"
#include <string.h>

enum { CHECKED_MAX = 132, TEXT_MAX = 184, CHECKSUM_LEN = 4 };
static const uint8_t alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static zcl_status checksum(const uint8_t *data, size_t length, uint8_t *output, size_t capacity)
{
    if (length > ZCL_BASE58_PAYLOAD_MAX || capacity < CHECKSUM_LEN)
        return ZCL_INVALID_ARGUMENT;
    uint8_t first[32] = {0}, second[32] = {0};
    if (mbedtls_sha256(data, length, first, 0) != 0)
        return ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(first, sizeof(first), second, 0) != 0)
        return ZCL_CRYPTO_FAILURE;
    memcpy(output, second, 4);
    return ZCL_OK;
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
    size_t zeroes = leading_zeroes(checked, length), count = 0;
    size_t start = zeroes;
    while (start < length) {
        if (count == sizeof(reversed))
            return ZCL_OUT_OF_RANGE;
        /* The zero prefix contributes no carry. Retire it monotonically;
         * each advance is bounded by the remaining caller-owned scratch. */
        reversed[count++] = alphabet[divide_by_58(checked + start, length - start)];
        start += leading_zeroes(checked + start, length - start);
    }
    if (zeroes > sizeof(reversed) - count)
        return ZCL_OUT_OF_RANGE;
    for (size_t index = 0; index < zeroes; ++index)
        reversed[count++] = (uint8_t)'1';
    if (count > capacity)
        return ZCL_BUFFER_TOO_SMALL;
    for (size_t index = 0; index < count; ++index)
        output[index] = reversed[count - index - 1];
    *written = count;
    return ZCL_OK;
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
        return status;
    size_t count = 0;
    status = encode_checked(checked, payload_len + CHECKSUM_LEN, encoded, sizeof(encoded), &count);
    if (status != ZCL_OK)
        return status;
    if (text_capacity < count)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(text, encoded, count);
    *text_len = count;
    return ZCL_OK;
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
    size_t zeroes = 0, used = 0;
    while (zeroes < length && text[zeroes] == (uint8_t)'1')
        ++zeroes;
    for (size_t index = 0; index < length; ++index) {
        int digit = base58_digit(text[index]);
        if (digit < 0)
            return ZCL_INVALID_ENCODING;
        /* Multiplication by 58 needs at most one additional base-256 byte.
         * The untouched prefix stays zero; retain the full-size overflow check. */
        const size_t span = used < sizeof(magnitude) ? used + 1 : used;
        const size_t start = sizeof(magnitude) - span;
        zcl_status status = multiply_by_58(magnitude + start, span, (uint32_t)digit);
        if (status != ZCL_OK)
            return status;
        used = span - leading_zeroes(magnitude + start, span);
    }
    size_t leading = sizeof(magnitude) - used;
    size_t significant = used;
    if (zeroes > CHECKED_MAX - significant)
        return ZCL_OUT_OF_RANGE;
    if (zeroes + significant > capacity)
        return ZCL_BUFFER_TOO_SMALL;
    memset(output, 0, zeroes);
    memcpy(output + zeroes, magnitude + leading, significant);
    *written = zeroes + significant;
    return ZCL_OK;
}

static zcl_status verify_checksum(const uint8_t *checked, size_t length)
{
    if (length < 5)
        return ZCL_INVALID_ENCODING;
    uint8_t expected[4] = {0};
    zcl_status status = checksum(checked, length - CHECKSUM_LEN, expected, sizeof(expected));
    if (status != ZCL_OK)
        return status;
    uint8_t difference = 0;
    for (size_t index = 0; index < CHECKSUM_LEN; ++index)
        difference = (uint8_t)(difference | (expected[index] ^ checked[length - CHECKSUM_LEN + index]));
    return difference == 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
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
        return status;
    status = verify_checksum(checked, length);
    if (status != ZCL_OK)
        return status;
    size_t size = length - CHECKSUM_LEN;
    if (payload_capacity < size)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(payload, checked, size);
    *payload_len = size;
    return ZCL_OK;
}
