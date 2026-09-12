/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_watch.h"
#include "sync_fixture.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void inspect(zcl_sync_watch *watch, uint64_t now)
{
    zcl_sync_snapshot report;
    if (zcl_sync_watch_snapshot(watch, now, &report) != ZCL_OK) abort();
    if (report.freshness == ZCL_BALANCE_UNAVAILABLE) {
        const zcl_sync_report empty = {0};
        if (memcmp(&report.report, &empty, sizeof(empty)) != 0 || report.age_ms != 0) abort();
        return;
    }
    const zcl_reported_balance *balance = &report.report.balance;
    if (balance->confirmed > ZCL_MAX_MONEY || balance->total > ZCL_MAX_MONEY) abort();
    if (balance->pending_delta < -(int64_t)ZCL_MAX_MONEY || balance->pending_delta > (int64_t)ZCL_MAX_MONEY) abort();
    if ((int64_t)balance->total != (int64_t)balance->confirmed + balance->pending_delta) abort();
    if (report.freshness == ZCL_BALANCE_UNVERIFIED &&
        (report.refreshing || report.last_fault != ZCL_OK || report.age_ms >= ZCL_SYNC_FRESH_MS)) abort();
}

static void response(zcl_sync_watch *watch, uint64_t token, uint64_t now,
    const uint8_t *bytes, size_t length, bool valid)
{
    static char frame[4096]; /* Host-only single-threaded fixture buffer. */
    if (valid && watch->in_flight) {
        const unsigned phase = (unsigned)watch->attempt.phase;
        if (phase < 1 || phase > 6) abort();
        const size_t count = sync_fixture_reply(watch->network, phase, watch->attempt.request_id,
            frame, sizeof(frame));
        (void)zcl_sync_watch_reply(watch, token, now, (const uint8_t *)frame, count);
    } else {
        (void)zcl_sync_watch_reply(watch, token, now, bytes, length);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > ZCL_ELECTRUM_FRAME_MAX) return 0;
    zcl_sync fixture;
    sync_fixture_start(&fixture, (data[0] & 8) == 0 ? ZCL_MAINNET : ZCL_TESTNET, 1);
    zcl_sync_watch watch;
    const uint8_t source[32] = {1};
    if (zcl_sync_watch_init(&watch, fixture.candidate.address, 35, fixture.candidate.network,
        source, sizeof(source)) != ZCL_OK) abort();
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
        case 1: {
            uint8_t request[256] = {0xa5};
            size_t written = SIZE_MAX;
            const zcl_status status = zcl_sync_watch_request(&watch, chosen, now,
                request, sizeof(request), &written);
            if (status == ZCL_OK) {
                if (written == 0 || written > sizeof(request)) abort();
            } else if (written != SIZE_MAX || request[0] != 0xa5) abort();
            break;
        }
        case 2:
            response(&watch, chosen, now, data + i, size - i, (op & 8) == 0);
            break;
        case 3:
            (void)zcl_sync_watch_fail(&watch, chosen, ZCL_CANCELLED);
            break;
        case 4:
            inspect(&watch, now);
            break;
        default: {
            zcl_sync_watch before;
            memcpy(&before, &watch, sizeof(before));
            if (zcl_sync_watch_fail(&watch, 0, ZCL_IO_FAILURE) != ZCL_CANCELLED) abort();
            if (memcmp(&before, &watch, sizeof(before)) != 0) abort();
            break;
        }
        }
        inspect(&watch, now);
    }
    zcl_sync_watch_close(&watch);
    return 0;
}
