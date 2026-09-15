/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

static unsigned mode = 0;
static size_t calls = 0;
static size_t wiped = 0, filled = 0, requested = 0, interruptions = 0;
static uint8_t *live_scratch = NULL;
static int observing = 0;
ssize_t __real_getrandom(void *, size_t, unsigned int);
void __real_zcl_secure_zero(void *, size_t);

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "random lifetime check failed at line %d\n", __LINE__); abort(); } } while (0)

/* Inspect only during the real scratch lifetime. If wiping is omitted, the
 * caller retires this pointer without evaluating or dereferencing it. */
void __wrap_zcl_secure_zero(void *buffer, size_t length)
{
    if (!observing) {
        __real_zcl_secure_zero(buffer, length);
        return;
    }
    REQUIRE(live_scratch != NULL && buffer == live_scratch && length == 64 && wiped == 0);
    __real_zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i)
        REQUIRE(bytes[i] == 0);
    live_scratch = NULL;
    ++wiped;
}

static int interrupted(void)
{
    return mode == 7 || (mode == 2 && calls <= 7) ||
        (mode == 8 && calls <= interruptions) || (mode == 9 && calls > 1);
}

static ssize_t provider_result(void *output, size_t length, unsigned int flags)
{
    if (mode == 0)
        return __real_getrandom(output, length, flags);
    if (flags != GRND_NONBLOCK)
        return 0;
    if (mode == 3 || (mode == 4 && calls > 1)) {
        errno = EAGAIN;
        return -1;
    }
    if (mode == 5)
        return 0;
    if (mode == 6)
        return (ssize_t)(length + 1);
    if (interrupted()) {
        errno = EINTR;
        return -1;
    }
    size_t count = length < 3 ? length : 3;
    memset(output, 0x42, count);
    return (ssize_t)count;
}

ssize_t __wrap_getrandom(void *output, size_t length, unsigned int flags)
{
    REQUIRE(observing && wiped == 0 && calls < 128);
    if (calls == 0)
        live_scratch = output;
    REQUIRE(live_scratch != NULL && requested <= 64 && filled < requested);
    REQUIRE(output == live_scratch + filled && length == requested - filled);
    REQUIRE(flags == GRND_NONBLOCK);
    ++calls;
    ssize_t count = provider_result(output, length, flags);
    if (count > 0 && (size_t)count <= length)
        filled += (size_t)count;
    return count;
}

static zcl_status observed_random(uint8_t *output, size_t length)
{
    REQUIRE(!observing);
    calls = 0;
    wiped = 0;
    filled = 0;
    requested = length;
    live_scratch = NULL;
    observing = 1;
    zcl_status status = zcl_random_bytes(output, length);
    live_scratch = NULL; /* Assignment only: an omitted wipe must not cause a stale read. */
    observing = 0;
    return status;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "random check failed at line %d\n", __LINE__); return 1; } } while (0)

static int bounded_success(void)
{
    uint8_t output[66] = {0};
    for (mode = 0; mode <= 2; ++mode) {
        for (size_t length = 1; length <= 64; ++length) {
            memset(output, 0xa5, sizeof(output));
            CHECK(observed_random(output + 1, length) == ZCL_OK);
            CHECK(wiped == 1 && filled == length);
            CHECK(output[0] == 0xa5 && output[length + 1] == 0xa5);
            if (mode != 0) {
                for (size_t i = 1; i <= length; ++i)
                    CHECK(output[i] == 0x42);
            }
            zcl_secure_zero(output, sizeof(output));
        }
    }
    return 0;
}

static int failures(void)
{
    uint8_t output[66] = {0}, before[66] = {0};
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(output));
    CHECK(observed_random(NULL, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(calls == 0 && wiped == 0);
    static const size_t invalid_lengths[] = {0, 65, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); ++i) {
        CHECK(observed_random(output, invalid_lengths[i]) == ZCL_OUT_OF_RANGE);
        CHECK(calls == 0 && wiped == 0);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    for (mode = 3; mode <= 7; ++mode) {
        CHECK(observed_random(output + 1, 64) == ZCL_IO_FAILURE);
        CHECK(wiped == 1);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
        CHECK(calls <= 128);
        if (mode == 7)
            CHECK(calls == 128);
    }
    return 0;
}

static int attempt_boundaries(void)
{
    uint8_t output[66], before[66];
    memset(before, 0xa5, sizeof(before));
    for (size_t length = 1; length <= 64; ++length) {
        const size_t reads = (length + 2) / 3;
        for (size_t extra = 0; extra <= 1; ++extra) {
            mode = 8;
            interruptions = 128 - reads + extra;
            memcpy(output, before, sizeof(output));
            zcl_status status = observed_random(output + 1, length);
            CHECK(calls == 128 && wiped == 1);
            CHECK(status == (extra == 0 ? ZCL_OK : ZCL_IO_FAILURE));
            if (extra != 0) {
                CHECK(memcmp(output, before, sizeof(output)) == 0);
                continue;
            }
            CHECK(filled == length);
            for (size_t i = 0; i < sizeof(output); ++i)
                CHECK(output[i] == (i > 0 && i <= length ? 0x42 : 0xa5));
        }
    }
    mode = 9; /* A partial secret followed by 127 interruptions must still be erased. */
    memcpy(output, before, sizeof(output));
    CHECK(observed_random(output + 1, 64) == ZCL_IO_FAILURE);
    CHECK(calls == 128 && filled == 3 && wiped == 1);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    return 0;
}

int main(void)
{
    if (bounded_success() || failures() || attempt_boundaries())
        return 1;
    puts("random: bounded reads, exact retry limits and live scratch erasure passed");
    return 0;
}
