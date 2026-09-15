/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Public zero-entropy BIP39 vectors, testnet m/44'/1'/0'/1/19. Expected
 * addresses were independently derived with the shared host OpenSSL oracle.
 * No storage, network, OS entropy or real wallet data enters this benchmark. */
typedef struct {
    uint8_t entropy[32], blinding[64], header[80];
    size_t entropy_len;
    const uint8_t *expected;
} benchmark_fixture;

static int batch(const benchmark_fixture *fixture, size_t count)
{
    uint8_t address[35] = {0};
    for (size_t i = 0; i < count; ++i) {
        const zcl_status status = zcl_wallet_recovered_change(fixture->header, sizeof(fixture->header),
            fixture->entropy, fixture->entropy_len, 19, fixture->blinding, sizeof(fixture->blinding),
            address, sizeof(address));
        const int equal = memcmp(address, fixture->expected, sizeof(address)) == 0;
        zcl_secure_zero(address, sizeof(address));
        if (status != ZCL_OK || !equal) {
            fputs("Recovered change benchmark failed independent public address verification\n", stderr);
            return 1;
        }
    }
    return 0;
}

static int sample(const benchmark_fixture *fixture, size_t run)
{
    struct timespec wall_start, wall_end, cpu_start, cpu_end;
    if (clock_gettime(CLOCK_MONOTONIC, &wall_start) != 0 ||
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start) != 0) {
        perror("change benchmark start clock"); return 1;
    }
    const size_t count = 100;
    if (batch(fixture, count) != 0) return 1;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0) {
        perror("change benchmark end clock"); return 1;
    }
    const double wall = (double)(wall_end.tv_sec - wall_start.tv_sec) * 1000.0 +
        (double)(wall_end.tv_nsec - wall_start.tv_nsec) / 1000000.0;
    const double cpu = (double)(cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0 +
        (double)(cpu_end.tv_nsec - cpu_start.tv_nsec) / 1000000.0;
    if (wall <= 0.0 || cpu <= 0.0 || wall > 300000.0) {
        fputs("Invalid change benchmark clock span\n", stderr); return 1;
    }
    return printf("entropy_bytes=%zu sample=%zu count=%zu wall_ms_per_change=%.6f cpu_ms_per_change=%.6f\n",
        fixture->entropy_len, run, count, wall / (double)count, cpu / (double)count) < 0 ? 1 : 0;
}

static int measure(size_t length, const uint8_t expected[35])
{
    benchmark_fixture fixture = {.entropy_len = length, .expected = expected};
    memset(fixture.blinding, 1, 32); memset(fixture.blinding + 32, 2, 32);
    int failed = 1;
    if (zcl_wallet_header_create(fixture.entropy, length, ZCL_TESTNET, fixture.blinding, 32,
        fixture.header, sizeof(fixture.header)) != ZCL_OK) goto cleanup;
    if (batch(&fixture, 10) != 0) goto cleanup;
    for (size_t run = 0; run < 5; ++run) if (sample(&fixture, run) != 0) goto cleanup;
    failed = 0;
cleanup:
    zcl_secure_zero(&fixture, sizeof(fixture));
    return failed;
}

int main(void)
{
    static const uint8_t short_expected[] = "tmWdoCDUViginL7DVpVu2YSB3GXm2K5b2ZN";
    static const uint8_t long_expected[] = "tmVCwjCPeNerUPfeYZtjzUuXtNHEyQWrHYT";
    return measure(16, short_expected) || measure(32, long_expected);
}
