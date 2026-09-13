/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync.h"
#include <string.h>

static zcl_status failure_status(const zcl_sync *session)
{
    return session->fault == ZCL_OK ? ZCL_INVALID_ARGUMENT : session->fault;
}

zcl_status zcl_sync_abort(zcl_sync *session, zcl_status reason)
{
    if (session == NULL || reason == ZCL_OK) return ZCL_INVALID_ARGUMENT;
    memset(&session->candidate.balance, 0, sizeof(session->candidate.balance));
    memset(&session->candidate.tip, 0, sizeof(session->candidate.tip));
    memset(&session->candidate.history, 0, sizeof(session->candidate.history));
    session->candidate.has_history = false;
    session->phase = ZCL_SYNC_FAILED;
    session->waiting = false;
    session->fault = reason;
    return reason;
}

zcl_status zcl_sync_start(zcl_sync *session, const uint8_t *address, size_t address_length,
                           zcl_network network, uint32_t first_id)
{
    if (session == NULL) return ZCL_INVALID_ARGUMENT;
    memset(session, 0, sizeof(*session));
    if (first_id == 0 || first_id > UINT32_MAX - 5)
        return zcl_sync_abort(session, ZCL_OUT_OF_RANGE);
    zcl_address parsed = {0};
    const zcl_status status = zcl_address_parse(address, address_length, network, &parsed);
    if (status != ZCL_OK) return zcl_sync_abort(session, status);
    memcpy(session->candidate.address, address, sizeof(session->candidate.address));
    session->candidate.network = network;
    session->request_id = first_id;
    session->phase = ZCL_SYNC_VERSION;
    return ZCL_OK;
}

zcl_status zcl_sync_start_with_history(zcl_sync *session, const uint8_t *address,
    size_t address_length, zcl_network network, uint32_t first_id)
{
    const zcl_status status = zcl_sync_start(session, address, address_length, network, first_id);
    if (status != ZCL_OK) return status;
    if (first_id > UINT32_MAX - 6) return zcl_sync_abort(session, ZCL_OUT_OF_RANGE);
    session->include_history = true;
    return ZCL_OK;
}

static zcl_status active(const zcl_sync *session)
{
    if (session == NULL) return ZCL_INVALID_ARGUMENT;
    if (session->phase == ZCL_SYNC_FAILED) return failure_status(session);
    if ((session->phase < ZCL_SYNC_VERSION || session->phase > ZCL_SYNC_TIP_AFTER) &&
        session->phase != ZCL_SYNC_HISTORY)
        return ZCL_INVALID_ARGUMENT;
    if (session->request_id == 0) return ZCL_INVALID_ARGUMENT;
    return ZCL_OK;
}

zcl_status zcl_sync_request(zcl_sync *session, uint8_t *output, size_t capacity, size_t *length)
{
    const zcl_status allowed = active(session);
    if (allowed != ZCL_OK) return allowed;
    if (session->waiting) return ZCL_BUSY;
    static const zcl_electrum_method methods[] = {
        ZCL_ELECTRUM_VERSION, ZCL_ELECTRUM_FEATURES, ZCL_ELECTRUM_GENESIS,
        ZCL_ELECTRUM_TIP, ZCL_ELECTRUM_BALANCE, ZCL_ELECTRUM_TIP
    };
    _Static_assert(sizeof(methods) / sizeof(methods[0]) == ZCL_SYNC_TIP_AFTER - ZCL_SYNC_VERSION + 1,
        "Every active phase needs a request method");
    const zcl_electrum_method method = session->phase == ZCL_SYNC_HISTORY
        ? ZCL_ELECTRUM_HISTORY : methods[(size_t)(session->phase - ZCL_SYNC_VERSION)];
    const zcl_status status = zcl_electrum_request(method, session->request_id,
        session->candidate.address, sizeof(session->candidate.address), session->candidate.network,
        output, capacity, length);
    if (status == ZCL_OK) session->waiting = true;
    return status;
}

static zcl_status identity_reply(const zcl_sync *session, const uint8_t *frame, size_t length)
{
    switch (session->phase) {
    case ZCL_SYNC_VERSION:
        return zcl_electrum_version_reply(frame, length, session->request_id);
    case ZCL_SYNC_FEATURES:
        return zcl_electrum_features_reply(frame, length, session->request_id, session->candidate.network);
    case ZCL_SYNC_GENESIS:
        return zcl_electrum_genesis_reply(frame, length, session->request_id, session->candidate.network);
    default:
        return ZCL_INVALID_ARGUMENT;
    }
}

static zcl_status history_reply(zcl_sync *session, const uint8_t *frame, size_t length)
{
    zcl_reported_history *history = &session->candidate.history;
    const zcl_status status = zcl_electrum_history_reply(frame, length, session->request_id, history);
    if (status != ZCL_OK) return status;
    for (size_t i = 0; i < history->count; ++i) {
        const int32_t height = history->entries[i].reported_height;
        if (height > 0 && (uint32_t)height > session->candidate.tip.height)
            return ZCL_IO_UNCERTAIN;
    }
    session->candidate.has_history = true;
    return ZCL_OK;
}

static zcl_status data_reply(zcl_sync *session, const uint8_t *frame, size_t length)
{
    if (session->phase == ZCL_SYNC_BALANCE)
        return zcl_electrum_balance_reply(frame, length, session->request_id, &session->candidate.balance);
    if (session->phase == ZCL_SYNC_HISTORY) return history_reply(session, frame, length);
    zcl_reported_tip tip = {0};
    const zcl_status status = zcl_electrum_tip_reply(frame, length, session->request_id,
        session->candidate.network, &tip);
    if (status != ZCL_OK) return status;
    if (session->phase == ZCL_SYNC_TIP_BEFORE) {
        session->candidate.tip = tip;
        return ZCL_OK;
    }
    if (tip.height != session->candidate.tip.height ||
        memcmp(tip.hash, session->candidate.tip.hash, sizeof(tip.hash)) != 0) return ZCL_IO_UNCERTAIN;
    return ZCL_OK;
}

static void advance(zcl_sync *session)
{
    if (session->phase == ZCL_SYNC_BALANCE && session->include_history)
        session->phase = ZCL_SYNC_HISTORY;
    else if (session->phase == ZCL_SYNC_HISTORY) session->phase = ZCL_SYNC_TIP_AFTER;
    else session->phase = (zcl_sync_phase)(session->phase + 1);
    if (session->phase != ZCL_SYNC_DONE) ++session->request_id;
}

zcl_status zcl_sync_reply(zcl_sync *session, const uint8_t *frame, size_t length)
{
    const zcl_status allowed = active(session);
    if (allowed != ZCL_OK) return allowed;
    if (!session->waiting) return zcl_sync_abort(session, ZCL_INVALID_ENCODING);
    zcl_status status;
    if (session->phase <= ZCL_SYNC_GENESIS) status = identity_reply(session, frame, length);
    else status = data_reply(session, frame, length);
    if (status != ZCL_OK) return zcl_sync_abort(session, status);
    session->waiting = false;
    advance(session);
    return ZCL_OK;
}

zcl_status zcl_sync_get_report(const zcl_sync *session, zcl_sync_report *report)
{
    if (session == NULL || report == NULL) return ZCL_INVALID_ARGUMENT;
    if (session->phase == ZCL_SYNC_FAILED) return failure_status(session);
    if (session->phase != ZCL_SYNC_DONE) return ZCL_BUSY;
    *report = session->candidate;
    return ZCL_OK;
}
