/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "rpc_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { \
    fprintf(stderr, "ASCII decoder comparison failed at %d\n", __LINE__); abort(); \
} } while (0)

/* Single-threaded fixture owns all large buffers. The oracle always invokes
 * the original provider, including on plain ASCII; no optimized helper reuse. */
static struct {
    uint8_t source[ZCL_RPC_JSON_MAX], before[ZCL_RPC_JSON_MAX];
    uint8_t actual[ZCL_RPC_JSON_MAX + 2], expected[ZCL_RPC_JSON_MAX + 2];
    size_t length;
} fixture;

static zcl_status reference(const zcl_rpc_json *doc, const zcl_rpc_token *token,
                            uint8_t *output, size_t capacity, size_t *length)
{
    const zjsonp_event event = {(zjsonp_event_kind)token->kind, token->offset, token->length};
    const size_t count = zjsonp_str_decode((const char *)doc->text, &event, (char *)output, capacity);
    if (count == SIZE_MAX) return ZCL_INVALID_ENCODING;
    if (count > capacity) return ZCL_BUFFER_TOO_SMALL;
    for (size_t i = 0; i < count; ++i) {
        if (output[i] < 0x20 || output[i] > 0x7e) return ZCL_INVALID_ENCODING;
    }
    *length = count;
    return ZCL_OK;
}

static zcl_status compare(const uint8_t *text, size_t length, size_t capacity, uint8_t kind)
{
    CHECK(length <= ZCL_RPC_JSON_MAX - 3 && capacity <= ZCL_RPC_JSON_MAX);
    memset(fixture.source, 0xa5, sizeof(fixture.source));
    memcpy(fixture.source + 3, text, length);
    memcpy(fixture.before, fixture.source, sizeof(fixture.before));
    memset(fixture.actual, 0xa5, sizeof(fixture.actual));
    memset(fixture.expected, 0xa5, sizeof(fixture.expected));
    fixture.length = SIZE_MAX;
    size_t expected_length = SIZE_MAX;
    const zcl_rpc_json doc = {.text = fixture.source, .length = length + 3};
    const zcl_rpc_token token = {3, (uint16_t)length, 0, kind};
    const zcl_status expected = reference(&doc, &token, fixture.expected + 1, capacity, &expected_length);
    const zcl_status actual = zcl_rpc_ascii(&doc, &token, fixture.actual + 1, capacity, &fixture.length);
    CHECK(actual == expected && fixture.length == expected_length);
    CHECK(memcmp(fixture.actual, fixture.expected, sizeof(fixture.actual)) == 0);
    CHECK(memcmp(fixture.before, fixture.source, sizeof(fixture.source)) == 0);
    return actual;
}

#ifdef ZCL_RPC_ASCII_FUZZ
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_RPC_JSON_MAX - 3) return 0;
    const size_t capacity = size == 0 ? 0 : (size_t)data[0] % (size + 1);
    (void)compare(data, size, capacity, ZJRP_KEY);
    (void)compare(data, size, size, ZJRP_STR);
    return 0;
}
#else
static void capacities(const uint8_t *text, size_t length)
{
    const size_t sizes[] = {0, 1, 2, 32, length / 2, length, length + 1};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        (void)compare(text, length, sizes[i], ZJRP_KEY);
        (void)compare(text, length, sizes[i], ZJRP_STR);
    }
}

static void raw_bytes(void)
{
    uint8_t text[] = {'a', 0, 'b'};
    for (unsigned byte = 0; byte <= UINT8_MAX; ++byte) {
        text[1] = (uint8_t)byte;
        capacities(text, sizeof(text));
        capacities(text + 1, 1);
    }
}

static void escaped(void)
{
    static const char *cases[] = {
        "", "result", "r\\u0065sult", "\\\"", "\\\\", "\\/",
        "\\b", "\\f", "\\n", "\\r", "\\t", "\\u0000", "\\u007e",
        "\\u007f", "\\u0080", "\\ud800\\udc00", "\\ud83d\\ude00",
        "\\ud800", "\\udc00", "\\u", "\\uxxxx", "prefix\\q", "prefix\\",
        "a\\u0062c\\n", "\xc2\xa2", "\xe2\x82\xac", "\xf0\x9f\x98\x80"
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        capacities((const uint8_t *)cases[i], strlen(cases[i]));
}

static void large_spans(void)
{
    static uint8_t text[ZCL_RPC_JSON_MAX - 3];
    const size_t lengths[] = {0, 1, 2, 15, 255, 256, 257, 2974, sizeof(text)};
    memset(text, 'x', sizeof(text));
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        capacities(text, lengths[i]);
    text[sizeof(text) - 1] = '\\';
    capacities(text, sizeof(text));
}

static void known_results(void)
{
    CHECK(compare((const uint8_t *)"result", 6, 6, ZJRP_STR) == ZCL_OK);
    CHECK(fixture.length == 6 && memcmp(fixture.actual + 1, "result", 6) == 0);
    CHECK(compare((const uint8_t *)"result", 6, 5, ZJRP_STR) == ZCL_BUFFER_TOO_SMALL);
    CHECK(fixture.length == SIZE_MAX && memcmp(fixture.actual + 1, "resul", 5) == 0);
    CHECK(compare((const uint8_t *)"r\\u0065sult", 11, 6, ZJRP_STR) == ZCL_OK);
    CHECK(fixture.length == 6 && memcmp(fixture.actual + 1, "result", 6) == 0);
    CHECK(compare((const uint8_t *)"\\n", 2, 0, ZJRP_STR) == ZCL_BUFFER_TOO_SMALL);
    CHECK(compare((const uint8_t *)"\\n", 2, 1, ZJRP_STR) == ZCL_INVALID_ENCODING);
    CHECK(fixture.length == SIZE_MAX && fixture.actual[1] == '\n');
}

/* Reject or decode a late exceptional byte even when the output fills before
 * that byte. The plain path must qualify the whole input, not only its copy. */
static void late_bytes(void)
{
    static uint8_t text[ZCL_RPC_JSON_MAX - 3];
    const size_t lengths[] = {256, 2974, sizeof(text)};
    const uint8_t exceptional[] = {0, '\n', '\\', 0x7f, 0x80, 0xff};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        for (size_t j = 0; j < sizeof(exceptional); ++j) {
            memset(text, 'x', lengths[i]);
            text[lengths[i] - 1] = exceptional[j];
            capacities(text, lengths[i]);
        }
    }
}

static void invalid_span(zcl_rpc_json *doc, zcl_rpc_token *token)
{
    uint8_t output[4] = {1, 2, 3, 4};
    const uint8_t before[4] = {1, 2, 3, 4};
    size_t length = SIZE_MAX;
    CHECK(zcl_rpc_ascii(doc, token, output, sizeof(output), &length) == ZCL_INVALID_ENCODING);
    CHECK(length == SIZE_MAX && memcmp(output, before, sizeof(output)) == 0);
}

static void invalid_arguments(void)
{
    zcl_rpc_json doc = {.text = (const uint8_t *)"abc", .length = 3};
    zcl_rpc_token token = {0, 3, 0, ZJRP_STR};
    uint8_t output[4] = {0};
    size_t length = SIZE_MAX;
    CHECK(zcl_rpc_ascii(NULL, &token, output, sizeof(output), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_rpc_ascii(&doc, NULL, output, sizeof(output), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_rpc_ascii(&doc, &token, NULL, sizeof(output), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_rpc_ascii(&doc, &token, output, sizeof(output), NULL) == ZCL_INVALID_ARGUMENT);
    token.kind = ZJRP_NUM;
    invalid_span(&doc, &token);
    token.kind = ZJRP_STR;
    token.offset = 4;
    invalid_span(&doc, &token);
    token.offset = UINT16_MAX;
    invalid_span(&doc, &token);
    token.offset = 1;
    invalid_span(&doc, &token);
    token.offset = 0;
    doc.text = NULL;
    invalid_span(&doc, &token);
}

int main(void)
{
    known_results(); raw_bytes(); escaped(); large_spans(); late_bytes(); invalid_arguments();
    puts("ASCII/provider equivalence, capacities, escapes, byte boundaries and input/output guards passed");
    return 0;
}
#endif
