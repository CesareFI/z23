/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "electrum_internal.h"
#include <string.h>

static zcl_status history_entry(const zcl_rpc_json *doc, const zcl_rpc_token *item,
                                zcl_reported_history_entry *entry)
{
    if (item->kind != ZJRP_OBJ_OPEN) return ZCL_INVALID_ENCODING;
    size_t length = 0;
    zcl_status status = zcl_rpc_hex(doc, zcl_rpc_member(doc, item,
        (const uint8_t *)"tx_hash", 7), entry->txid, sizeof(entry->txid), &length);
    if (status != ZCL_OK) return status;
    if (length != sizeof(entry->txid)) return ZCL_INVALID_ENCODING;
    int64_t height = 0;
    status = zcl_rpc_integer(doc, zcl_rpc_member(doc, item,
        (const uint8_t *)"height", 6), &height);
    if (status != ZCL_OK) return status;
    if (height < -1 || height > INT32_MAX) return ZCL_OUT_OF_RANGE;
    entry->reported_height = (int32_t)height;
    return ZCL_OK;
}

static bool duplicate_entry(const zcl_reported_history *history,
                             const zcl_reported_history_entry *entry)
{
    for (size_t i = 0; i < history->count; ++i) {
        if (memcmp(history->entries[i].txid, entry->txid, sizeof(entry->txid)) == 0)
            return true;
    }
    return false;
}

static zcl_status history_entries(const zcl_rpc_json *doc, const zcl_rpc_token *result,
                                   zcl_reported_history *history)
{
    if (result->kind != ZJRP_ARR_OPEN) return ZCL_INVALID_ENCODING;
    for (size_t i = 0; i < ZCL_ELECTRUM_HISTORY_MAX; ++i) {
        const zcl_rpc_token *item = zcl_rpc_child(doc, result, i);
        if (item == NULL) return ZCL_OK;
        zcl_reported_history_entry *entry = &history->entries[i];
        const zcl_status status = history_entry(doc, item, entry);
        if (status != ZCL_OK) return status;
        if (duplicate_entry(history, entry)) return ZCL_INVALID_ENCODING;
        ++history->count;
    }
    return zcl_rpc_child(doc, result, ZCL_ELECTRUM_HISTORY_MAX) == NULL
        ? ZCL_OK : ZCL_RESOURCE_EXHAUSTED;
}

zcl_status zcl_electrum_history_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_reported_history *history)
{
    if (history == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_rpc_json doc;
    zcl_status status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    zcl_reported_history parsed = {0};
    status = history_entries(&doc, result, &parsed);
    if (status != ZCL_OK) return status;
    *history = parsed;
    return ZCL_OK;
}
