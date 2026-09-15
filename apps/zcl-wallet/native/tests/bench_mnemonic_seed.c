/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_keys.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Published BIP39 fixtures only. No OS entropy, wallet or network access. */
typedef struct {
    uint8_t entropy_hex[65];
    size_t entropy_len;
    uint8_t mnemonic[216];
    size_t mnemonic_len;
    uint8_t seed_hex[129];
} mnemonic_fixture;
static const mnemonic_fixture fixtures[] = {
#include "bip39_vectors.inc"
};

static unsigned digit(uint8_t value)
{
    if (value >= '0' && value <= '9')
        return (unsigned)(value - '0');
    if (value >= 'a' && value <= 'f')
        return (unsigned)(value - 'a') + 10U;
    return 16;
}

static int expected_seed(const mnemonic_fixture *fixture, uint8_t expected[64])
{
    for (size_t i = 0; i < 64; ++i) {
        unsigned high = digit(fixture->seed_hex[2 * i]);
        unsigned low = digit(fixture->seed_hex[2 * i + 1]);
        if (high > 15 || low > 15) {
            fputs("invalid public seed fixture\n", stderr);
            return 1;
        }
        expected[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int derive_batch(const mnemonic_fixture *fixture,
                        const uint8_t expected[64], size_t count)
{
    static const uint8_t passphrase[] = "TREZOR";
    uint8_t seed[64] = {0};
    for (size_t iteration = 0; iteration < count; ++iteration) {
        zcl_status status = zcl_mnemonic_seed(fixture->mnemonic,
            fixture->mnemonic_len, passphrase, sizeof(passphrase) - 1,
            seed, sizeof(seed));
        int matches = memcmp(seed, expected, sizeof(seed)) == 0;
        zcl_secure_zero(seed, sizeof(seed));
        if (status != ZCL_OK || !matches) {
            fputs("seed derivation failed public-vector verification\n", stderr);
            return 1;
        }
    }
    return 0;
}

static int sample(const mnemonic_fixture *fixture, const uint8_t expected[64],
                   size_t sample_id)
{
    struct timespec wall_start, wall_end, cpu_start, cpu_end;
    if (clock_gettime(CLOCK_MONOTONIC, &wall_start) != 0 ||
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start) != 0) {
        perror("seed benchmark start clock");
        return 1;
    }
    const size_t count = 200;
    if (derive_batch(fixture, expected, count) != 0)
        return 1;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0) {
        perror("seed benchmark end clock");
        return 1;
    }
    double wall_ms = (double)(wall_end.tv_sec - wall_start.tv_sec) * 1000.0 +
        (double)(wall_end.tv_nsec - wall_start.tv_nsec) / 1000000.0;
    double cpu_ms = (double)(cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0 +
        (double)(cpu_end.tv_nsec - cpu_start.tv_nsec) / 1000000.0;
    if (wall_ms <= 0.0 || cpu_ms <= 0.0 || wall_ms > 300000.0) {
        fputs("invalid seed benchmark clock span\n", stderr);
        return 1;
    }
    printf("text_bytes=%zu sample=%zu count=%zu wall_ms_per_seed=%.6f cpu_ms_per_seed=%.6f\n",
        fixture->mnemonic_len, sample_id, count, wall_ms / (double)count,
        cpu_ms / (double)count);
    return 0;
}

int main(void)
{
    const size_t indices[] = {0, 8};
    for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
        const mnemonic_fixture *fixture = &fixtures[indices[i]];
        uint8_t expected[64] = {0};
        if (expected_seed(fixture, expected) || derive_batch(fixture, expected, 10))
            return 1;
        for (size_t run = 0; run < 5; ++run) {
            if (sample(fixture, expected, run))
                return 1;
        }
    }
    return 0;
}
