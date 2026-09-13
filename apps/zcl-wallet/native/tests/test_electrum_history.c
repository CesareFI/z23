/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "History check failed at %d\n", __LINE__); abort(); } } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1
#define HASH "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
#define UPPER "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
static char frame[4096];

static void refused(const uint8_t *text, size_t length, uint32_t id, zcl_status expected)
{
    zcl_reported_history result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    const zcl_status status = zcl_electrum_history_reply(text, length, id, &result);
    CHECK(status != ZCL_OK);
    if (expected != ZCL_OK && status != expected) {
        fprintf(stderr, "History refusal status %d, expected %d (id %u, length %zu)\n",
            (int)status, (int)expected, id, length);
        abort();
    }
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
}

static void values(void)
{
    struct {
        uint64_t before;
        zcl_reported_history history;
        uint64_t after;
    } box = {UINT64_MAX, {0}, UINT64_MAX};
    zcl_reported_history *history = &box.history;
    CHECK(zcl_electrum_history_reply(TEXT("{\"id\":1,\"result\":[]}"), 1, history) == ZCL_OK);
    CHECK(history->count == 0);
    static const int32_t heights[] = {-1, 0, 1, INT32_MAX};
    for (size_t i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
        const int n = snprintf(frame, sizeof(frame), "{\"jsonrpc\":\"2.0\",\"error\":null,\"id\":4294967295,\"result\":[{\"tx_hash\":\"" UPPER "\",\"height\":%ld,\"fee\":123,\"extension\":\"\\ud83d\\ude00\"}]}", (long)heights[i]);
        CHECK(n > 0 && (size_t)n < sizeof(frame));
        CHECK(zcl_electrum_history_reply((const uint8_t *)frame, (size_t)n, UINT32_MAX, history) == ZCL_OK);
        CHECK(history->count == 1 && history->entries[0].reported_height == heights[i]);
        for (size_t j = 0; j < 32; ++j) CHECK(history->entries[0].txid[j] == j);
    }
    CHECK(box.before == UINT64_MAX && box.after == UINT64_MAX);
    CHECK(zcl_electrum_history_reply(TEXT("{\"id\":1,\"result\":[{\"tx_\\u0068ash\":\"\\u0030" HASH "\",\"height\":0}]}"), 1, history) != ZCL_OK);
    /* Escapes are decoded before hex length is checked, not counted as bytes. */
    CHECK(zcl_electrum_history_reply(TEXT("{\"id\":1,\"result\":[{\"tx_\\u0068ash\":\"\\u003000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\",\"height\":0}]}"), 1, history) == ZCL_OK);
    CHECK(history->count == 1 && history->entries[0].txid[31] == 31);
}

static void malformed(void)
{
    static const char *bad[] = {
        "{\"id\":1,\"result\":null}", "{\"id\":1,\"result\":{}}",
        "{\"id\":1,\"result\":[null]}", "{\"id\":1,\"result\":[[]]}",
        "{\"id\":1,\"result\":[\"*\"]}", "{\"id\":1,\"result\":[{}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"00\",\"height\":0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "0\",\"height\":0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "00\",\"height\":0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"g00102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\",\"height\":0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":32,\"height\":0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":-2}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":2147483648}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":9223372036854775808}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":-9223372036854775808}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":-0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":1e2}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":1.0}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":\"1\"}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":0,\"\\u0068eight\":1}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":0,\"fee\":{\"x\":1,\"x\":2}}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":0,\"extra\":\"\\ud800\"}]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":0},null]}",
        "{\"id\":1,\"result\":[{\"tx_hash\":\"" HASH "\",\"height\":0},{\"tx_hash\":\"" UPPER "\",\"height\":1}]}",
        "{\"id\":2,\"result\":[]}", "{\"id\":1,\"result\":[]}{}",
        "{\"id\":1,\"method\":\"x\",\"result\":[]}", "{\"id\":1,\"error\":1,\"result\":[]}",
        "{\"id\":1,\"result\":[],\"result\":[]}", "{\"id\":1,\"result\":[],\"\\u0069d\":1}"
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        refused((const uint8_t *)bad[i], strlen(bad[i]), 1, ZCL_OK);
}

static size_t bounded_frame(size_t count, bool fee)
{
    const char *prefix = "{\"id\":1,\"result\":[";
    size_t used = strlen(prefix);
    memcpy(frame, prefix, used);
    CHECK(count <= ZCL_ELECTRUM_HISTORY_MAX + 1);
    for (size_t i = 0; i < count; ++i) {
        /* Descending claimed heights prove that the parser preserves order. */
        const int n = snprintf(frame + used, sizeof(frame) - used,
            "%s{\"tx_hash\":\"%064zx\",\"height\":%zu%s}", i == 0 ? "" : ",",
            i, count - i, fee ? ",\"fee\":1" : "");
        CHECK(n > 0 && (size_t)n < sizeof(frame) - used);
        used += (size_t)n;
    }
    CHECK(sizeof(frame) - used >= 3);
    memcpy(frame + used, "]}", 3);
    return used + 2;
}

static void bounds(void)
{
    zcl_reported_history history = {0};
    size_t length = bounded_frame(ZCL_ELECTRUM_HISTORY_MAX, true);
    CHECK(zcl_electrum_history_reply((const uint8_t *)frame, length, 1, &history) == ZCL_OK);
    CHECK(history.count == ZCL_ELECTRUM_HISTORY_MAX);
    for (size_t i = 0; i < history.count; ++i) {
        CHECK(history.entries[i].txid[31] == i);
        CHECK(history.entries[i].reported_height == (int32_t)(history.count - i));
    }
    length = bounded_frame(ZCL_ELECTRUM_HISTORY_MAX + 1, false);
    refused((const uint8_t *)frame, length, 1, ZCL_RESOURCE_EXHAUSTED);
    refused((const uint8_t *)frame, SIZE_MAX, 1, ZCL_OUT_OF_RANGE);
    refused(NULL, 0, 1, ZCL_INVALID_ARGUMENT);
    refused(TEXT("{\"id\":1,\"result\":[]}"), 0, ZCL_INVALID_ENCODING);
    CHECK(zcl_electrum_history_reply(TEXT("{}"), 1, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_electrum_history_reply(TEXT("{\"id\":1,\"result\":[]}"), 1, &history) == ZCL_OK);
    CHECK(history.count == 0);
    for (size_t i = 0; i < ZCL_ELECTRUM_HISTORY_MAX; ++i) {
        CHECK(history.entries[i].reported_height == 0);
        for (size_t j = 0; j < 32; ++j) CHECK(history.entries[i].txid[j] == 0);
    }
}

static void request(void)
{
    static const char expected[] = "{\"id\":1,\"method\":\"blockchain.scripthash.get_history\",\"params\":[\"acb87996319dca2c2e2afd6c0f7514b18e72e204069718976e1abdc8fcf5de75\"]}\n";
    uint8_t hash[20] = {0}, address[35], output[ZCL_ELECTRUM_REQUEST_MAX];
    size_t address_len = 0, length = 0;
    CHECK(zcl_address_from_hash(hash, sizeof(hash), ZCL_MAINNET, address, sizeof(address), &address_len) == ZCL_OK);
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_HISTORY, 1, address, address_len, ZCL_MAINNET, output, sizeof(output), &length) == ZCL_OK);
    CHECK(length == sizeof(expected) - 1 && memcmp(output, expected, length) == 0);
    for (size_t capacity = 0; capacity < sizeof(expected) - 1; ++capacity) {
        memset(output, 0xa5, sizeof(output));
        length = SIZE_MAX;
        CHECK(zcl_electrum_request(ZCL_ELECTRUM_HISTORY, 1, address, address_len, ZCL_MAINNET, output, capacity, &length) == ZCL_BUFFER_TOO_SMALL);
        CHECK(length == SIZE_MAX);
        for (size_t j = 0; j < sizeof(output); ++j) CHECK(output[j] == 0xa5);
    }
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_HISTORY, 1, NULL, 0, ZCL_MAINNET, output, sizeof(output), &length) != ZCL_OK);
    CHECK(zcl_electrum_request(ZCL_ELECTRUM_HISTORY, 1, address, address_len, ZCL_TESTNET, output, sizeof(output), &length) == ZCL_UNSUPPORTED);
    CHECK(length == SIZE_MAX && output[0] == 0xa5);
}

int main(void)
{
    values(); malformed(); bounds(); request();
    puts("History values, duplicate/envelope refusal, bounded transactional output and requests passed");
    return 0;
}
