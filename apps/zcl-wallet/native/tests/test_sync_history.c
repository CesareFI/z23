/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_watch.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "History sync check failed at %d\n", __LINE__); abort(); } } while (0)
static char frame[4096];

static void no_report(const zcl_sync *session, zcl_status expected)
{
    zcl_sync_report report, before;
    memset(&report, 0xa5, sizeof(report));
    memcpy(&before, &report, sizeof(before));
    CHECK(zcl_sync_get_report(session, &report) == expected);
    CHECK(memcmp(&report, &before, sizeof(report)) == 0);
}

static void cleared(const zcl_sync *session)
{
    const zcl_reported_history empty = {0};
    CHECK(!session->candidate.has_history);
    CHECK(memcmp(&session->candidate.history, &empty, sizeof(empty)) == 0);
    CHECK(session->candidate.balance.total == 0 && session->candidate.tip.height == 0);
}

static void request(zcl_sync *session)
{
    uint8_t output[256] = {0};
    size_t length = 777;
    zcl_sync before = *session;
    CHECK(zcl_sync_request(session, output, 1, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == 777 && memcmp(&before, session, sizeof(before)) == 0);
    CHECK(zcl_sync_request(session, output, sizeof(output) - 1, &length) == ZCL_OK);
    CHECK(length > 0 && length < sizeof(output));
    output[length] = 0;
    CHECK((strstr((const char *)output, "scripthash") != NULL) ==
        (session->phase == ZCL_SYNC_BALANCE || session->phase == ZCL_SYNC_HISTORY));
    CHECK((strstr((const char *)output, "get_history") != NULL) == (session->phase == ZCL_SYNC_HISTORY));
    CHECK(zcl_sync_request(session, output, sizeof(output), &length) == ZCL_BUSY);
}

static void advance(zcl_sync *session, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        no_report(session, ZCL_BUSY);
        request(session);
        const size_t length = sync_fixture_reply(session->candidate.network,
            (unsigned)session->phase, session->request_id, frame, sizeof(frame));
        CHECK(zcl_sync_reply(session, (const uint8_t *)frame, length) == ZCL_OK);
    }
}

static void complete(zcl_network network, uint32_t first_id)
{
    zcl_sync session;
    sync_fixture_start_with_history(&session, network, first_id);
    advance(&session, 5);
    CHECK(session.phase == ZCL_SYNC_HISTORY && session.request_id == first_id + 5);
    advance(&session, 1);
    CHECK(session.phase == ZCL_SYNC_TIP_AFTER && session.candidate.has_history);
    no_report(&session, ZCL_BUSY);
    advance(&session, 1);
    CHECK(session.phase == ZCL_SYNC_DONE && session.request_id == first_id + 6);
    zcl_sync_report report = {0};
    CHECK(zcl_sync_get_report(&session, &report) == ZCL_OK);
    CHECK(report.has_history && report.history.count == 2 && report.network == network);
    CHECK(report.history.entries[0].reported_height == 0 && report.history.entries[1].reported_height == -1);
    CHECK(report.history.entries[1].txid[31] == 1 && report.balance.total == 993);
    CHECK(zcl_sync_abort(&session, ZCL_CANCELLED) == ZCL_CANCELLED);
    no_report(&session, ZCL_CANCELLED);
    cleared(&session);
}

static void phase_failures(void)
{
    for (unsigned stop = 0; stop < 7; ++stop) {
        zcl_sync session;
        sync_fixture_start_with_history(&session, ZCL_MAINNET, 10);
        advance(&session, stop);
        request(&session);
        const size_t length = sync_fixture_reply(ZCL_MAINNET, (unsigned)session.phase,
            session.request_id + 1, frame, sizeof(frame));
        CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_INVALID_ENCODING);
        no_report(&session, ZCL_INVALID_ENCODING);
        cleared(&session);
        sync_fixture_start_with_history(&session, ZCL_MAINNET, 10);
        advance(&session, stop);
        CHECK(zcl_sync_abort(&session, ZCL_TIMED_OUT) == ZCL_TIMED_OUT);
        cleared(&session);
        no_report(&session, ZCL_TIMED_OUT);
    }
}

static void tip_consistency(void)
{
    zcl_sync session;
    for (unsigned history = 0; history < 3; ++history) {
        sync_fixture_start_with_history(&session, ZCL_MAINNET, 1);
        advance(&session, history == 0 ? 5 : 6);
        request(&session);
        const size_t length = sync_fixture_reply(ZCL_MAINNET, (unsigned)session.phase,
            session.request_id, frame, sizeof(frame));
        if (history == 2) {
            char *hex = strstr(frame, "\"hex\":\"");
            CHECK(hex != NULL);
            hex[7] = '1'; /* Different raw header, same reported height. */
        } else {
            char *height = strstr(frame, "\"height\":0");
            CHECK(height != NULL);
            height[9] = '1'; /* history beyond initial tip, or changed final tip */
        }
        CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_IO_UNCERTAIN);
        no_report(&session, ZCL_IO_UNCERTAIN);
        cleared(&session);
    }
    /* Empty history is explicit success only after the final tip. */
    sync_fixture_start_with_history(&session, ZCL_MAINNET, 1);
    advance(&session, 5);
    request(&session);
    static const uint8_t empty[] = "{\"id\":6,\"result\":[]}";
    CHECK(zcl_sync_reply(&session, empty, sizeof(empty) - 1) == ZCL_OK);
    no_report(&session, ZCL_BUSY);
    advance(&session, 1);
    zcl_sync_report report = {0};
    CHECK(zcl_sync_get_report(&session, &report) == ZCL_OK);
    CHECK(report.has_history && report.history.count == 0);
}

static void claimed_height_match(void)
{
    /* Synthetic server assertions at height 1, not a consensus-valid block. */
    zcl_sync session;
    sync_fixture_start_with_history(&session, ZCL_TESTNET, 1);
    for (unsigned step = 0; step < 7; ++step) {
        request(&session);
        const size_t length = sync_fixture_reply(ZCL_TESTNET, (unsigned)session.phase,
            session.request_id, frame, sizeof(frame));
        if (session.phase == ZCL_SYNC_TIP_BEFORE || session.phase == ZCL_SYNC_TIP_AFTER ||
            session.phase == ZCL_SYNC_HISTORY) {
            char *height = strstr(frame, "\"height\":0");
            CHECK(height != NULL);
            height[9] = '1';
        }
        CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_OK);
    }
    zcl_sync_report report = {0};
    CHECK(zcl_sync_get_report(&session, &report) == ZCL_OK);
    CHECK(report.has_history && report.history.entries[0].reported_height == 1 && report.tip.height == 1);
}

static void id_bounds(void)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_TESTNET, UINT32_MAX - 5);
    uint8_t address[35];
    memcpy(address, session.candidate.address, sizeof(address));
    CHECK(zcl_sync_start_with_history(&session, address, sizeof(address), ZCL_TESTNET, UINT32_MAX - 5) == ZCL_OUT_OF_RANGE);
    cleared(&session);
    CHECK(zcl_sync_start_with_history(NULL, address, sizeof(address), ZCL_TESTNET, 1) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_sync_start_with_history(&session, address, sizeof(address), ZCL_MAINNET, 1) == ZCL_UNSUPPORTED);
    CHECK(zcl_sync_start_with_history(&session, address, sizeof(address), ZCL_TESTNET, 0) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_TESTNET, UINT32_MAX - 5) == ZCL_OK);
    advance(&session, 6);
    CHECK(!session.candidate.has_history && session.request_id == UINT32_MAX);
}

static void watch_advance(zcl_sync_watch *watch, uint64_t token, uint64_t now, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        uint8_t output[256];
        size_t length = 0;
        CHECK(zcl_sync_watch_request(watch, token, now, output, sizeof(output), &length) == ZCL_OK);
        length = sync_fixture_reply(watch->network, (unsigned)watch->attempt.phase,
            watch->attempt.request_id, frame, sizeof(frame));
        CHECK(zcl_sync_watch_reply(watch, token, now, (const uint8_t *)frame, length) == ZCL_OK);
    }
}

static void watch_lifetime(void)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    uint8_t source[32] = {42};
    zcl_sync_watch watch;
    CHECK(zcl_sync_watch_init_with_history(&watch, fixture.candidate.address, 35,
        ZCL_MAINNET, source, sizeof(source)) == ZCL_OK);
    source[0] = 43;
    uint64_t token = 777;
    CHECK(zcl_sync_watch_begin(&watch, 10, 100, UINT32_MAX - 5, &token) == ZCL_OUT_OF_RANGE);
    CHECK(token == 777 && watch.sequence == 0 && watch.clock_ms == 0);
    CHECK(zcl_sync_watch_begin(&watch, 10, 100, UINT32_MAX - 6, &token) == ZCL_OK);
    watch_advance(&watch, token, 10, 6);
    zcl_sync_snapshot snapshot = {0};
    CHECK(zcl_sync_watch_snapshot(&watch, 10, &snapshot) == ZCL_OK);
    CHECK(snapshot.freshness == ZCL_BALANCE_UNAVAILABLE && !snapshot.report.has_history);
    watch_advance(&watch, token, 20, 1);
    CHECK(zcl_sync_watch_snapshot(&watch, 20, &snapshot) == ZCL_OK);
    CHECK(snapshot.freshness == ZCL_BALANCE_UNVERIFIED && snapshot.report.has_history);
    CHECK(snapshot.source_id[0] == 42 && snapshot.report.history.count == 2);
    const uint64_t previous = token;
    CHECK(zcl_sync_watch_begin(&watch, 30, 100, 1, &token) == ZCL_OK);
    watch_advance(&watch, token, 30, 6);
    CHECK(zcl_sync_watch_reply(&watch, previous, UINT64_MAX, NULL, 0) == ZCL_CANCELLED);
    CHECK(watch.clock_ms == 30 && watch.in_flight);
    CHECK(zcl_sync_watch_snapshot(&watch, 130, &snapshot) == ZCL_OK);
    CHECK(snapshot.freshness == ZCL_BALANCE_STALE && snapshot.last_fault == ZCL_TIMED_OUT);
    CHECK(snapshot.report.has_history && snapshot.report.history.count == 2);
    cleared(&watch.attempt);
    CHECK(zcl_sync_watch_snapshot(&watch, 129, &snapshot) == ZCL_OK);
    CHECK(snapshot.freshness == ZCL_BALANCE_UNAVAILABLE && !snapshot.report.has_history);
    CHECK(snapshot.report.history.count == 0 && snapshot.last_fault == ZCL_IO_UNCERTAIN);
    zcl_sync_watch_close(&watch);
    CHECK(zcl_sync_watch_init_with_history(&watch, fixture.candidate.address, 35,
        ZCL_MAINNET, source, sizeof(source)) == ZCL_OK);
    CHECK(zcl_sync_watch_snapshot(&watch, 200, &snapshot) == ZCL_OK);
    CHECK(snapshot.freshness == ZCL_BALANCE_UNAVAILABLE && snapshot.source_id[0] == 43);
    CHECK(!snapshot.report.has_history && !snapshot.refreshing);
    CHECK(zcl_sync_watch_reply(&watch, token, 200, NULL, 0) == ZCL_CANCELLED);
    zcl_sync_watch_close(&watch);
}

int main(void)
{
    complete(ZCL_MAINNET, 1); complete(ZCL_TESTNET, UINT32_MAX - 6);
    phase_failures(); tip_consistency(); claimed_height_match(); id_bounds(); watch_lifetime();
    puts("Optional history complete-only reports, tip/ID bounds, cancellation and source lifetime passed");
    return 0;
}
