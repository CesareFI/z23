/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <string.h>
#include <time.h>

/* Invocation-owned temporary stores and inert public ciphertext only. No path
 * arguments, OS entropy, wallet authentication, network or production data.
 * Every measured operation verifies the complete result. Durability is never
 * disabled; host filesystem/cache behavior remains part of the observation. */
typedef enum {
    READ_EMPTY, READ_PENDING, READ_COMMITTED, PROMOTE_COMMITTED, CREATE_AND_READ,
    PROFILE_COUNT
} profile;

enum { STORE_COUNT = 8 };
typedef struct { storage_fixture stores[STORE_COUNT]; size_t opened; } store_pool;
typedef struct { struct timespec wall, cpu; } clock_sample;
typedef struct { uint8_t bytes[140]; size_t length; } public_record;

static int close_pool(store_pool *pool)
{
    int failed = 0;
    while (pool->opened != 0) {
        --pool->opened;
        failed |= fixture_close(&pool->stores[pool->opened]);
    }
    return failed;
}

static int prepare_pool(store_pool *pool, profile kind, const public_record *record)
{
    for (size_t i = 0; i < STORE_COUNT; ++i) {
        storage_fixture *store = &pool->stores[i];
        if (fixture_open(store) != 0) return 1;
        ++pool->opened; /* Cleanup owns the descriptor before populating files. */
        if (kind == READ_PENDING) {
            if (fixture_write(store, ".wallet.pending", record->bytes, record->length) != 0) return 1;
        } else if (kind == READ_COMMITTED || kind == PROMOTE_COMMITTED) {
            if (fixture_create(store, record->bytes, record->length) != ZCL_OK) return 1;
        }
    }
    return 0;
}

static int check_read(const storage_fixture *store, profile kind, const public_record *record)
{
    uint8_t output[140], unchanged[140];
    memset(output, 0xa5, sizeof(output));
    memcpy(unchanged, output, sizeof(unchanged));
    size_t length = SIZE_MAX;
    bool pending = true;
    const zcl_status status = fixture_read(store, output, sizeof(output), &length, &pending);
    if (kind == READ_EMPTY) {
        CHECK(status == ZCL_NOT_FOUND && length == SIZE_MAX && pending);
        CHECK(memcmp(output, unchanged, sizeof(output)) == 0);
        return 0;
    }
    CHECK(status == ZCL_OK && length == record->length);
    CHECK(pending == (kind == READ_PENDING));
    CHECK(memcmp(output, record->bytes, length) == 0);
    CHECK(memcmp(output + length, unchanged + length, sizeof(output) - length) == 0);
    return 0;
}

static int operation(const storage_fixture *store, profile kind, const public_record *record)
{
    if (kind == CREATE_AND_READ) {
        CHECK(fixture_create(store, record->bytes, record->length) == ZCL_OK);
    } else if (kind == PROMOTE_COMMITTED) {
        CHECK(fixture_promote(store, record->bytes, record->length) == ZCL_OK);
    }
    return check_read(store, kind, record);
}

static int batch(const store_pool *pool, profile kind, const public_record *record, size_t count)
{
    CHECK(pool->opened == STORE_COUNT);
    for (size_t i = 0; i < count; ++i) {
        if (operation(&pool->stores[i % STORE_COUNT], kind, record) != 0) return 1;
    }
    return 0;
}

static int read_clock(clock_sample *sample)
{
    if (clock_gettime(CLOCK_MONOTONIC, &sample->wall) != 0 ||
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &sample->cpu) != 0) {
        perror("storage benchmark clock");
        return 1;
    }
    return 0;
}

static double elapsed_us(const struct timespec *start, const struct timespec *end)
{
    return ((double)end->tv_sec - (double)start->tv_sec) * 1000000.0 +
        ((double)end->tv_nsec - (double)start->tv_nsec) / 1000.0;
}

static int report(profile kind, size_t run, size_t count,
    const clock_sample *start, const clock_sample *end)
{
    static const char *const names[PROFILE_COUNT] = {
        "read_empty", "read_pending", "read_committed", "promote_and_read", "create_and_read"
    };
    const double wall = elapsed_us(&start->wall, &end->wall);
    const double cpu = elapsed_us(&start->cpu, &end->cpu);
    CHECK(wall > 0.0 && cpu > 0.0 && wall < 300000000.0);
    return printf("profile=%s sample=%zu count=%zu wall_us_per_operation=%.6f cpu_us_per_operation=%.6f\n",
        names[kind], run, count, wall / (double)count, cpu / (double)count) < 0 ? 1 : 0;
}

static int measure(profile kind, const public_record *record, size_t run, bool emit)
{
    store_pool pool = {0};
    clock_sample start = {0}, end = {0};
    /* Creation uses every fresh directory exactly once. The other profiles
     * measure repeated opens across eight stores, not cold-cache latency. */
    const size_t count = kind == CREATE_AND_READ ? STORE_COUNT : 128;
    int failed = 1;
    if (prepare_pool(&pool, kind, record) != 0) goto cleanup;
    if (read_clock(&start) != 0) goto cleanup;
    if (batch(&pool, kind, record, count) != 0) goto cleanup;
    if (read_clock(&end) != 0) goto cleanup;
    failed = emit ? report(kind, run, count, &start, &end) : 0;
cleanup:
    if (close_pool(&pool) != 0) failed = 1;
    if (failed != 0) fputs("storage benchmark fixture or verification failed\n", stderr);
    return failed;
}

int main(void)
{
    public_record record = {0};
    if (fixture_record(record.bytes, sizeof(record.bytes), &record.length) != 0) return 1;
    for (unsigned i = 0; i < PROFILE_COUNT; ++i) {
        const profile kind = (profile)i;
        if (measure(kind, &record, 0, false) != 0) return 1;
        for (size_t run = 0; run < 5; ++run) {
            if (measure(kind, &record, run, true) != 0) return 1;
        }
    }
    return 0;
}
