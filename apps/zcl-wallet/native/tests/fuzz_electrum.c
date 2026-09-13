/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

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

static void replies(const uint8_t *data, size_t size)
{
    history_reply(data, size);
    zcl_reported_balance balance, before;
    memset(&balance, 0xa5, sizeof(balance));
    memcpy(&before, &balance, sizeof(before));
    const zcl_status parsed = zcl_electrum_balance_reply(data, size, 1, &balance);
    if (parsed == ZCL_OK) {
        if (balance.confirmed > ZCL_MAX_MONEY || balance.total > ZCL_MAX_MONEY) abort();
        if (balance.pending_delta < -(int64_t)ZCL_MAX_MONEY || balance.pending_delta > (int64_t)ZCL_MAX_MONEY) abort();
        if ((int64_t)balance.total != (int64_t)balance.confirmed + balance.pending_delta) abort();
    } else if (memcmp(&balance, &before, sizeof(before)) != 0) abort();
    (void)zcl_electrum_version_reply(data, size, 1);
    for (int chain = 0; chain < 2; ++chain) {
        (void)zcl_electrum_features_reply(data, size, 1, (zcl_network)chain);
        (void)zcl_electrum_genesis_reply(data, size, 1, (zcl_network)chain);
        zcl_reported_tip tip, old;
        memset(&tip, 0xa5, sizeof(tip));
        memcpy(&old, &tip, sizeof(old));
        const zcl_status status = zcl_electrum_tip_reply(data, size, 1, (zcl_network)chain, &tip);
        if (status != ZCL_OK && memcmp(&tip, &old, sizeof(tip)) != 0) abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    replies(data, size);
    /* Static storage belongs only to this single-threaded host fuzz target. */
    static zcl_electrum_line line;
    zcl_electrum_line_reset(&line);
    size_t offset = 0;
    const size_t chunk = size == 0 ? 1 : (size_t)data[0] + 1;
    while (offset < size) {
        const size_t count = size - offset < chunk ? size - offset : chunk;
        size_t consumed = SIZE_MAX;
        const zcl_status status = zcl_electrum_line_feed(&line, data + offset, count, &consumed);
        if (status != ZCL_OK) break;
        if (consumed == 0 || consumed > count || line.used > ZCL_ELECTRUM_FRAME_MAX) abort();
        offset += consumed;
        if (line.ready) {
            replies(line.bytes, line.used);
            zcl_electrum_line_reset(&line);
        }
    }
    return 0;
}
