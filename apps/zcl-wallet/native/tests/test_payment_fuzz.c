/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

static void exercise(const uint8_t *bytes, size_t length)
{
    uint8_t before[ZCL_PAYMENT_TEXT_MAX] = {0};
    assert(length <= sizeof(before));
    if (length != 0) memcpy(before, bytes, length);
    assert(LLVMFuzzerTestOneInput(bytes, length) == 0);
    if (length != 0) assert(memcmp(before, bytes, length) == 0);
}

static void byte_and_length_boundaries(void)
{
    uint8_t bytes[ZCL_PAYMENT_TEXT_MAX] = {0};
    static const size_t lengths[] = {0, 1, 2, 3, 4, 199, 200, 201, ZCL_PAYMENT_TEXT_MAX};
    for (unsigned value = 0; value <= 255; ++value) {
        memset(bytes, (int)value, sizeof(bytes));
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
            exercise(bytes, lengths[i]);
    }
}

static void unicode_and_uri_controls(void)
{
    /* Visible ASCII, URI delimiters and 2/3/4-byte UTF-8; also forbidden
     * controls, malformed/overlong sequences, surrogates and truncations. */
    static const uint8_t text[][24] = {
        "invoice", " &=%?#", "caf\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x92\xb0",
        "line\nfeed", "\xe2\x80\xae", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"
    };
    static const size_t sizes[] = {7, 6, 5, 3, 4, 9, 3, 2, 3, 4};
    _Static_assert(sizeof(text) / sizeof(text[0]) == sizeof(sizes) / sizeof(sizes[0]),
        "Every fixture has an explicit byte length");
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
        for (size_t length = 0; length <= sizes[i]; ++length)
            exercise(text[i], length);

    static const uint8_t raw[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?label=invoice&amount=1.25";
    exercise(raw, sizeof(raw) - 1);
    exercise(NULL, 0);
}

int main(void)
{
    byte_and_length_boundaries();
    unicode_and_uri_controls();
    return puts("Payment fuzz: exact labels, live acceptance and hostile boundary replay passed") < 0 ? 1 : 0;
}
