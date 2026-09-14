/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "utf8_reference.h"
#include "uri_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "UTF-8 reference failed at %d\n", __LINE__); abort(); } } while (0)

/* Caller chooses width explicitly to test canonical and overlong forms. */
static void encode(uint32_t code, size_t width, uint8_t *text, size_t capacity)
{
    static const uint8_t prefixes[] = {0, 0, 0xc0, 0xe0, 0xf0};
    CHECK(text != NULL && capacity >= 4 && width >= 1 && width <= 4 && code <= 0x1fffff);
    for (size_t i = width; i > 1; --i) {
        text[i - 1] = (uint8_t)(0x80 + code % 64);
        code /= 64;
    }
    CHECK(code <= (uint32_t)(255 - prefixes[width]));
    text[0] = (uint8_t)(prefixes[width] + code);
}

static size_t canonical_width(uint32_t code)
{
    if (code < 0x80) return 1;
    if (code < 0x800) return 2;
    return code < 0x10000 ? 3 : 4;
}

static void compare(const uint8_t *text, size_t length, zcl_status expected)
{
    CHECK(utf8_reference_text(text, length) == expected);
    CHECK(zcl_utf8_visible_text(text, length) == expected);
}

static void every_codepoint(void)
{
    size_t refused = 0;
    for (uint32_t code = 0; code <= 0x10ffff; ++code) {
        uint8_t text[4] = {0};
        const size_t width = canonical_width(code);
        encode(code, width, text, sizeof(text));
        const bool invalid = (code >= 0xd800 && code <= 0xdfff) || utf8_reference_forbidden(code);
        compare(text, width, invalid ? ZCL_INVALID_ENCODING : ZCL_OK);
        if (invalid) ++refused;
    }
    CHECK(refused == 2048 + 65 + 170 + 2); /* Surrogates, Cc, Cf, Zl/Zp. */
}

static void noncanonical_forms(void)
{
    for (uint32_t code = 0; code < 0x10000; ++code) {
        for (size_t width = canonical_width(code) + 1; width <= 4; ++width) {
            uint8_t text[4] = {0};
            encode(code, width, text, sizeof(text));
            compare(text, width, ZCL_INVALID_ENCODING);
        }
    }
    /* Four-byte bit patterns beyond the Unicode scalar ceiling, including F5..F7. */
    for (uint32_t code = 0x110000; code <= 0x1fffff; ++code) {
        uint8_t text[4] = {0};
        encode(code, 4, text, sizeof(text));
        compare(text, sizeof(text), ZCL_INVALID_ENCODING);
    }
}

static void every_short_prefix(void)
{
    for (unsigned first = 0; first <= 255; ++first) {
        for (unsigned second = 0; second <= 255; ++second) {
            const uint8_t text[4] = {(uint8_t)first, (uint8_t)second, 0x80, 0x80};
            for (size_t length = 1; length <= sizeof(text); ++length)
                compare(text, length, utf8_reference_text(text, length));
        }
    }
}

static void field_boundaries(void)
{
    uint8_t text[201];
    memset(text, 'a', sizeof(text));
    compare(text, 0, ZCL_OK);
    compare(NULL, 0, ZCL_INVALID_ARGUMENT);
    compare(text, sizeof(text), ZCL_INVALID_ARGUMENT);
    compare(text, SIZE_MAX, ZCL_INVALID_ARGUMENT);
    compare(text, 200, ZCL_OK);
    /* Boundary-aligned U+1F642, then every incomplete suffix. */
    encode(0x1f642, 4, text + 196, sizeof(text) - 196);
    compare(text, 200, ZCL_OK);
    for (size_t length = 197; length < 200; ++length) compare(text, length, ZCL_INVALID_ENCODING);
    const uint8_t tail[] = {0x00, 0x7f, 0xc0, 0xc1, 0xf5, 0xff};
    for (size_t i = 0; i < sizeof(tail); ++i) {
        text[199] = tail[i];
        compare(text, 200, ZCL_INVALID_ENCODING);
    }
}

int main(void)
{
    every_codepoint();
    noncanonical_forms();
    every_short_prefix();
    field_boundaries();
    puts("Independent UTF-8: all codepoints, overlong forms, invalid ceiling and short prefixes passed");
    return 0;
}
