/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>

static unsigned mode = 0;
static size_t calls = 0;
ssize_t __real_getrandom(void *, size_t, unsigned int);

ssize_t __wrap_getrandom(void *output, size_t length, unsigned int flags)
{
    ++calls;
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
    if (mode == 7 || (mode == 2 && calls <= 7)) {
        errno = EINTR;
        return -1;
    }
    size_t count = length < 3 ? length : 3;
    memset(output, 0x42, count);
    return (ssize_t)count;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "random check failed at line %d\n", __LINE__); return 1; } } while (0)

static int bounded_success(void)
{
    uint8_t output[66] = {0};
    for (mode = 0; mode <= 2; ++mode) {
        for (size_t length = 1; length <= 64; ++length) {
            calls = 0;
            memset(output, 0xa5, sizeof(output));
            CHECK(zcl_random_bytes(output + 1, length) == ZCL_OK);
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
    CHECK(zcl_random_bytes(NULL, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_random_bytes(output, 0) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_random_bytes(output, 65) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_random_bytes(output, SIZE_MAX) == ZCL_OUT_OF_RANGE);
    for (mode = 3; mode <= 7; ++mode) {
        calls = 0;
        CHECK(zcl_random_bytes(output + 1, 64) == ZCL_IO_FAILURE);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
        CHECK(calls <= 128);
        if (mode == 7)
            CHECK(calls == 128);
    }
    return 0;
}

int main(void)
{
    if (bounded_success() || failures())
        return 1;
    puts("random: OS read, partial reads, interruptions and failure bounds passed");
    return 0;
}
