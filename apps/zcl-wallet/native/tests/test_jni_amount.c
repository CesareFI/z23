/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI amount check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(parseAmount)(JNIEnv *, jclass, jbyteArray);
JNIEXPORT jbyteArray JNICALL API(formatAmount)(JNIEnv *, jclass, jlong);
JNIEXPORT jbyteArray JNICALL API(formatAmountDelta)(JNIEnv *, jclass, jlong);
JNIEXPORT jlong JNICALL API(changeAmount)(JNIEnv *, jclass, jlong, jlong, jboolean);

typedef struct { jsize length; uint8_t bytes[32]; } fake_array;
/* Bounded single-threaded public fake-VM state, never linked into the app. */
static fake_array input;
static struct { uint64_t before; fake_array value; uint64_t after; } output;
static bool pending, allocation_failure;
static unsigned calls, fault;

static bool vm_failure(void)
{
    CHECK(!pending);
    ++calls;
    if (fault != calls) return false;
    pending = true;
    return true;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray array)
{
    (void)env;
    CHECK(array == (jarray)&input);
    return vm_failure() ? 0 : input.length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&input && offset == 0 && count >= 0 && count <= 17 && bytes != NULL);
    if (vm_failure()) { if (count > 0) bytes[0] = 42; return; }
    memcpy(bytes, input.bytes, (size_t)count);
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize count)
{
    (void)env;
    CHECK(count > 0 && count <= 18);
    if (vm_failure() || allocation_failure) return NULL;
    output.value.length = count;
    return (jbyteArray)&output.value;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&output.value && offset == 0 && count > 0 && count <= 18 && bytes != NULL);
    if (vm_failure()) { output.value.bytes[0] = (uint8_t)bytes[0]; return; }
    memcpy(output.value.bytes, bytes, (size_t)count);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length, .GetByteArrayRegion = get_bytes,
    .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes
};
static JNIEnv environment = &table;

static void reset(unsigned selected_fault)
{
    pending = false; allocation_failure = false;
    calls = 0; fault = selected_fault;
    memset(&input, 0, sizeof(input));
    input.length = 10;
    memcpy(input.bytes, "1.23456789", 10);
    memset(&output, 0xa5, sizeof(output));
}

static jbyteArray format(bool delta, jlong amount, JNIEnv *env)
{
    return delta ? API(formatAmountDelta)(env, NULL, amount) : API(formatAmount)(env, NULL, amount);
}

static void pending_entries(void)
{
    reset(0);
    pending = true;
    CHECK(API(parseAmount)(&environment, NULL, (jbyteArray)&input) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(pending && calls == 0);
    for (unsigned delta = 0; delta < 2; ++delta) {
        CHECK(format(delta != 0, 1, &environment) == NULL);
        CHECK(pending && calls == 0);
    }
    pending = false;
    CHECK(API(parseAmount)(NULL, NULL, (jbyteArray)&input) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(parseAmount)(&environment, NULL, NULL) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(format(false, 1, NULL) == NULL && format(true, -1, NULL) == NULL);
    CHECK(calls == 0);
}

static void parse_faults(void)
{
    for (unsigned selected = 0; selected <= 2; ++selected) {
        reset(selected);
        const fake_array saved = input;
        const jlong value = API(parseAmount)(&environment, NULL, (jbyteArray)&input);
        CHECK(value == (selected == 0 ? INT64_C(123456789) : -(jlong)ZCL_INVALID_ARGUMENT));
        CHECK(pending == (selected != 0));
        CHECK(calls == (selected == 0 ? 2 : selected));
        CHECK(memcmp(&input, &saved, sizeof(input)) == 0);
    }
    const jsize lengths[] = {-1, 0, 18, 32, INT32_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        reset(0);
        input.length = lengths[i];
        CHECK(API(parseAmount)(&environment, NULL, (jbyteArray)&input) == -(jlong)ZCL_INVALID_ENCODING);
        CHECK(!pending);
    }
}

static void parse_statuses(void)
{
    static const struct { const char *text; jlong expected; } cases[] = {
        {"0", 0}, {"0.00000001", 1}, {"21000000", (jlong)ZCL_MAX_MONEY},
        {"21000000.00000001", -(jlong)ZCL_OUT_OF_RANGE},
        {"-1", -(jlong)ZCL_INVALID_ENCODING}, {"1x", -(jlong)ZCL_INVALID_ENCODING}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset(0);
        const size_t length = strlen(cases[i].text);
        CHECK(length <= sizeof(input.bytes));
        memcpy(input.bytes, cases[i].text, length);
        input.length = (jsize)length;
        CHECK(API(parseAmount)(&environment, NULL, (jbyteArray)&input) == cases[i].expected);
        CHECK(!pending);
    }
}

static void exact_format(bool delta, jlong value, const char *expected)
{
    const size_t length = strlen(expected);
    for (unsigned selected = 0; selected <= 3; ++selected) {
        reset(selected == 3 ? 0 : selected);
        allocation_failure = selected == 3;
        const jbyteArray result = format(delta, value, &environment);
        CHECK(output.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && output.after == output.before);
        if (selected != 0) {
            CHECK(result == NULL && pending == (selected != 3));
            continue;
        }
        CHECK(result == (jbyteArray)&output.value && !pending);
        CHECK((size_t)output.value.length == length && memcmp(output.value.bytes, expected, length) == 0);
        for (size_t i = length; i < sizeof(output.value.bytes); ++i) CHECK(output.value.bytes[i] == 0xa5);
    }
}

static void formats(void)
{
    exact_format(false, 0, "0");
    exact_format(false, 1, "0.00000001");
    exact_format(false, (jlong)ZCL_MAX_MONEY, "21000000");
    exact_format(true, 0, "0");
    exact_format(true, -1, "-0.00000001");
    exact_format(true, 1, "+0.00000001");
    exact_format(true, 1 - (jlong)ZCL_MAX_MONEY, "-20999999.99999999");
    const jlong invalid[] = {INT64_MIN, INT64_MAX, 1 + (jlong)ZCL_MAX_MONEY};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        reset(0);
        CHECK(format(false, invalid[i], &environment) == NULL);
        CHECK(format(true, invalid[i], &environment) == NULL);
        CHECK(calls == 0 && !pending);
    }
}

static void arithmetic(void)
{
    CHECK(API(changeAmount)(NULL, NULL, 7, 5, JNI_FALSE) == 12);
    CHECK(API(changeAmount)(NULL, NULL, 7, 5, JNI_TRUE) == 2);
    CHECK(API(changeAmount)(NULL, NULL, 0, 1, JNI_TRUE) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(changeAmount)(NULL, NULL, -1, 0, JNI_FALSE) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(changeAmount)(NULL, NULL, INT64_MAX, INT64_MAX, JNI_FALSE) ==
        -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(changeAmount)(NULL, NULL, 7, 5, (jboolean)2) ==
        -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(changeAmount)(NULL, NULL, 7, 5, (jboolean)UINT8_MAX) ==
        -(jlong)ZCL_INVALID_ARGUMENT);
}

int main(void)
{
    pending_entries();
    parse_faults();
    parse_statuses();
    formats();
    arithmetic();
    puts("JNI amount exact values, pending exceptions and VM fault checks passed");
    return 0;
}
