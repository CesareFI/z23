/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

static unsigned mode = 0;
static size_t calls = 0;
static bool scratch_live;
static uint8_t *scratch;
static size_t offset, requested, erasures;
static const uint8_t *script;
static size_t script_length;
ssize_t __real_getrandom(void *, size_t, unsigned int);
void __real_zcl_secure_zero(void *, size_t);

#define VERIFY(condition) do { if (!(condition)) { \
    fprintf(stderr, "random ownership check failed at line %d\n", __LINE__); abort(); } } while (0)

void __wrap_zcl_secure_zero(void *buffer, size_t length)
{
    if (!scratch_live) {
        __real_zcl_secure_zero(buffer, length);
        return;
    }
    VERIFY(buffer == scratch && length == 64);
    __real_zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) VERIFY(bytes[i] == 0);
    /* Retire the borrowed pointer while random.c's scratch is still live. */
    scratch = NULL;
    scratch_live = false;
    ++erasures;
}

static void observe_read(void *output, size_t length, unsigned int flags)
{
    VERIFY(output != NULL && length > 0 && length <= 64 && flags == GRND_NONBLOCK);
    if (calls == 0) {
        VERIFY(!scratch_live);
        scratch = output;
        scratch_live = true;
        offset = 0;
        requested = length;
        erasures = 0;
    }
    VERIFY(scratch_live && offset <= requested);
    VERIFY(output == scratch + offset && length == requested - offset);
    VERIFY(calls < 128);
    ++calls;
}

static ssize_t scripted_result(void *output, size_t length)
{
    VERIFY(script != NULL && script_length > 0 && script_length <= 128);
    const uint8_t action = script[(calls - 1) % script_length];
    const unsigned kind = action & 7u;
    size_t count = kind == 0 ? (size_t)(action >> 3) + 1 : length;
    if (count > length) count = length;
    const uint8_t marker = (uint8_t)(action ^ (uint8_t)calls);
    memset(output, marker, count); /* Even failed reads may dirty their span. */
    switch (kind) {
    case 0: case 1: return (ssize_t)count;
    case 2: case 7: errno = EINTR; return -1;
    case 3: errno = EAGAIN; return -1;
    case 4: return 0;
    case 5: return (ssize_t)(length + 1);
    default: errno = EIO; return -1;
    }
}

static ssize_t read_result(void *output, size_t length, unsigned int flags)
{
    if (mode == 8) return scripted_result(output, length);
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

ssize_t __wrap_getrandom(void *output, size_t length, unsigned int flags)
{
    observe_read(output, length, flags);
    const ssize_t result = read_result(output, length, flags);
    if (result > 0 && (size_t)result <= length) offset += (size_t)result;
    return result;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "random check failed at line %d\n", __LINE__); return 1; } } while (0)

/* Independent response-sequence model. The caller discards expected bytes
 * from failed sequences and checks its entire output span remains unchanged. */
static zcl_status model_script(const uint8_t *data, size_t size, size_t length,
    uint8_t *expected, size_t *read_count)
{
    size_t filled = 0;
    *read_count = 0;
    while (*read_count < 128 && filled < length) {
        const uint8_t action = data[*read_count % size];
        ++*read_count;
        const unsigned kind = action % 8u;
        if (kind == 2 || kind == 7) continue;
        if (kind != 0 && kind != 1) return ZCL_IO_FAILURE;
        const size_t maximum = kind == 1 ? length : (size_t)(action / 8u) + 1;
        for (size_t byte = 0; byte < maximum && filled < length; ++byte)
            expected[filled++] = (uint8_t)(action ^ (uint8_t)*read_count);
    }
    if (filled != length) return ZCL_IO_FAILURE;
    return ZCL_OK;
}

static void random_case(const uint8_t *data, size_t size)
{
    VERIFY(data != NULL && size >= 2 && size <= 129 && !scratch_live);
    const size_t length = data[0] & 0x7fu;
    const bool missing = (data[0] & 0x80u) != 0;
    uint8_t output[66], expected[66];
    memset(output, 0xa5, sizeof(output));
    memset(expected, 0xa5, sizeof(expected));
    size_t expected_calls = 0;
    zcl_status expected_status = ZCL_INVALID_ARGUMENT;
    if (!missing) {
        expected_status = ZCL_OUT_OF_RANGE;
        if (length > 0 && length <= 64)
            expected_status = model_script(data + 1, size - 1, length, expected + 1, &expected_calls);
    }
    if (expected_status != ZCL_OK) memset(expected, 0xa5, sizeof(expected));
    mode = 8; calls = 0; erasures = 0;
    script = data + 1; script_length = size - 1;
    const zcl_status status = zcl_random_bytes(missing ? NULL : output + 1, length);
    VERIFY(!scratch_live); /* Check the live flag before any expired pointer. */
    VERIFY(status == expected_status && calls == expected_calls);
    VERIFY(erasures == (expected_calls == 0 ? 0u : 1u));
    VERIFY(memcmp(output, expected, sizeof(output)) == 0);
    script = NULL; script_length = 0;
}

#ifndef ZCL_RANDOM_FUZZ
static int bounded_success(void)
{
    uint8_t output[66] = {0};
    for (mode = 0; mode <= 2; ++mode) {
        for (size_t length = 1; length <= 64; ++length) {
            calls = 0;
            memset(output, 0xa5, sizeof(output));
            CHECK(zcl_random_bytes(output + 1, length) == ZCL_OK);
            CHECK(!scratch_live && erasures == 1);
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
        CHECK(!scratch_live && erasures == 1);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
        CHECK(calls <= 128);
        if (mode == 7)
            CHECK(calls == 128);
    }
    return 0;
}

static void scripted_boundaries(void)
{
    uint8_t data[129] = {0};
    /* Every size claim and every single response action, including NULL. */
    for (unsigned length = 0; length <= UINT8_MAX; ++length) {
        data[0] = (uint8_t)length;
        for (unsigned action = 0; action <= UINT8_MAX; ++action) {
            data[1] = (uint8_t)action;
            random_case(data, 2);
        }
    }
    data[0] = 64;
    for (unsigned first = 0; first < 8; ++first) {
        for (unsigned second = 0; second < 8; ++second) {
            data[1] = (uint8_t)first; data[2] = (uint8_t)second;
            random_case(data, 3);
        }
    }
    /* Exact attempt boundary: success at 128 versus unfinished at 128. */
    memset(data + 1, 2, sizeof(data) - 1);
    random_case(data, sizeof(data));
    data[128] = 1;
    random_case(data, sizeof(data));
    data[128] = 0;
    random_case(data, sizeof(data));
}

int main(void)
{
    if (bounded_success() || failures())
        return 1;
    scripted_boundaries();
    puts("random: OS read, partial reads, interruptions and failure bounds passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size >= 2 && size <= 129) random_case(data, size);
    return 0;
}
#endif
