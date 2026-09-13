/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <stdbool.h>
#include <string.h>

static bool is_digit(uint8_t byte)
{
    return byte >= (uint8_t)'0' && byte <= (uint8_t)'9';
}

/* The subtraction precedes multiplication: no unchecked accumulator growth. */
static zcl_status append_digit(uint64_t *value, uint8_t byte, uint64_t limit)
{
    uint64_t digit = (uint64_t)(byte - (uint8_t)'0');
    if (digit > limit || *value > (limit - digit) / UINT64_C(10))
        return ZCL_OUT_OF_RANGE;
    *value = *value * UINT64_C(10) + digit;
    return ZCL_OK;
}

static zcl_status parse_whole(const uint8_t *text, size_t len,
                              size_t *position, uint64_t *whole)
{
    size_t index = 0;
    uint64_t value = 0;
    while (index < len && is_digit(text[index])) {
        zcl_status status = append_digit(&value, text[index], UINT64_C(21000000));
        if (status != ZCL_OK)
            return status;
        ++index;
    }
    if (index == 0 || (index > 1 && text[0] == (uint8_t)'0'))
        return ZCL_INVALID_ENCODING;
    *position = index;
    *whole = value;
    return ZCL_OK;
}

static zcl_status parse_fraction(const uint8_t *text, size_t len,
                                 size_t position, uint64_t *fraction)
{
    if (position == len) {
        *fraction = 0;
        return ZCL_OK;
    }
    if (text[position] != (uint8_t)'.')
        return ZCL_INVALID_ENCODING;
    ++position; /* position was strictly less than len, so this cannot wrap. */
    size_t count = len - position;
    if (count == 0 || count > 8)
        return ZCL_INVALID_ENCODING;
    uint64_t value = 0;
    for (size_t index = position; index < len; ++index) {
        if (!is_digit(text[index]))
            return ZCL_INVALID_ENCODING;
        zcl_status status = append_digit(&value, text[index], ZCL_ZATOSHIS_PER_COIN - 1);
        if (status != ZCL_OK)
            return status;
    }
    /* At most eight digits total; each multiplication stays below 10^8. */
    for (; count < 8; ++count)
        value *= UINT64_C(10);
    *fraction = value;
    return ZCL_OK;
}

zcl_status zcl_amount_parse(const uint8_t *text, size_t text_len, uint64_t *amount)
{
    if (text == NULL || amount == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (text_len == 0 || text_len > ZCL_AMOUNT_TEXT_MAX)
        return ZCL_INVALID_ENCODING;
    size_t position = 0;
    uint64_t whole = 0, fraction = 0;
    zcl_status status = parse_whole(text, text_len, &position, &whole);
    if (status != ZCL_OK)
        return status;
    status = parse_fraction(text, text_len, position, &fraction);
    if (status != ZCL_OK)
        return status;
    /* parse_whole bounded whole, so multiplication cannot overflow uint64_t. */
    uint64_t base = whole * ZCL_ZATOSHIS_PER_COIN;
    if (fraction > ZCL_MAX_MONEY - base)
        return ZCL_OUT_OF_RANGE;
    *amount = base + fraction;
    return ZCL_OK;
}

static size_t format_whole(uint64_t whole, uint8_t *output, size_t capacity)
{
    if (whole > UINT64_C(21000000))
        return 0;
    uint8_t reversed[8] = {0};
    size_t count = 0;
    /* Caller bounds whole to 21,000,000: this writes at most eight bytes. */
    do {
        reversed[count] = (uint8_t)((uint8_t)'0' + (uint8_t)(whole % UINT64_C(10)));
        ++count;
        whole /= UINT64_C(10);
    } while (whole != 0);
    if (count > capacity)
        return 0;
    for (size_t index = 0; index < count; ++index)
        output[index] = reversed[count - index - 1];
    return count;
}

static size_t format_fraction(uint64_t fraction, uint8_t *output, size_t capacity)
{
    if (capacity < 8 || fraction >= ZCL_ZATOSHIS_PER_COIN)
        return 0;
    uint64_t divisor = UINT64_C(10000000);
    for (size_t index = 0; index < 8; ++index) {
        output[index] = (uint8_t)((uint8_t)'0' + (uint8_t)(fraction / divisor));
        fraction %= divisor;
        divisor /= UINT64_C(10);
    }
    size_t count = 8;
    while (count > 0 && output[count - 1] == (uint8_t)'0')
        --count;
    return count;
}

zcl_status zcl_amount_format(uint64_t amount, uint8_t *text, size_t text_capacity,
                             size_t *text_len)
{
    if (text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (amount > ZCL_MAX_MONEY)
        return ZCL_OUT_OF_RANGE;
    uint8_t temporary[17] = {0};
    size_t count = format_whole(amount / ZCL_ZATOSHIS_PER_COIN, temporary, sizeof(temporary));
    if (count == 0)
        return ZCL_INVALID_ARGUMENT;
    uint64_t fraction = amount % ZCL_ZATOSHIS_PER_COIN;
    if (fraction != 0) {
        temporary[count++] = (uint8_t)'.';
        size_t fraction_len = format_fraction(fraction, temporary + count, sizeof(temporary) - count);
        if (fraction_len == 0)
            return ZCL_INVALID_ARGUMENT;
        count += fraction_len;
    }
    if (text_capacity < count)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(text, temporary, count);
    *text_len = count;
    return ZCL_OK;
}

zcl_status zcl_amount_delta_format(int64_t delta, uint8_t *text, size_t text_capacity,
                                  size_t *text_len)
{
    if (text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (delta < -(int64_t)ZCL_MAX_MONEY || delta > (int64_t)ZCL_MAX_MONEY)
        return ZCL_OUT_OF_RANGE;
    uint8_t temporary[18] = {0};
    size_t prefix = (size_t)(delta != 0);
    uint64_t magnitude = 0;
    if (delta < 0) {
        temporary[0] = (uint8_t)'-';
        magnitude = (uint64_t)(-delta); /* Range check excludes INT64_MIN. */
    } else {
        temporary[0] = (uint8_t)'+';
        magnitude = (uint64_t)delta;
    }
    size_t count = 0;
    /* prefix is zero or one; all arithmetic stays within the 18-byte array. */
    zcl_status status = zcl_amount_format(magnitude, temporary + prefix,
                                         sizeof(temporary) - prefix, &count);
    if (status != ZCL_OK)
        return status;
    count += prefix; /* The formatter returns at most 17 bytes. */
    if (text_capacity < count)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(text, temporary, count);
    *text_len = count;
    return ZCL_OK;
}

zcl_status zcl_amount_add(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (left > ZCL_MAX_MONEY || right > ZCL_MAX_MONEY - left)
        return ZCL_OUT_OF_RANGE;
    *result = left + right;
    return ZCL_OK;
}

zcl_status zcl_amount_subtract(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (left > ZCL_MAX_MONEY || right > left)
        return ZCL_OUT_OF_RANGE;
    *result = left - right;
    return ZCL_OK;
}
