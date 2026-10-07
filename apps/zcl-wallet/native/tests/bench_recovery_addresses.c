/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_recovery.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Same binary, public zero entropy, testnet account0/change indices19..34.
 * Compare unchanged single-address calls with seed/context reuse. No storage,
 * network, real entropy or wallet data; timing is not a pass/fail threshold. */
typedef struct { uint8_t entropy[32], blinding[32], expected[560]; size_t length; } fixture;
typedef zcl_status (*derive_fn)(const fixture *, uint8_t *);

static zcl_status singles(const fixture *item, uint8_t *output)
{
    for (uint32_t i = 0; i < 16; ++i) {
        size_t length = 0;
        zcl_status status = zcl_change_from_entropy(item->entropy, item->length, ZCL_TESTNET,
            19 + i, item->blinding, sizeof(item->blinding), output + (size_t)i * 35, 35, &length);
        if (status != ZCL_OK) return status;
        if (length != 35) return ZCL_CRYPTO_FAILURE;
    }
    return ZCL_OK;
}

static zcl_status batched(const fixture *item, uint8_t *output)
{
    return zcl_recovery_address_batch(item->entropy, item->length, ZCL_TESTNET, 1, 19, 16,
        item->blinding, sizeof(item->blinding), output, sizeof(item->expected));
}

static int exercise(const fixture *item, derive_fn derive, size_t repetitions)
{
    uint8_t output[560] = {0};
    for (size_t i = 0; i < repetitions; ++i) {
        const zcl_status status = derive(item, output);
        const int equal = memcmp(output, item->expected, sizeof(output)) == 0;
        zcl_secure_zero(output, sizeof(output));
        if (status != ZCL_OK || !equal) {
            fputs("Recovery benchmark address verification failed\n", stderr); return 1;
        }
    }
    return 0;
}

static double elapsed_ms(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
        (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}

static int sample(const fixture *item, derive_fn derive, const char *name, size_t run)
{
    struct timespec wall_start, wall_end, cpu_start, cpu_end;
    if (clock_gettime(CLOCK_MONOTONIC, &wall_start) != 0 ||
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start) != 0) {
        perror("recovery benchmark start clock"); return 1;
    }
    const size_t repetitions = 10;
    if (exercise(item, derive, repetitions) != 0) return 1;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0) {
        perror("recovery benchmark end clock"); return 1;
    }
    const double wall = elapsed_ms(wall_start, wall_end), cpu = elapsed_ms(cpu_start, cpu_end);
    if (wall <= 0 || cpu <= 0 || wall > 300000) {
        fputs("Invalid recovery benchmark clock span\n", stderr); return 1;
    }
    return printf("entropy_bytes=%zu method=%s sample=%zu batches=%zu addresses_per_batch=16 wall_ms_per_batch=%.6f cpu_ms_per_batch=%.6f\n",
        item->length, name, run, repetitions, wall / (double)repetitions,
        cpu / (double)repetitions) < 0 ? 1 : 0;
}

static int measure(size_t length)
{
    fixture item = {.length = length};
    memset(item.blinding, 1, sizeof(item.blinding));
    int failed = 1;
    if (singles(&item, item.expected) != ZCL_OK) goto cleanup;
    if (exercise(&item, singles, 1) != 0 || exercise(&item, batched, 1) != 0) goto cleanup;
    static const derive_fn methods[] = {singles, batched};
    static const char *const names[] = {"singles", "batch"};
    for (size_t run = 0; run < 7; ++run)
        for (size_t i = 0; i < 2; ++i) {
            const size_t which = (run + i) % 2;
            if (sample(&item, methods[which], names[which], run) != 0) goto cleanup;
        }
    failed = 0;
cleanup:
    zcl_secure_zero(&item, sizeof(item));
    return failed;
}

int main(void)
{
    return measure(16) || measure(32);
}
