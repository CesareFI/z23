/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "uri_text.h"

#include <string.h>

zcl_status zcl_uri_validate_text(const uint8_t *text, size_t length)
{
    if (text == NULL || length > ZCL_PAYMENT_TEXT_MAX)
        return ZCL_INVALID_ARGUMENT;
    static const uint8_t forbidden[] = "\"<>\\^`{|}#";
    for (size_t index = 0; index < length; ++index) {
        if (text[index] < 0x21 || text[index] > 0x7e)
            return ZCL_INVALID_ENCODING;
        for (size_t bad = 0; bad < sizeof(forbidden) - 1; ++bad) {
            if (text[index] == forbidden[bad])
                return ZCL_INVALID_ENCODING;
        }
    }
    return ZCL_OK;
}

static int hex_digit(uint8_t byte)
{
    if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9')
        return (int)(byte - (uint8_t)'0');
    if (byte >= (uint8_t)'A' && byte <= (uint8_t)'F')
        return (int)(byte - (uint8_t)'A') + 10;
    if (byte >= (uint8_t)'a' && byte <= (uint8_t)'f')
        return (int)(byte - (uint8_t)'a') + 10;
    return -1;
}

static zcl_status decoded_byte(const uint8_t *raw, size_t remaining,
                                uint8_t *byte, size_t *consumed)
{
    if (raw[0] != (uint8_t)'%') {
        *byte = raw[0];
        *consumed = 1;
        return ZCL_OK;
    }
    if (remaining < 3)
        return ZCL_INVALID_ENCODING;
    int high = hex_digit(raw[1]), low = hex_digit(raw[2]);
    if (high < 0 || low < 0)
        return ZCL_INVALID_ENCODING;
    *byte = (uint8_t)((unsigned)high * 16U + (unsigned)low);
    *consumed = 3;
    return ZCL_OK;
}

static zcl_status decode_bytes(const uint8_t *raw, size_t raw_len,
                               uint8_t decoded[ZCL_PAYMENT_TEXT_MAX], size_t *length)
{
    size_t position = 0;
    size_t count = 0;
    while (position < raw_len) {
        if (count == ZCL_PAYMENT_TEXT_MAX)
            return ZCL_OUT_OF_RANGE;
        size_t consumed = 0;
        zcl_status status = decoded_byte(raw + position, raw_len - position,
                                         decoded + count, &consumed);
        if (status != ZCL_OK)
            return status;
        ++count;
        position += consumed;
    }
    *length = count;
    return ZCL_OK;
}

zcl_status zcl_uri_decode_field(const uint8_t *raw, size_t raw_len,
                               uint8_t *output, size_t capacity, size_t *length)
{
    if (raw == NULL || output == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (raw_len > ZCL_PAYMENT_TEXT_MAX)
        return ZCL_OUT_OF_RANGE;
    uint8_t temporary[ZCL_PAYMENT_TEXT_MAX] = {0};
    size_t count = 0;
    zcl_status status = decode_bytes(raw, raw_len, temporary, &count);
    if (status != ZCL_OK)
        return status;
    if (count > capacity)
        return ZCL_BUFFER_TOO_SMALL;
    status = zcl_utf8_visible_text(temporary, count);
    if (status != ZCL_OK)
        return status;
    memcpy(output, temporary, count);
    *length = count;
    return ZCL_OK;
}
