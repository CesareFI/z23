/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define REQUIRE(v) do { if (!(v)) { \
    fprintf(stderr, "Electrum framing invariant at %d\n", __LINE__); abort(); } } while (0)

static void prefix(const zcl_electrum_line *line, const uint8_t *input, size_t available)
{
    REQUIRE(line->used <= available && line->used <= ZCL_ELECTRUM_FRAME_MAX);
    REQUIRE(memcmp(line->bytes, input, line->used) == 0);
    REQUIRE(memchr(input, '\n', line->used) == NULL);
}

static void immutable(zcl_electrum_line *line, zcl_status expected)
{
    /* Single-threaded host fixture only; never put a framing buffer on stack. */
    static zcl_electrum_line before;
    memcpy(&before, line, sizeof(before));
    size_t consumed = SIZE_MAX;
    REQUIRE(zcl_electrum_line_feed(line, (const uint8_t *)"\n", 1, &consumed) == expected);
    REQUIRE(consumed == SIZE_MAX && memcmp(&before, line, sizeof(before)) == 0);
}

static void reset(zcl_electrum_line *line)
{
    static const uint8_t zero[sizeof(*line)] = {0};
    zcl_electrum_line_reset(line);
    REQUIRE(memcmp(line, zero, sizeof(*line)) == 0);
}

static void history_reply(const uint8_t *data, size_t size)
{
    struct {
        uint64_t before;
        zcl_reported_history history;
        uint64_t after;
    } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_reported_history previous;
    memcpy(&previous, &box.history, sizeof(previous));
    const zcl_status status = zcl_electrum_history_reply(data, size, 1, &box.history);
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (status != ZCL_OK) {
        if (memcmp(&previous, &box.history, sizeof(previous)) != 0) abort();
        return;
    }
    if (box.history.count > ZCL_ELECTRUM_HISTORY_MAX) abort();
    for (size_t i = 0; i < box.history.count; ++i) {
        if (box.history.entries[i].reported_height < -1) abort();
        for (size_t j = 0; j < i; ++j) {
            if (memcmp(box.history.entries[i].txid, box.history.entries[j].txid, 32) == 0) abort();
        }
    }
}

static void identity_replies(const uint8_t *data, size_t size)
{
    /* The pinned networks have different genesis identities. This checks only
     * mutually exclusive acceptance, not authentication of a remote source. */
    const zcl_status main_features = zcl_electrum_features_reply(data, size, 1, ZCL_MAINNET);
    const zcl_status test_features = zcl_electrum_features_reply(data, size, 1, ZCL_TESTNET);
    const zcl_status main_genesis = zcl_electrum_genesis_reply(data, size, 1, ZCL_MAINNET);
    const zcl_status test_genesis = zcl_electrum_genesis_reply(data, size, 1, ZCL_TESTNET);
    if ((main_features == ZCL_OK && test_features == ZCL_OK) ||
        (main_genesis == ZCL_OK && test_genesis == ZCL_OK)) {
        fprintf(stderr, "Electrum identity accepted both networks\n");
        abort();
    }
}

static void balance_reply(const uint8_t *data, size_t size)
{
    zcl_reported_balance balance, before;
    memset(&balance, 0xa5, sizeof(balance));
    memcpy(&before, &balance, sizeof(before));
    const zcl_status parsed = zcl_electrum_balance_reply(data, size, 1, &balance);
    if (parsed == ZCL_OK) {
        if (balance.confirmed > ZCL_MAX_MONEY || balance.total > ZCL_MAX_MONEY) abort();
        if (balance.pending_delta < -(int64_t)ZCL_MAX_MONEY || balance.pending_delta > (int64_t)ZCL_MAX_MONEY) abort();
        if ((int64_t)balance.total != (int64_t)balance.confirmed + balance.pending_delta) abort();
    } else if (memcmp(&balance, &before, sizeof(before)) != 0) abort();
}

static void replies(const uint8_t *data, size_t size)
{
    history_reply(data, size);
    identity_replies(data, size);
    balance_reply(data, size);
    (void)zcl_electrum_version_reply(data, size, 1);
    for (int chain = 0; chain < 2; ++chain) {
        zcl_reported_tip tip, old;
        memset(&tip, 0xa5, sizeof(tip));
        memcpy(&old, &tip, sizeof(old));
        const zcl_status status = zcl_electrum_tip_reply(data, size, 1, (zcl_network)chain, &tip);
        if (status != ZCL_OK && memcmp(&tip, &old, sizeof(tip)) != 0) abort();
    }
}

static void framing(const uint8_t *data, size_t size, size_t chunk)
{
    /* Static storage belongs only to this single-threaded host fuzz target. */
    static struct { uint64_t before; zcl_electrum_line line; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    reset(&box.line);
    size_t offset = 0, start = 0;
    bool checked_ready = false;
    while (offset < size) {
        const size_t count = size - offset < chunk ? size - offset : chunk;
        size_t consumed = SIZE_MAX;
        const zcl_status status = zcl_electrum_line_feed(&box.line, data + offset, count, &consumed);
        if (status != ZCL_OK) {
            REQUIRE(status == ZCL_OUT_OF_RANGE && consumed == SIZE_MAX);
            REQUIRE(box.line.failed && !box.line.ready && box.line.used == ZCL_ELECTRUM_FRAME_MAX);
            prefix(&box.line, data + start, size - start);
            immutable(&box.line, ZCL_INVALID_ENCODING);
            break;
        }
        REQUIRE(consumed > 0 && consumed <= count && !box.line.failed);
        offset += consumed;
        if (box.line.ready) {
            REQUIRE(data[offset - 1] == '\n' && box.line.used == offset - start - 1);
            prefix(&box.line, data + start, offset - start - 1);
            if (!checked_ready) immutable(&box.line, ZCL_BUSY);
            checked_ready = true;
            replies(box.line.bytes, box.line.used);
            reset(&box.line);
            start = offset;
        } else {
            REQUIRE(consumed == count && box.line.used == offset - start);
        }
    }
    if (!box.line.failed) prefix(&box.line, data + start, size - start);
    reset(&box.line);
    REQUIRE(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    if (size == 0) data = (const uint8_t *)"";
    replies(data, size);
    /* Exercise the same bytes both fragmented and in one bounded input span. */
    framing(data, size, size == 0 ? 1 : (size_t)data[0] + 1);
    framing(data, size, size == 0 ? 1 : size);
    return 0;
}
