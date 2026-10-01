/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "rpc_json.h"
#include "electrum_genesis_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Electrum check failed at %d\n", __LINE__); abort(); } } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1
static char frame[5000];
static zcl_electrum_line line;
static const char main_hash[] = "0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602";
static const char test_hash[] = "03e1c4bb705c871bf9bfda3e74b7f8f86bff267993c215a89d5795e3708e5e1f";

static void requests(void)
{
    uint8_t hash[20] = {0}, address[35], script_hash[66], request[258];
    size_t length = 0;
    CHECK(zcl_address_from_hash(hash, sizeof(hash), ZCL_MAINNET, address, sizeof(address), &length) == ZCL_OK);
    memset(script_hash, 0xa5, sizeof(script_hash));
    CHECK(zcl_electrum_script_hash(address, length, ZCL_MAINNET, script_hash + 1, 64) == ZCL_OK);
    /* Independent OpenSSL SHA-256 of 76a914 + 20 zero bytes + 88ac, reversed. */
    static const char expected[] = "acb87996319dca2c2e2afd6c0f7514b18e72e204069718976e1abdc8fcf5de75";
    CHECK(memcmp(script_hash + 1, expected, 64) == 0);
    CHECK(script_hash[0] == 0xa5 && script_hash[65] == 0xa5);
    for (int method = ZCL_ELECTRUM_VERSION; method <= ZCL_ELECTRUM_HISTORY; ++method) {
        memset(request, 0xa5, sizeof(request));
        CHECK(zcl_electrum_request((zcl_electrum_method)method, UINT32_MAX, address, sizeof(address),
                                    ZCL_MAINNET, request + 1, 256, &length) == ZCL_OK);
        CHECK(length < 256 && request[0] == 0xa5 && request[length + 1] == 0xa5);
        CHECK(request[length] == '\n' && memchr(request + 1, 0, length) == NULL);
        CHECK(memcmp(request + 1, "{\"id\":4294967295,", 17) == 0);
    }
    memset(request, 0xa5, sizeof(request));
    length = 99;
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_BALANCE, 1, address, sizeof(address), ZCL_TESTNET,
                                request, sizeof(request), &length) != ZCL_OK);
    CHECK(length == 99 && request[0] == 0xa5);
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_VERSION, 0, NULL, 0, ZCL_MAINNET, request, sizeof(request), &length) != ZCL_OK);
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_VERSION, 1, NULL, 0, ZCL_MAINNET, request, 1, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == 99 && request[0] == 0xa5);
}

static void refused_balance(const char *text)
{
    zcl_reported_balance output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    CHECK(zcl_electrum_balance_reply((const uint8_t *)text, strlen(text), 1, &output) != ZCL_OK);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
}

static void balance_values(void)
{
    zcl_reported_balance balance = {0};
    CHECK(zcl_electrum_balance_reply(TEXT("{\"result\":{\"unconfirmed\":-125,\"confirmed\":500},\"id\":1,\"error\":null}"), 1, &balance) == ZCL_OK);
    CHECK(balance.confirmed == 500 && balance.pending_delta == -125 && balance.total == 375);
    CHECK(zcl_electrum_balance_reply(TEXT("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":2100000000000000}}"), 1, &balance) == ZCL_OK);
    CHECK(balance.total == ZCL_MAX_MONEY && balance.confirmed == 0);
    static const char *bad[] = {
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":-1}}",
        "{\"id\":1,\"result\":{\"confirmed\":2100000000000000,\"unconfirmed\":1}}",
        "{\"id\":1,\"result\":{\"confirmed\":1e2,\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":{\"confirmed\":1.0,\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":{\"confirmed\":\"1\",\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":{\"confirmed\":-0,\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":9223372036854775808}}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":-9223372036854775808}}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":-9223372036854775809}}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"confirmed\":100,\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0,\"extension\":{\"a\":1,\"a\":2}}}",
        "{\"id\":1,\"\\u0069d\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0}}",
        "{\"id\":2,\"result\":{\"confirmed\":0,\"unconfirmed\":0}}",
        "{\"id\":1,\"method\":\"notification\",\"result\":{\"confirmed\":0,\"unconfirmed\":0}}",
        "{\"id\":1,\"error\":{\"code\":-1},\"result\":{\"confirmed\":0,\"unconfirmed\":0}}",
        "{\"id\":1,\"result\":null}", "{\"id\":1,\"result\":[]}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0}} {}",
        "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0,}}"
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) refused_balance(bad[i]);
}

static void identity(void)
{
    CHECK(zcl_electrum_version_reply(TEXT("{\"id\":1,\"result\":[\"ElectrumX fixture\",\"1.2\"]}"), 1) == ZCL_OK);
    CHECK(zcl_electrum_version_reply(TEXT("{\"id\":1,\"result\":[\"ElectrumX fixture\",\"1.4\"]}"), 1) == ZCL_UNSUPPORTED);
    CHECK(zcl_electrum_version_reply(TEXT("{\"id\":1,\"result\":[\"x\",\"1.2\",null]}"), 1) != ZCL_OK);
    CHECK(zcl_electrum_version_reply(TEXT("{\"id\":1,\"result\":[\"x\",\"1.2\"]}"), 2) != ZCL_OK);
    for (int chain = 0; chain < 2; ++chain) {
        int n = snprintf(frame, sizeof(frame), "{\"id\":2,\"result\":{\"genesis_hash\":\"%s\",\"hash_function\":\"sha256\",\"hosts\":{\"example.invalid\":{\"ssl_port\":50002}}}}",
            chain == 0 ? main_hash : test_hash);
        CHECK(n > 0 && (size_t)n < sizeof(frame));
        CHECK(zcl_electrum_features_reply((const uint8_t *)frame, (size_t)n, 2, (zcl_network)chain) == ZCL_OK);
        CHECK(zcl_electrum_features_reply((const uint8_t *)frame, (size_t)n, 2, (zcl_network)(1 - chain)) != ZCL_OK);
    }
}

static void headers(void)
{
    for (int chain = 0; chain < 2; ++chain) {
        const zcl_network network = (zcl_network)chain;
        const char *hex = chain == 0 ? main_genesis : test_genesis;
        int n = snprintf(frame, sizeof(frame), "{\"id\":3,\"result\":{\"count\":1,\"max\":2016,\"hex\":\"%s\"}}", hex);
        CHECK(n > 0 && (size_t)n < sizeof(frame));
        CHECK(zcl_electrum_genesis_reply((const uint8_t *)frame, (size_t)n, 3, network) == ZCL_OK);
        CHECK(zcl_electrum_genesis_reply((const uint8_t *)frame, (size_t)n, 3, (zcl_network)(1 - chain)) != ZCL_OK);
        n = snprintf(frame, sizeof(frame), "{\"id\":4,\"result\":{\"height\":0,\"hex\":\"%s\"}}", hex);
        CHECK(n > 0 && (size_t)n < sizeof(frame));
        zcl_reported_tip tip = {0};
        CHECK(zcl_electrum_tip_reply((const uint8_t *)frame, (size_t)n, 4, network, &tip) == ZCL_OK);
        uint8_t expected[32];
        CHECK(zcl_network_genesis(network, expected, sizeof(expected)) == ZCL_OK);
        CHECK(tip.height == 0 && memcmp(tip.hash, expected, 32) == 0);
        char *header = strstr(frame, "04000000");
        CHECK(header != NULL);
        header[280] = 'f'; header[281] = 'e'; /* Noncanonical solution length. */
        zcl_reported_tip before = tip;
        CHECK(zcl_electrum_tip_reply((const uint8_t *)frame, (size_t)n, 4, network, &tip) != ZCL_OK);
        CHECK(memcmp(&before, &tip, sizeof(tip)) == 0);
    }
}

static void json_bounds(void)
{
    zcl_rpc_json doc;
    static const char *bad[] = {"", "[]", "{]", "{\"x\":01}", "{\"x\":1e}", "{\"x\":[1,]}",
        "{\"x\":\"\\ud800\"}", "{\"x\":\"\\udc00\"}", "{\"x\":\"\\ud800\\u0041\"}",
        "{\"x\":\"\xc0\xaf\"}", "{\"x\":\"a\n\"}", "{\"\xc3\xa9\":1}", "{\"\\u00e9\":1}",
        "{\"x\":[[[[[[[[]]]]]]]]}", "{\"x\":\"\\x\"}"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        CHECK(zcl_rpc_json_parse((const uint8_t *)bad[i], strlen(bad[i]), &doc) != ZCL_OK);
    CHECK(zcl_rpc_json_parse(TEXT("{\"x\":\"\xc3\xa9\",\"other\":\"\\ud83d\\ude00\"}"), &doc) == ZCL_OK);
    CHECK(zcl_rpc_json_parse(TEXT("{\"x\":[],\"y\":{}}"), &doc) == ZCL_OK);
    CHECK(zcl_rpc_json_parse(NULL, 1, &doc) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_rpc_json_parse((const uint8_t *)frame, SIZE_MAX, &doc) == ZCL_OUT_OF_RANGE);
    memset(frame, ' ', sizeof(frame));
    frame[0] = '{'; frame[1] = '"'; frame[2] = 'a'; frame[3] = '"'; frame[4] = ':'; frame[5] = '[';
    for (size_t i = 0; i < 200; ++i) { frame[6 + i * 2] = '0'; frame[7 + i * 2] = ','; }
    frame[405] = ']'; frame[406] = '}';
    CHECK(zcl_rpc_json_parse((const uint8_t *)frame, 407, &doc) == ZCL_RESOURCE_EXHAUSTED);
}

static void fork_headers(void)
{
    /* Synthetic serialization fixture, not a valid Equihash solution. */
    char hex[1087];
    memset(hex, '0', sizeof(hex) - 1);
    hex[sizeof(hex) - 1] = 0;
    memcpy(hex, "04000000", 8);
    memcpy(hex + 280, "fd9001", 6);
    for (int chain = 0; chain < 2; ++chain) {
        const uint32_t fork = chain == 0 ? 585318 : 6350;
        const uint32_t heights[] = {fork - 1, fork, fork + 1, INT32_MAX};
        for (size_t i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
            int n = snprintf(frame, sizeof(frame), "{\"id\":1,\"result\":{\"height\":%u,\"hex\":\"%s\"}}", heights[i], hex);
            CHECK(n > 0 && (size_t)n < sizeof(frame));
            zcl_reported_tip tip = {0};
            zcl_status status = zcl_electrum_tip_reply((const uint8_t *)frame, (size_t)n, 1, (zcl_network)chain, &tip);
            CHECK((status == ZCL_OK) == (i != 0));
            if (status == ZCL_OK) CHECK(tip.height == heights[i]);
        }
        int n = snprintf(frame, sizeof(frame), "{\"id\":1,\"result\":{\"height\":%u,\"hex\":\"%s\"}}", fork, main_genesis);
        CHECK(n > 0 && (size_t)n < sizeof(frame));
        zcl_reported_tip tip = {0};
        CHECK(zcl_electrum_tip_reply((const uint8_t *)frame, (size_t)n, 1, (zcl_network)chain, &tip) != ZCL_OK);
    }
    zcl_reported_tip tip = {0};
    CHECK(zcl_electrum_tip_reply(TEXT("{\"id\":1,\"result\":{\"height\":2147483648,\"hex\":\"\"}}"), 1, ZCL_MAINNET, &tip) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_electrum_tip_reply(TEXT("{\"id\":1,\"result\":{\"height\":-1,\"hex\":\"\"}}"), 1, ZCL_MAINNET, &tip) == ZCL_OUT_OF_RANGE);
}

static void argument_bounds(void)
{
    zcl_reported_balance balance;
    zcl_reported_tip tip;
    memset(&balance, 0xa5, sizeof(balance));
    memset(&tip, 0xa5, sizeof(tip));
    CHECK(zcl_electrum_balance_reply(NULL, 0, 1, &balance) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_electrum_tip_reply(NULL, 0, 1, ZCL_MAINNET, &tip) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_electrum_balance_reply(TEXT("{}"), 1, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_electrum_tip_reply(TEXT("{}"), 1, ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_electrum_version_reply((const uint8_t *)frame, SIZE_MAX, 1) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_electrum_features_reply((const uint8_t *)frame, SIZE_MAX, 1, ZCL_MAINNET) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_electrum_genesis_reply((const uint8_t *)frame, SIZE_MAX, 1, ZCL_MAINNET) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_electrum_balance_reply((const uint8_t *)frame, SIZE_MAX, 1, &balance) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_electrum_tip_reply((const uint8_t *)frame, SIZE_MAX, 1, ZCL_MAINNET, &tip) == ZCL_OUT_OF_RANGE);
    for (size_t i = 0; i < sizeof(balance); ++i) CHECK(((const uint8_t *)&balance)[i] == 0xa5);
    for (size_t i = 0; i < sizeof(tip); ++i) CHECK(((const uint8_t *)&tip)[i] == 0xa5);
}

static void exact_json_limits(void)
{
    static uint8_t maximum[ZCL_ELECTRUM_FRAME_MAX + 1];
    static const char prefix[] = "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0},\"extension\":\"";
    memset(maximum, 'a', sizeof(maximum));
    memcpy(maximum, prefix, sizeof(prefix) - 1);
    memcpy(maximum + ZCL_ELECTRUM_FRAME_MAX - 2, "\"}", 2);
    maximum[ZCL_ELECTRUM_FRAME_MAX] = '\n';
    zcl_reported_balance balance = {0};
    CHECK(zcl_electrum_balance_reply(maximum, ZCL_ELECTRUM_FRAME_MAX, 1, &balance) == ZCL_OK);
    CHECK(zcl_electrum_balance_reply(maximum, sizeof(maximum), 1, &balance) == ZCL_OUT_OF_RANGE);
    zcl_electrum_line_reset(&line);
    size_t consumed = 0;
    CHECK(zcl_electrum_line_feed(&line, maximum, sizeof(maximum), &consumed) == ZCL_OK);
    CHECK(line.ready && line.used == ZCL_ELECTRUM_FRAME_MAX && consumed == sizeof(maximum));
    CHECK(zcl_electrum_balance_reply(line.bytes, line.used, 1, &balance) == ZCL_OK);

    /* Feature-host keys may contain a full DNS name, not only short method keys. */
    zcl_rpc_json doc;
    memset(frame, 'a', sizeof(frame));
    memcpy(frame, "{\"", 2);
    memcpy(frame + 2 + ZCL_RPC_KEY_MAX, "\":0}", 4);
    CHECK(zcl_rpc_json_parse((const uint8_t *)frame, 6 + ZCL_RPC_KEY_MAX, &doc) == ZCL_OK);
    frame[2 + ZCL_RPC_KEY_MAX] = 'a';
    memcpy(frame + 3 + ZCL_RPC_KEY_MAX, "\":0}", 4);
    CHECK(zcl_rpc_json_parse((const uint8_t *)frame, 7 + ZCL_RPC_KEY_MAX, &doc) != ZCL_OK);
}

static void framing(void)
{
    static const uint8_t input[] = "{\"id\":1}\r\n{\"id\":2}\n";
    for (size_t split = 0; split < 10; ++split) {
        zcl_electrum_line_reset(&line);
        size_t used = 99;
        CHECK(zcl_electrum_line_feed(&line, input, split, &used) == ZCL_OK && used == split);
        CHECK(zcl_electrum_line_feed(&line, input + split, sizeof(input) - 1 - split, &used) == ZCL_OK);
        CHECK(line.ready && line.used == 9 && used + split == 10);
        CHECK(memcmp(line.bytes, input, 9) == 0);
        CHECK(zcl_electrum_line_feed(&line, input, 1, &used) == ZCL_BUSY);
    }
    zcl_electrum_line_reset(&line);
    memset(line.bytes, ' ', sizeof(line.bytes));
    line.used = sizeof(line.bytes);
    size_t used = 77;
    CHECK(zcl_electrum_line_feed(&line, (const uint8_t *)"x", 1, &used) == ZCL_OUT_OF_RANGE);
    CHECK(line.failed && used == 77);
    CHECK(zcl_electrum_line_feed(&line, (const uint8_t *)"\n", 1, &used) == ZCL_INVALID_ENCODING);
    zcl_electrum_line_reset(&line);
    for (size_t i = 0; i < sizeof(line.bytes); ++i) CHECK(line.bytes[i] == 0);
    CHECK(zcl_electrum_line_feed(&line, input, SIZE_MAX, &used) == ZCL_OUT_OF_RANGE);
    CHECK(line.used == 0 && !line.failed && used == 77);
    memset(line.bytes, ' ', sizeof(line.bytes));
    line.used = sizeof(line.bytes);
    CHECK(zcl_electrum_line_feed(&line, (const uint8_t *)"\n", 1, &used) == ZCL_OK);
    CHECK(line.ready && !line.failed && line.used == sizeof(line.bytes) && used == 1);
}

static void rpc_comparison_values(void)
{
    static const struct { const char *json, *text; bool matches; } cases[] = {
        {"{\"x\":\"\"}", "", true},
        {"{\"x\":\"id\"}", "id", true},
        {"{\"x\":\"id\"}", "i", false},
        {"{\"x\":\"id\"}", "ids", false},
        {"{\"x\":\"id\"}", "Id", false},
        {"{\"x\":\"\\u0069d\"}", "id", true},
        {"{\"x\":\"a\\u0020b\"}", "a b", true},
        {"{\"x\":\"a\\/b\"}", "a/b", true},
        {"{\"x\":\"\\\\\"}", "\\", true},
        {"{\"x\":\"\\\"\"}", "\"", true},
        {"{\"x\":\"\\u0000\"}", "", false},
        {"{\"x\":\"\\n\"}", "\n", false},
        {"{\"x\":\"\xc3\xa9\"}", "\xc3\xa9", false},
        {"{\"x\":\"\\u00e9\"}", "\xc3\xa9", false},
        {"{\"x\":\"\\ud83d\\ude00\"}", "\xf0\x9f\x98\x80", false},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        zcl_rpc_json doc;
        CHECK(zcl_rpc_json_parse((const uint8_t *)cases[i].json, strlen(cases[i].json), &doc) == ZCL_OK);
        const zcl_rpc_token *value = zcl_rpc_member(&doc, &doc.tokens[0], TEXT("x"));
        CHECK(value != NULL);
        CHECK(zcl_rpc_string_is(&doc, value, (const uint8_t *)cases[i].text,
            strlen(cases[i].text)) == cases[i].matches);
    }
}

static void rpc_comparison_bounds(void)
{
    zcl_rpc_json doc;
    CHECK(zcl_rpc_json_parse(TEXT("{\"x\":\"id\",\"n\":12}"), &doc) == ZCL_OK);
    const zcl_rpc_token *value = zcl_rpc_member(&doc, &doc.tokens[0], TEXT("x"));
    const zcl_rpc_token *number = zcl_rpc_member(&doc, &doc.tokens[0], TEXT("n"));
    CHECK(value != NULL && number != NULL);
    CHECK(!zcl_rpc_string_is(NULL, value, TEXT("id")));
    CHECK(!zcl_rpc_string_is(&doc, NULL, TEXT("id")));
    CHECK(!zcl_rpc_string_is(&doc, number, TEXT("12")));
    CHECK(!zcl_rpc_string_is(&doc, value, (const uint8_t *)"id", SIZE_MAX));
    doc.text = NULL;
    CHECK(!zcl_rpc_string_is(&doc, value, TEXT("id")));
}

static void rpc_comparison_lengths(void)
{
    const size_t lengths[] = {0, 1, ZCL_RPC_KEY_MAX - 1, ZCL_RPC_KEY_MAX, ZCL_RPC_KEY_MAX + 1};
    char encoded[300], expected[ZCL_RPC_KEY_MAX + 2];
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        memset(expected, 'a', sizeof(expected));
        expected[lengths[i]] = 0;
        const int size = snprintf(encoded, sizeof(encoded), "{\"x\":\"%s\"}", expected);
        CHECK(size > 0 && (size_t)size < sizeof(encoded));
        zcl_rpc_json doc;
        CHECK(zcl_rpc_json_parse((const uint8_t *)encoded, (size_t)size, &doc) == ZCL_OK);
        const zcl_rpc_token *value = zcl_rpc_member(&doc, &doc.tokens[0], TEXT("x"));
        CHECK(value != NULL);
        CHECK(zcl_rpc_string_is(&doc, value, (const uint8_t *)expected, lengths[i]) ==
            (lengths[i] <= ZCL_RPC_KEY_MAX));
        CHECK(!zcl_rpc_string_is(&doc, value, (const uint8_t *)expected, lengths[i] + 1));
    }
}

int main(void)
{
    requests(); balance_values(); identity(); headers(); json_bounds(); framing();
    fork_headers(); argument_bounds(); exact_json_limits();
    rpc_comparison_values(); rpc_comparison_bounds(); rpc_comparison_lengths();
    puts("Read-only Electrum requests, strict replies, genesis identity, money and framing passed");
    return 0;
}
