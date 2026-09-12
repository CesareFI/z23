/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "rpc_json.h"

static zcl_status balances(const zcl_rpc_json *doc, const zcl_rpc_token *result,
                            zcl_reported_balance *balance)
{
    int64_t confirmed = 0, pending = 0;
    const zcl_status first = zcl_rpc_integer(doc, zcl_rpc_member(doc, result,
        (const uint8_t *)"confirmed", 9), &confirmed);
    if (first != ZCL_OK) return first;
    const zcl_status second = zcl_rpc_integer(doc, zcl_rpc_member(doc, result,
        (const uint8_t *)"unconfirmed", 11), &pending);
    if (second != ZCL_OK) return second;
    if (confirmed < 0 || (uint64_t)confirmed > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    if (pending < -(int64_t)ZCL_MAX_MONEY || pending > (int64_t)ZCL_MAX_MONEY)
        return ZCL_OUT_OF_RANGE;
    /* Both operands are now +/-2.1e15; their sum cannot overflow int64. */
    const int64_t total = confirmed + pending;
    if (total < 0 || (uint64_t)total > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    *balance = (zcl_reported_balance){(uint64_t)confirmed, pending, (uint64_t)total};
    return ZCL_OK;
}

zcl_status zcl_electrum_balance_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_reported_balance *balance)
{
    if (balance == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_rpc_json doc;
    zcl_status status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    if (result->kind != ZJRP_OBJ_OPEN) return ZCL_INVALID_ENCODING;
    return balances(&doc, result, balance);
}
