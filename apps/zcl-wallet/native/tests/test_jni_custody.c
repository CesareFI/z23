/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <jni.h>
#include <stdint.h>
#include <stdio.h>

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_custodyWindowRemainingMillis(JNIEnv *, jclass,
    jboolean, jlong, jlong);

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "JNI custody clock check failed at line %d\n", __LINE__); return 1; } } while (0)

/* This scalar JNI entry calls no VM operation and owns no reference. Public
 * timestamps exercise the actual adapter, including malformed ABI booleans. */
static jlong remaining(jboolean setup, jlong start, jlong now)
{
    return Java_org_zclassic_wallet_core_NativeCore_custodyWindowRemainingMillis(NULL, NULL,
        setup, start, now);
}

static int window_boundaries(jboolean setup, jlong duration)
{
    static const jlong starts[] = {0, 1, INT32_MAX, UINT32_MAX, INT64_MAX - 600000, INT64_MAX};
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        const jlong start = starts[i];
        CHECK(remaining(setup, start, start) == duration);
        if (start > 0) CHECK(remaining(setup, start, start - 1) == 0);
        if (start < INT64_MAX) CHECK(remaining(setup, start, start + 1) == duration - 1);
        if (start > INT64_MAX - duration) continue;
        CHECK(remaining(setup, start, start + duration - 1) == 1);
        CHECK(remaining(setup, start, start + duration) == 0);
    }
    CHECK(remaining(setup, 0, INT64_MAX) == 0);
    return 0;
}

static int invalid_arguments(void)
{
    static const jlong invalid[][2] = {{-1, 0}, {0, -1}, {-1, -1},
        {INT64_MIN, INT64_MIN}, {INT64_MIN, INT64_MAX}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(remaining(JNI_FALSE, invalid[i][0], invalid[i][1]) == 0);
        CHECK(remaining(JNI_TRUE, invalid[i][0], invalid[i][1]) == 0);
    }
    for (unsigned value = 2; value <= UINT8_MAX; ++value)
        CHECK(remaining((jboolean)value, 0, 0) == 0);
    return 0;
}

int main(void)
{
    CHECK(window_boundaries(JNI_FALSE, 90000) == 0);
    CHECK(window_boundaries(JNI_TRUE, 600000) == 0);
    CHECK(invalid_arguments() == 0);
    puts("JNI custody clock: both deadlines, signed widths and all invalid boolean bytes refused");
    return 0;
}
