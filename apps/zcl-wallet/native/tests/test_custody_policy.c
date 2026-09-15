/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_custody.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "custody policy check failed at line %d\n", __LINE__); return 1; } } while (0)

typedef zcl_status (*remaining_fn)(uint64_t, uint64_t, uint64_t *);

static int remaining_window(remaining_fn window, uint64_t duration)
{
    const uint64_t starts[] = {0, 1, UINT64_MAX - duration, UINT64_MAX};
    CHECK(window(0, 0, NULL) == ZCL_INVALID_ARGUMENT);
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        const uint64_t start = starts[i];
        uint64_t remaining = UINT64_MAX;
        CHECK(window(start, start, &remaining) == ZCL_OK);
        CHECK(remaining == duration);
        remaining = UINT64_MAX;
        if (start > 0) {
            CHECK(window(start, start - 1, &remaining) == ZCL_IO_UNCERTAIN);
            CHECK(remaining == UINT64_MAX);
        }
        if (start > UINT64_MAX - duration) continue;
        CHECK(window(start, start + duration - 1, &remaining) == ZCL_OK);
        CHECK(remaining == 1);
        remaining = UINT64_MAX;
        CHECK(window(start, start + duration, &remaining) == ZCL_TIMED_OUT);
        CHECK(remaining == UINT64_MAX);
        CHECK(window(start, UINT64_MAX, &remaining) == ZCL_TIMED_OUT);
        CHECK(remaining == UINT64_MAX);
    }
    return 0;
}

int main(void)
{
    CHECK(ZCL_AUTH_WINDOW_MS == UINT64_C(90000));
    CHECK(ZCL_SETUP_WINDOW_MS == UINT64_C(600000));
    CHECK(remaining_window(zcl_authentication_window_remaining, UINT64_C(90000)) == 0);
    CHECK(remaining_window(zcl_setup_window_remaining, UINT64_C(600000)) == 0);
    static const uint64_t starts[] = {0, 1, UINT64_C(123456789), UINT64_MAX - ZCL_AUTH_WINDOW_MS, UINT64_MAX};
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        const uint64_t start = starts[i];
        CHECK(zcl_authentication_window_check(start, start) == ZCL_OK);
        if (start > 0) CHECK(zcl_authentication_window_check(start, start - 1) == ZCL_IO_UNCERTAIN);
        if (start <= UINT64_MAX - ZCL_AUTH_WINDOW_MS) {
            CHECK(zcl_authentication_window_check(start, start + ZCL_AUTH_WINDOW_MS - 1) == ZCL_OK);
            CHECK(zcl_authentication_window_check(start, start + ZCL_AUTH_WINDOW_MS) == ZCL_TIMED_OUT);
        }
    }
    CHECK(zcl_authentication_window_check(0, UINT64_MAX) == ZCL_TIMED_OUT);
    const zcl_wrapping_policy baseline = {256, ZCL_KEY_TEE, 15, 0, 3};
    CHECK(zcl_wrapping_policy_check(NULL) == ZCL_INVALID_ARGUMENT);
    for (uint32_t hardware = 1; hardware <= 2; ++hardware) {
        for (int32_t seconds = -1; seconds <= 0; ++seconds) {
            zcl_wrapping_policy policy = baseline;
            policy.hardware = hardware;
            policy.authentication_seconds = seconds;
            CHECK(zcl_wrapping_policy_check(&policy) == ZCL_OK);
        }
    }
    for (uint32_t bit = 0; bit < 32; ++bit) {
        zcl_wrapping_policy policy = baseline;
        policy.flags ^= UINT32_C(1) << bit;
        CHECK(zcl_wrapping_policy_check(&policy) == ZCL_UNSUPPORTED);
        policy = baseline;
        policy.key_bits ^= UINT32_C(1) << bit;
        CHECK(zcl_wrapping_policy_check(&policy) == ZCL_UNSUPPORTED);
        policy = baseline;
        policy.authentication_methods ^= UINT32_C(1) << bit;
        CHECK(zcl_wrapping_policy_check(&policy) == ZCL_UNSUPPORTED);
    }
    static const uint32_t invalid_hardware[] = {0, 3, UINT32_MAX};
    for (size_t i = 0; i < sizeof(invalid_hardware) / sizeof(invalid_hardware[0]); ++i) {
        zcl_wrapping_policy policy = baseline;
        policy.hardware = invalid_hardware[i];
        CHECK(zcl_wrapping_policy_check(&policy) == ZCL_UNSUPPORTED);
    }
    static const int32_t invalid_times[] = {INT32_MIN, -2, 1, 30, INT32_MAX};
    for (size_t i = 0; i < sizeof(invalid_times) / sizeof(invalid_times[0]); ++i) {
        zcl_wrapping_policy policy = baseline;
        policy.authentication_seconds = invalid_times[i];
        CHECK(zcl_wrapping_policy_check(&policy) == ZCL_UNSUPPORTED);
    }
    puts("custody policy: strict hardware/per-use metadata and monotonic authentication window boundaries passed");
    return 0;
}
