/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include "uri_text.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "payment check failed at line %d\n", __LINE__); return false; \
} } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1
#define ADDRESS "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
#define REQUEST "zclassic:" ADDRESS

static bool accepts_public_requests(void)
{
    zcl_payment_request request = {0};
    CHECK(zcl_payment_parse(TEXT(ADDRESS), ZCL_MAINNET, &request) == ZCL_OK);
    CHECK(!request.has_amount && !request.has_label && !request.has_message);
    CHECK(zcl_payment_parse(TEXT(REQUEST "?amount=1.00000001&label=Caf%C3%A9&message=Hello%20world"), ZCL_MAINNET, &request) == ZCL_OK);
    CHECK(request.has_amount && request.amount == UINT64_C(100000001));
    CHECK(request.has_label && request.label_len == 5 && memcmp(request.label, "Caf\xc3\xa9", 5) == 0);
    CHECK(request.has_message && request.message_len == 11 && memcmp(request.message, "Hello world", 11) == 0);
    CHECK(zcl_payment_parse(TEXT("ZCLASSIC:" ADDRESS "?label=a+b"), ZCL_MAINNET, &request) == ZCL_OK);
    CHECK(request.label_len == 3 && memcmp(request.label, "a+b", 3) == 0);
    CHECK(zcl_payment_parse(TEXT(REQUEST "?label=&message="), ZCL_MAINNET, &request) == ZCL_OK);
    CHECK(request.has_label && request.has_message && request.label_len == 0 && request.message_len == 0);
    return true;
}

static bool rejects_unsafe_requests(void)
{
    static const struct { const char *text; size_t length; } invalid[] = {
#define CASE(s) {s, sizeof(s) - 1}
        CASE(REQUEST "?amount=1&amount=2"), CASE(REQUEST "?amount=1&%61mount=2"),
        CASE(REQUEST "?amount=0"), CASE(REQUEST "?amount=-1"), CASE(REQUEST "?amount=1e6"),
        CASE(REQUEST "?req-feature=yes"), CASE(REQUEST "?r=https://example.com"),
        CASE(REQUEST "?amount.1=2"), CASE(REQUEST "?label=%00"), CASE(REQUEST "?label=%0a"),
        CASE(REQUEST "?label=%E2%80%AEhidden"), CASE(REQUEST "?label=%ff"),
        CASE(REQUEST "?label=%c0%af"), CASE(REQUEST "?label=%"),
        CASE(REQUEST "?label=%ED%A0%80"), CASE(REQUEST "?label=%F4%90%80%80"),
        CASE(REQUEST "?label=%E2%82"), CASE(REQUEST "?label=x&&message=y"),
        CASE(REQUEST "?label"), CASE(REQUEST "?"), CASE(REQUEST "#fragment"),
        CASE("zcash:" ADDRESS), CASE("bitcoin:" ADDRESS), CASE("zclassic://" ADDRESS),
        CASE(REQUEST "?label=raw space"), CASE(REQUEST "?label=raw\xc3\xa9"),
        CASE(REQUEST "?label=a&message=b&amount=1&label=c"), CASE(ADDRESS "\n")
#undef CASE
    };
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        zcl_payment_request request = {0};
        request.amount = UINT64_C(42);
        CHECK(zcl_payment_parse((const uint8_t *)invalid[index].text, invalid[index].length, ZCL_MAINNET, &request) != ZCL_OK);
        CHECK(request.amount == UINT64_C(42) && !request.has_amount);
    }
    return true;
}

static bool field_and_argument_bounds(void)
{
    uint8_t text[1025] = {0};
    static const uint8_t prefix[] = REQUEST "?label=";
    memcpy(text, prefix, sizeof(prefix) - 1);
    memset(text + sizeof(prefix) - 1, (int)'x', 201);
    zcl_payment_request request = {0};
    CHECK(zcl_payment_parse(text, sizeof(prefix) - 1 + 200, ZCL_MAINNET, &request) == ZCL_OK);
    CHECK(request.label_len == 200);
    CHECK(zcl_payment_parse(text, sizeof(prefix) - 1 + 201, ZCL_MAINNET, &request) != ZCL_OK);
    CHECK(zcl_payment_parse(text, SIZE_MAX, ZCL_MAINNET, &request) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_payment_parse(NULL, 0, ZCL_MAINNET, &request) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_payment_parse(TEXT(REQUEST), ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_payment_parse(TEXT(REQUEST), ZCL_TESTNET, &request) != ZCL_OK);
    uint8_t output[200] = {0};
    size_t length = SIZE_MAX;
    CHECK(zcl_uri_decode_field(TEXT("abc"), output, 2, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == SIZE_MAX && output[0] == 0);
    return true;
}

static size_t encode_codepoint(uint32_t code, uint8_t *bytes, size_t capacity)
{
    if (capacity < 4 || code > 0x10ffff)
        return 0;
    if (code < 0x800) {
        bytes[0] = (uint8_t)(0xc0U | (code >> 6));
        bytes[1] = (uint8_t)(0x80U | (code & 0x3fU));
        return 2;
    }
    if (code < 0x10000) {
        bytes[0] = (uint8_t)(0xe0U | (code >> 12));
        bytes[1] = (uint8_t)(0x80U | ((code >> 6) & 0x3fU));
        bytes[2] = (uint8_t)(0x80U | (code & 0x3fU));
        return 3;
    }
    bytes[0] = (uint8_t)(0xf0U | (code >> 18));
    bytes[1] = (uint8_t)(0x80U | ((code >> 12) & 0x3fU));
    bytes[2] = (uint8_t)(0x80U | ((code >> 6) & 0x3fU));
    bytes[3] = (uint8_t)(0x80U | (code & 0x3fU));
    return 4;
}

static bool rejects_unicode_format_characters(void)
{
    /* Independent complete Cf enumeration from UnicodeData 17.0.0, against
     * the compact range table used by the production parser. */
    static const uint32_t format_characters[] = {
#include "unicode_format_17.inc"
    };
    for (size_t index = 0; index < sizeof(format_characters) / sizeof(format_characters[0]); ++index) {
        uint8_t encoded[4] = {0};
        size_t length = encode_codepoint(format_characters[index], encoded, sizeof(encoded));
        CHECK(length > 0);
        CHECK(zcl_utf8_visible_text(encoded, length) == ZCL_INVALID_ENCODING);
    }
    return true;
}

int main(void)
{
    if (!accepts_public_requests() || !rejects_unsafe_requests() || !field_and_argument_bounds() ||
        !rejects_unicode_format_characters())
        return 1;
    return puts("wallet-core: 4 payment test groups passed") == EOF ? 1 : 0;
}
