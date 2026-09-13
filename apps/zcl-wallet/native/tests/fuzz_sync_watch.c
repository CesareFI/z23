/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_watch.h"
#include "sync_fixture.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static bool same_state(const zcl_sync_snapshot *first, const zcl_sync_snapshot *second)
{
    return first->freshness == second->freshness && first->refreshing == second->refreshing &&
        first->last_fault == second->last_fault;
}

static void inspect_wakeup(const zcl_sync_watch *watch, uint64_t now, const zcl_sync_snapshot *report)
{
    const uint64_t delay = report->next_change_ms;
    if (delay > ZCL_SYNC_FRESH_MS) abort();
    if (delay == 0) {
        if (report->refreshing || report->freshness == ZCL_BALANCE_UNVERIFIED) abort();
        return;
    }
    /* A relative delay requires no absolute addition in production. Bound this
     * fixture's future timestamps explicitly before exercising both sides. */
    if (now > UINT64_MAX - delay) return;
    zcl_sync_watch copy = *watch;
    zcl_sync_snapshot before = {0}, after = {0};
    if (zcl_sync_watch_snapshot(&copy, now + delay - 1, &before) != ZCL_OK) abort();
    if (!same_state(&before, report)) abort();
    if (before.next_change_ms != 1) abort();
    if (zcl_sync_watch_snapshot(&copy, now + delay, &after) != ZCL_OK) abort();
    if (same_state(&after, report)) abort();
    if (after.next_change_ms != 0) abort();
}

static void inspect_history(const zcl_sync_report *report, bool history)
{
    if (report->has_history != history) abort();
    if (report->history.count > ZCL_ELECTRUM_HISTORY_MAX) abort();
    for (size_t i = 0; i < report->history.count; ++i) {
        const int32_t height = report->history.entries[i].reported_height;
        if (height < -1 || (height > 0 && (uint32_t)height > report->tip.height)) abort();
    }
}

static void inspect_balance(const zcl_reported_balance *balance)
{
    if (balance->confirmed > ZCL_MAX_MONEY || balance->total > ZCL_MAX_MONEY) abort();
    if (balance->pending_delta < -(int64_t)ZCL_MAX_MONEY || balance->pending_delta > (int64_t)ZCL_MAX_MONEY) abort();
    if ((int64_t)balance->total != (int64_t)balance->confirmed + balance->pending_delta) abort();
}

static void inspect(zcl_sync_watch *watch, uint64_t now)
{
    zcl_sync_snapshot report;
    if (zcl_sync_watch_snapshot(watch, now, &report) != ZCL_OK) abort();
    inspect_wakeup(watch, now, &report);
    if (report.freshness == ZCL_BALANCE_UNAVAILABLE) {
        const zcl_sync_report empty = {0};
        if (memcmp(&report.report, &empty, sizeof(empty)) != 0 || report.age_ms != 0) abort();
        return;
    }
    inspect_history(&report.report, watch->include_history);
    inspect_balance(&report.report.balance);
    if (report.freshness == ZCL_BALANCE_UNVERIFIED &&
        (report.refreshing || report.last_fault != ZCL_OK || report.age_ms >= ZCL_SYNC_FRESH_MS)) abort();
}

static void response(zcl_sync_watch *watch, uint64_t token, uint64_t now,
    const uint8_t *bytes, size_t length, bool valid)
{
    static char frame[4096]; /* Host-only single-threaded fixture buffer. */
    if (valid && watch->in_flight) {
        const unsigned phase = (unsigned)watch->attempt.phase;
        if ((phase < 1 || phase > 6) && phase != ZCL_SYNC_HISTORY) abort();
        const size_t count = sync_fixture_reply(watch->network, phase, watch->attempt.request_id,
            frame, sizeof(frame));
        (void)zcl_sync_watch_reply(watch, token, now, (const uint8_t *)frame, count);
    } else {
        (void)zcl_sync_watch_reply(watch, token, now, bytes, length);
    }
}

static void request(zcl_sync_watch *watch, uint64_t chosen, uint64_t now)
{
    uint8_t request[256] = {0xa5};
    size_t written = SIZE_MAX;
    const zcl_status status = zcl_sync_watch_request(watch, chosen, now,
        request, sizeof(request), &written);
    if (status == ZCL_OK) {
        if (written == 0 || written > sizeof(request)) abort();
    } else if (written != SIZE_MAX || request[0] != 0xa5) abort();
}

static void reject_unowned_failure(zcl_sync_watch *watch)
{
    zcl_sync_watch before;
    memcpy(&before, watch, sizeof(before));
    if (zcl_sync_watch_fail(watch, 0, ZCL_IO_FAILURE) != ZCL_CANCELLED) abort();
    if (memcmp(&before, watch, sizeof(before)) != 0) abort();
}

static void initialize(zcl_sync_watch *watch, const zcl_sync *fixture, bool history)
{
    const uint8_t source[32] = {1};
    const zcl_status status = history
        ? zcl_sync_watch_init_with_history(watch, fixture->candidate.address, 35, fixture->candidate.network, source, sizeof(source))
        : zcl_sync_watch_init(watch, fixture->candidate.address, 35, fixture->candidate.network, source, sizeof(source));
    if (status != ZCL_OK) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > ZCL_ELECTRUM_FRAME_MAX) return 0;
    zcl_sync fixture;
    sync_fixture_start(&fixture, (data[0] & 8) == 0 ? ZCL_MAINNET : ZCL_TESTNET, 1);
    zcl_sync_watch watch;
    initialize(&watch, &fixture, (data[0] & 4) != 0);
    uint64_t token = 0, old = 0, now = 0;
    const size_t operations = size < 128 ? size : 128;
    for (size_t i = 0; i < operations; ++i) {
        const uint8_t op = data[i];
        if (op == 0) now = 0;
        else if (now <= UINT64_MAX - op) now += op;
        const uint64_t chosen = (op & 128) == 0 ? token : old;
        switch (op % 6) {
        case 0: {
            uint64_t next = 0;
            if (zcl_sync_watch_begin(&watch, now, (uint64_t)op + 20, 1, &next) == ZCL_OK) {
                old = token;
                token = next;
            }
            break;
        }
        case 1:
            request(&watch, chosen, now);
            break;
        case 2:
            response(&watch, chosen, now, data + i, size - i, (op & 8) == 0);
            break;
        case 3:
            (void)zcl_sync_watch_fail(&watch, chosen, ZCL_CANCELLED);
            break;
        case 4:
            inspect(&watch, now);
            break;
        default:
            reject_unowned_failure(&watch);
            break;
        }
        inspect(&watch, now);
    }
    zcl_sync_watch_close(&watch);
    return 0;
}
