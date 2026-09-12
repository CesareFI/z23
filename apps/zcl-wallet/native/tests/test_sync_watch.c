/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_watch.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sync watch check failed at %d\n", __LINE__); abort(); } } while (0)
static char frame[4096];

static void init(zcl_sync_watch *watch, zcl_network network)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, network, 1);
    uint8_t source[32] = {1};
    CHECK(zcl_sync_watch_init(watch, fixture.candidate.address, 35, network, source, 32) == ZCL_OK);
    source[0] = 2;
    CHECK(watch->source_id[0] == 1);
}

static zcl_sync_snapshot snapshot(zcl_sync_watch *watch, uint64_t now,
    zcl_balance_freshness freshness, bool refreshing, zcl_status fault)
{
    zcl_sync_snapshot result;
    memset(&result, 0xa5, sizeof(result));
    CHECK(zcl_sync_watch_snapshot(watch, now, &result) == ZCL_OK);
    CHECK(result.freshness == freshness && result.refreshing == refreshing && result.last_fault == fault);
    CHECK(result.source_id[0] == 1);
    if (freshness == ZCL_BALANCE_UNAVAILABLE) {
        const zcl_sync_report empty = {0};
        CHECK(memcmp(&result.report, &empty, sizeof(empty)) == 0 && result.age_ms == 0);
    }
    return result;
}

static uint64_t begin(zcl_sync_watch *watch, uint64_t now, uint64_t timeout)
{
    uint64_t token = 0;
    CHECK(zcl_sync_watch_begin(watch, now, timeout, UINT32_MAX - 5, &token) == ZCL_OK);
    CHECK(token != 0);
    return token;
}

static void step(zcl_sync_watch *watch, uint64_t token, uint64_t now, unsigned number)
{
    uint8_t request[256];
    size_t written = 0;
    CHECK(zcl_sync_watch_request(watch, token, now, request, sizeof(request), &written) == ZCL_OK);
    CHECK(written > 0 && written <= sizeof(request));
    const size_t length = sync_fixture_reply(watch->network, number, watch->attempt.request_id,
        frame, sizeof(frame));
    CHECK(zcl_sync_watch_reply(watch, token, now, (const uint8_t *)frame, length) == ZCL_OK);
}

static void finish(zcl_sync_watch *watch, uint64_t token, uint64_t now)
{
    for (unsigned n = 1; n <= 6; ++n) step(watch, token, now + n, n);
}

static void late(zcl_sync_watch *watch, uint64_t token)
{
    zcl_sync_watch before;
    memcpy(&before, watch, sizeof(before));
    size_t written = 99;
    uint8_t request[256] = {0xa5};
    /* Even a late callback with a nonsensical clock cannot poison a new owner attempt. */
    CHECK(zcl_sync_watch_request(watch, token, UINT64_MAX, request, sizeof(request), &written) == ZCL_CANCELLED);
    CHECK(zcl_sync_watch_reply(watch, token, 0, (const uint8_t *)"{}", 2) == ZCL_CANCELLED);
    CHECK(zcl_sync_watch_fail(watch, token, ZCL_IO_FAILURE) == ZCL_CANCELLED);
    CHECK(written == 99 && request[0] == 0xa5 && memcmp(watch, &before, sizeof(before)) == 0);
}

static void freshness_and_retry(zcl_network network)
{
    zcl_sync_watch watch;
    init(&watch, network);
    (void)snapshot(&watch, 100, ZCL_BALANCE_UNAVAILABLE, false, ZCL_OK);
    const uint64_t first = begin(&watch, 100, 100);
    (void)snapshot(&watch, 100, ZCL_BALANCE_UNAVAILABLE, true, ZCL_OK);
    uint64_t unchanged = 88;
    CHECK(zcl_sync_watch_begin(&watch, 100, 100, 1, &unchanged) == ZCL_BUSY && unchanged == 88);
    finish(&watch, first, 100);
    zcl_sync_snapshot report = snapshot(&watch, 106, ZCL_BALANCE_UNVERIFIED, false, ZCL_OK);
    CHECK(report.report.network == network && report.report.balance.total == 993 && report.age_ms == 0);
    late(&watch, first);
    report = snapshot(&watch, 60105, ZCL_BALANCE_UNVERIFIED, false, ZCL_OK);
    CHECK(report.age_ms == 59999);
    (void)snapshot(&watch, 60106, ZCL_BALANCE_STALE, false, ZCL_OK);
    const uint64_t second = begin(&watch, 60106, 10);
    CHECK(second > first);
    late(&watch, first);
    (void)snapshot(&watch, 60115, ZCL_BALANCE_STALE, true, ZCL_OK);
    report = snapshot(&watch, 60116, ZCL_BALANCE_STALE, false, ZCL_TIMED_OUT);
    CHECK(report.report.balance.total == 993 && watch.attempt.candidate.balance.total == 0);
    late(&watch, second);
    const uint64_t third = begin(&watch, 60116, 100);
    finish(&watch, third, 60116);
    (void)snapshot(&watch, 60122, ZCL_BALANCE_UNVERIFIED, false, ZCL_OK);
    zcl_sync_watch_close(&watch);
    init(&watch, network); /* New process/owner starts empty, never fresh from persisted display state. */
    (void)snapshot(&watch, 0, ZCL_BALANCE_UNAVAILABLE, false, ZCL_OK);
}

static void fail_at_every_phase(void)
{
    for (unsigned stop = 1; stop <= 6; ++stop) {
        zcl_sync_watch watch;
        init(&watch, ZCL_MAINNET);
        uint64_t token = begin(&watch, 0, 100);
        finish(&watch, token, 0);
        token = begin(&watch, 10, 20);
        for (unsigned n = 1; n < stop; ++n) step(&watch, token, 10 + n, n);
        const zcl_status reason = stop % 2 == 0 ? ZCL_CANCELLED : ZCL_IO_FAILURE;
        CHECK(zcl_sync_watch_fail(&watch, token, reason) == reason);
        const zcl_sync_snapshot report = snapshot(&watch, 20, ZCL_BALANCE_STALE, false, reason);
        CHECK(report.report.balance.total == 993 && watch.attempt.candidate.balance.total == 0);
        late(&watch, token);
    }
}

static void deadline_and_clock(void)
{
    zcl_sync_watch watch;
    init(&watch, ZCL_MAINNET);
    uint64_t token = begin(&watch, 100, 10);
    for (unsigned n = 1; n < 6; ++n) step(&watch, token, 100 + n, n);
    uint8_t request[256];
    size_t written = 0;
    CHECK(zcl_sync_watch_request(&watch, token, 109, request, sizeof(request), &written) == ZCL_OK);
    const size_t length = sync_fixture_reply(ZCL_MAINNET, 6, watch.attempt.request_id, frame, sizeof(frame));
    CHECK(zcl_sync_watch_reply(&watch, token, 110, (const uint8_t *)frame, length) == ZCL_TIMED_OUT);
    (void)snapshot(&watch, 110, ZCL_BALANCE_UNAVAILABLE, false, ZCL_TIMED_OUT);
    token = begin(&watch, 110, 100);
    finish(&watch, token, 110);
    (void)snapshot(&watch, 115, ZCL_BALANCE_UNAVAILABLE, false, ZCL_IO_UNCERTAIN);
    token = begin(&watch, 115, 100);
    CHECK(zcl_sync_watch_request(&watch, token, 114, request, sizeof(request), &written) == ZCL_IO_UNCERTAIN);
    (void)snapshot(&watch, 114, ZCL_BALANCE_UNAVAILABLE, false, ZCL_IO_UNCERTAIN);
    token = begin(&watch, UINT64_MAX - 10, 10);
    (void)snapshot(&watch, UINT64_MAX, ZCL_BALANCE_UNAVAILABLE, false, ZCL_TIMED_OUT);
    late(&watch, token);
}

static void arguments_and_malformed(void)
{
    zcl_sync_watch watch;
    init(&watch, ZCL_MAINNET);
    uint64_t token = 77;
    CHECK(zcl_sync_watch_begin(&watch, UINT64_MAX, 1, 1, &token) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_watch_begin(&watch, 0, 0, 1, &token) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_watch_begin(&watch, 0, 30001, 1, &token) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_watch_begin(&watch, 0, 1, UINT32_MAX, &token) == ZCL_OUT_OF_RANGE);
    CHECK(token == 77 && !watch.in_flight);
    watch.sequence = UINT64_MAX; /* Explicit saturation fixture, no token reuse. */
    CHECK(zcl_sync_watch_begin(&watch, 0, 1, 1, &token) == ZCL_RESOURCE_EXHAUSTED && token == 77);
    init(&watch, ZCL_MAINNET);
    token = begin(&watch, 0, 100);
    CHECK(zcl_sync_watch_fail(&watch, token, ZCL_OK) == ZCL_INVALID_ARGUMENT && watch.in_flight);
    CHECK(zcl_sync_watch_reply(&watch, token, 1, (const uint8_t *)"{}", 2) == ZCL_INVALID_ENCODING);
    (void)snapshot(&watch, 1, ZCL_BALANCE_UNAVAILABLE, false, ZCL_INVALID_ENCODING);
    zcl_sync_snapshot output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    zcl_sync_watch_close(&watch);
    CHECK(zcl_sync_watch_snapshot(&watch, 2, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK(zcl_sync_watch_init(&watch, NULL, 0, ZCL_MAINNET, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(!watch.initialized);
    CHECK(zcl_sync_watch_init(NULL, NULL, 0, ZCL_MAINNET, NULL, 0) == ZCL_INVALID_ARGUMENT);
    zcl_sync_watch_close(NULL);
}

int main(void)
{
    freshness_and_retry(ZCL_MAINNET); freshness_and_retry(ZCL_TESTNET);
    fail_at_every_phase(); deadline_and_clock(); arguments_and_malformed();
    puts("Sync watch: complete-only unverified reports, stale/offline state, deadline/clock bounds and late-token rejection passed");
    return 0;
}
