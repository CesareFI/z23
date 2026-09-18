/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_sync_watch.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Public genesis and zero-hash address fixtures only. No sockets, keys,
 * storage, TLS, Android scheduling or chain-validation claims. One checked
 * enclosing allocation owns all large buffers, outside the measured batches. */
typedef struct {
    zcl_sync_watch watch;
    zcl_sync_snapshot snapshot;
    zcl_electrum_line line;
    char frames[7][4096];
    size_t lengths[7], steps, chunk;
    uint64_t now;
    uint8_t address[35], source[32];
    zcl_network network;
    bool history;
} benchmark_fixture;

#define CHECK(v) do { if (!(v)) { \
    fprintf(stderr, "Read-only sync benchmark failed at %d\n", __LINE__); return 1; \
} } while (0)

static int prepare(benchmark_fixture *fixture, zcl_network network, bool history, size_t chunk)
{
    const uint8_t hash[20] = {0};
    size_t length = 0;
    fixture->network = network;
    fixture->history = history;
    fixture->chunk = chunk;
    fixture->steps = history ? 7 : 6;
    fixture->source[0] = 1;
    CHECK(chunk > 0 && chunk <= sizeof(fixture->frames[0]));
    CHECK(zcl_address_from_hash(hash, sizeof(hash), network, fixture->address,
        sizeof(fixture->address), &length) == ZCL_OK);
    const zcl_status status = history
        ? zcl_sync_watch_init_with_history(&fixture->watch, fixture->address, length,
            network, fixture->source, sizeof(fixture->source))
        : zcl_sync_watch_init(&fixture->watch, fixture->address, length,
            network, fixture->source, sizeof(fixture->source));
    CHECK(status == ZCL_OK);
    static const unsigned history_steps[] = {1, 2, 3, 4, 5, ZCL_SYNC_HISTORY, 6};
    for (size_t step = 0; step < fixture->steps; ++step) {
        const unsigned reply = history ? history_steps[step] : (unsigned)step + 1;
        length = sync_fixture_reply(network, reply, (uint32_t)step + 1,
            fixture->frames[step], sizeof(fixture->frames[step]));
        CHECK(length < sizeof(fixture->frames[step]));
        fixture->frames[step][length] = '\n';
        fixture->lengths[step] = length + 1;
    }
    return 0;
}

static int exchange(benchmark_fixture *fixture, uint64_t token, size_t step)
{
    uint8_t request[ZCL_ELECTRUM_REQUEST_MAX];
    size_t written = 0;
    CHECK(zcl_sync_watch_request(&fixture->watch, token, fixture->now,
        request, sizeof(request), &written) == ZCL_OK);
    CHECK(written > 0 && written <= sizeof(request) && request[written - 1] == '\n');
    zcl_electrum_line_reset(&fixture->line);
    const size_t length = fixture->lengths[step];
    for (size_t offset = 0; offset < length;) {
        const size_t remaining = length - offset;
        const size_t chunk = remaining < fixture->chunk ? remaining : fixture->chunk;
        size_t consumed = 0;
        CHECK(zcl_electrum_line_feed(&fixture->line,
            (const uint8_t *)fixture->frames[step] + offset, chunk, &consumed) == ZCL_OK);
        CHECK(consumed == chunk);
        offset += consumed;
        CHECK(fixture->line.ready == (offset == length));
    }
    CHECK(fixture->line.used == length - 1);
    CHECK(memcmp(fixture->line.bytes, fixture->frames[step], length - 1) == 0);
    CHECK(zcl_sync_watch_reply(&fixture->watch, token, fixture->now,
        fixture->line.bytes, fixture->line.used) == ZCL_OK);
    return 0;
}

static int verify_report(const benchmark_fixture *fixture)
{
    const zcl_sync_snapshot *snapshot = &fixture->snapshot;
    const zcl_sync_report *report = &snapshot->report;
    CHECK(snapshot->freshness == ZCL_BALANCE_UNVERIFIED && !snapshot->refreshing);
    CHECK(snapshot->last_fault == ZCL_OK && snapshot->age_ms == 0);
    CHECK(snapshot->next_change_ms == ZCL_SYNC_FRESH_MS);
    CHECK(memcmp(snapshot->source_id, fixture->source, sizeof(fixture->source)) == 0);
    CHECK(report->network == fixture->network && report->tip.height == 0);
    CHECK(memcmp(report->address, fixture->address, sizeof(fixture->address)) == 0);
    CHECK(report->balance.confirmed == 1000 && report->balance.pending_delta == -7 &&
        report->balance.total == 993);
    CHECK(report->has_history == fixture->history);
    CHECK(report->history.count == (fixture->history ? 2u : 0u));
    if (fixture->history) {
        uint8_t expected[32] = {0};
        CHECK(memcmp(report->history.entries[0].txid, expected, sizeof(expected)) == 0);
        expected[31] = 1;
        CHECK(memcmp(report->history.entries[1].txid, expected, sizeof(expected)) == 0);
        CHECK(report->history.entries[0].reported_height == 0 &&
            report->history.entries[1].reported_height == -1);
    }
    return 0;
}

static int batch(benchmark_fixture *fixture, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        CHECK(fixture->now < UINT64_MAX);
        ++fixture->now;
        uint64_t token = 0;
        CHECK(zcl_sync_watch_begin(&fixture->watch, fixture->now, 100, 1, &token) == ZCL_OK);
        CHECK(token != 0);
        for (size_t step = 0; step < fixture->steps; ++step)
            CHECK(exchange(fixture, token, step) == 0);
        CHECK(zcl_sync_watch_snapshot(&fixture->watch, fixture->now, &fixture->snapshot) == ZCL_OK);
        CHECK(verify_report(fixture) == 0);
    }
    return 0;
}

static double elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
        (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static int sample(benchmark_fixture *fixture, size_t run)
{
    struct timespec wall_start, wall_end, cpu_start, cpu_end;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &wall_start) == 0);
    CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start) == 0);
    const size_t count = 100;
    CHECK(batch(fixture, count) == 0);
    CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end) == 0);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &wall_end) == 0);
    const double wall = elapsed_ms(&wall_start, &wall_end);
    const double cpu = elapsed_ms(&cpu_start, &cpu_end);
    CHECK(wall > 0.0 && cpu > 0.0);
    CHECK(printf("network=%u history=%u chunk=%zu sample=%zu count=%zu "
        "wall_us_per_sync=%.3f cpu_us_per_sync=%.3f workspace_bytes=%zu\n",
        (unsigned)fixture->network, (unsigned)fixture->history, fixture->chunk, run, count,
        wall * 1000.0 / (double)count, cpu * 1000.0 / (double)count, sizeof(*fixture)) >= 0);
    return 0;
}

static int measure(zcl_network network, bool history, size_t chunk)
{
    benchmark_fixture *fixture = calloc(1, sizeof(*fixture));
    CHECK(fixture != NULL);
    int failed = 1;
    if (prepare(fixture, network, history, chunk) != 0 || batch(fixture, 10) != 0) goto cleanup;
    for (size_t run = 0; run < 5; ++run) if (sample(fixture, run) != 0) goto cleanup;
    failed = 0;
cleanup:
    zcl_sync_watch_close(&fixture->watch);
    zcl_electrum_line_reset(&fixture->line);
    free(fixture);
    return failed;
}

int main(void)
{
    const size_t chunks[] = {1, 256, 4096};
    const zcl_network networks[] = {ZCL_MAINNET, ZCL_TESTNET};
    for (size_t network = 0; network < sizeof(networks) / sizeof(networks[0]); ++network)
        for (size_t history = 0; history < 2; ++history)
            for (size_t chunk = 0; chunk < sizeof(chunks) / sizeof(chunks[0]); ++chunk)
                if (measure(networks[network], history != 0, chunks[chunk]) != 0) return 1;
    return fflush(stdout) == 0 ? 0 : 1;
}
