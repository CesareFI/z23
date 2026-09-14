/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "utf8_reference.h"

_Static_assert(ZCL_PAYMENT_FIELD_MAX == 200, "Review the independent field bound");

static const uint32_t format_characters[] = {
#include "unicode_format_17.inc"
};
_Static_assert(sizeof(format_characters) / sizeof(format_characters[0]) == 170,
    "Unicode 17 complete Cf enumeration");

bool utf8_reference_forbidden(uint32_t code)
{
    if (code < 0x20 || (code >= 0x7f && code < 0xa0) || code == 0x2028 || code == 0x2029)
        return true;
    /* Independently enumerated Cf scalars, not compact production ranges. */
    size_t low = 0, high = sizeof(format_characters) / sizeof(format_characters[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (code == format_characters[middle]) return true;
        if (code < format_characters[middle]) high = middle;
        else low = middle + 1;
    }
    return false;
}

typedef struct { uint8_t first_low, first_high, second_low, second_high, length; } byte_rule;
static const byte_rule rules[] = {
    {0x00, 0x7f, 0x00, 0x00, 1}, {0xc2, 0xdf, 0x80, 0xbf, 2},
    {0xe0, 0xe0, 0xa0, 0xbf, 3}, {0xe1, 0xec, 0x80, 0xbf, 3},
    {0xed, 0xed, 0x80, 0x9f, 3}, {0xee, 0xef, 0x80, 0xbf, 3},
    {0xf0, 0xf0, 0x90, 0xbf, 4}, {0xf1, 0xf3, 0x80, 0xbf, 4},
    {0xf4, 0xf4, 0x80, 0x8f, 4}
};

static const byte_rule *match_lead(uint8_t first)
{
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i) {
        if (first >= rules[i].first_low && first <= rules[i].first_high) return &rules[i];
    }
    return NULL;
}

static bool match_tails(const uint8_t *text, const byte_rule *rule)
{
    for (size_t i = 1; i < rule->length; ++i) {
        const uint8_t low = i == 1 ? rule->second_low : 0x80;
        const uint8_t high = i == 1 ? rule->second_high : 0xbf;
        if (text[i] < low || text[i] > high) return false;
    }
    return true;
}

static uint32_t scalar(const uint8_t *text, size_t length)
{
    static const uint8_t prefix[] = {0, 0, 0xc0, 0xe0, 0xf0};
    uint32_t code = (uint32_t)text[0] - prefix[length];
    for (size_t i = 1; i < length; ++i) code = code * 64 + ((uint32_t)text[i] - 0x80);
    return code;
}

zcl_status utf8_reference_text(const uint8_t *text, size_t length)
{
    if (text == NULL || length > 200) return ZCL_INVALID_ARGUMENT;
    size_t offset = 0;
    while (offset < length) {
        const byte_rule *rule = match_lead(text[offset]);
        if (rule == NULL || rule->length > length - offset) return ZCL_INVALID_ENCODING;
        if (!match_tails(text + offset, rule)) return ZCL_INVALID_ENCODING;
        if (utf8_reference_forbidden(scalar(text + offset, rule->length))) return ZCL_INVALID_ENCODING;
        offset += rule->length;
    }
    return ZCL_OK;
}
