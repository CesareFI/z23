/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "uri_text.h"

/* Unicode 17.0.0 UnicodeData.txt General_Category=Cf, Cc, Zl and Zp. This is a
 * conservative request-display policy, not a complete Unicode spoof detector. */
static bool control_format_or_separator(uint32_t code)
{
    static const uint32_t ranges[][2] = {
        {0x0, 0x1f}, {0x7f, 0x9f}, {0xad, 0xad}, {0x600, 0x605},
        {0x61c, 0x61c}, {0x6dd, 0x6dd}, {0x70f, 0x70f}, {0x890, 0x891},
        {0x8e2, 0x8e2}, {0x180e, 0x180e}, {0x200b, 0x200f}, {0x2028, 0x202e},
        {0x2060, 0x2064}, {0x2066, 0x206f}, {0xfeff, 0xfeff}, {0xfff9, 0xfffb},
        {0x110bd, 0x110bd}, {0x110cd, 0x110cd}, {0x13430, 0x1343f},
        {0x1bca0, 0x1bca3}, {0x1d173, 0x1d17a}, {0xe0001, 0xe0001}, {0xe0020, 0xe007f}
    };
    for (size_t index = 0; index < sizeof(ranges) / sizeof(ranges[0]); ++index) {
        /* Ascending ranges cannot contain a value below this lower bound. */
        if (code < ranges[index][0])
            return false;
        if (code <= ranges[index][1])
            return true;
    }
    return false;
}

static size_t sequence_size(uint8_t lead)
{
    if (lead < 0x80)
        return 1;
    if (lead >= 0xc2 && lead <= 0xdf)
        return 2;
    if (lead >= 0xe0 && lead <= 0xef)
        return 3;
    if (lead >= 0xf0 && lead <= 0xf4)
        return 4;
    return 0;
}

static zcl_status read_codepoint(const uint8_t *text, size_t remaining,
                                  uint32_t *code, size_t *consumed)
{
    static const uint8_t masks[] = {0, 0x7f, 0x1f, 0x0f, 0x07};
    static const uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
    size_t count = sequence_size(text[0]);
    if (count == 0 || count > remaining)
        return ZCL_INVALID_ENCODING;
    uint32_t value = (uint32_t)(text[0] & masks[count]);
    for (size_t index = 1; index < count; ++index) {
        if ((text[index] & 0xc0) != 0x80)
            return ZCL_INVALID_ENCODING;
        value = (value << 6) | (uint32_t)(text[index] & 0x3f);
    }
    if (value < minimum[count] || value > 0x10ffff)
        return ZCL_INVALID_ENCODING;
    if (value >= 0xd800 && value <= 0xdfff)
        return ZCL_INVALID_ENCODING;
    *code = value;
    *consumed = count;
    return ZCL_OK;
}

zcl_status zcl_utf8_visible_text(const uint8_t *text, size_t length)
{
    if (text == NULL || length > ZCL_PAYMENT_FIELD_MAX)
        return ZCL_INVALID_ARGUMENT;
    size_t position = 0;
    while (position < length) {
        uint32_t code = 0;
        size_t consumed = 0;
        zcl_status status = read_codepoint(text + position, length - position, &code, &consumed);
        if (status != ZCL_OK)
            return status;
        if (control_format_or_separator(code))
            return ZCL_INVALID_ENCODING;
        position += consumed;
    }
    return ZCL_OK;
}
