/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Only the test target redirects the JNI translation unit's allocator calls.
 * The harness itself uses normal allocation; libFuzzer/VM internals are untouched. */
#undef malloc
#undef free
#include "jni_support.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI sync check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openSyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jint JNICALL API(closeSyncOwner)(JNIEnv *, jclass, jlong);
JNIEXPORT jlong JNICALL API(beginSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(failSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jint);
JNIEXPORT jbyteArray JNICALL API(syncRequest)(JNIEnv *, jclass, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(syncReply)(JNIEnv *, jclass, jlong, jlong, jlong, jbyteArray);
JNIEXPORT jlongArray JNICALL API(syncSnapshot)(JNIEnv *, jclass, jlong, jlong);

/* Bounded fake VM local references for sanitizer/failure injection. This tests
 * native cleanup only; real JVM -Xcheck:jni and Android tests remain separate.
 * A failed JNI allocation/region operation leaves a pending fake exception.
 * Release all local references after each test/fuzz invocation, as a VM would.
 */
typedef struct {
    jsize length;
    bool longs;
    union { uint8_t bytes[16385]; jlong numbers[9]; } data;
} fake_array;
static fake_array *references[32];
static size_t reference_count;
static bool pending_exception, fail_new, fail_set, fail_get, fail_frame;
static uint8_t *owned_frame;

void *zcl_jni_test_malloc(size_t size)
{
    if (size != ZCL_ELECTRUM_FRAME_MAX) return malloc(size);
    CHECK(owned_frame == NULL);
    if (fail_frame) { fail_frame = false; return NULL; }
    owned_frame = malloc(size);
    return owned_frame;
}
void zcl_jni_test_free(void *pointer)
{
    if (pointer != NULL && pointer == owned_frame) {
        for (size_t i = 0; i < ZCL_ELECTRUM_FRAME_MAX; ++i) CHECK(owned_frame[i] == 0);
        owned_frame = NULL;
    }
    free(pointer);
}

static fake_array *array_new(jsize length, bool longs)
{
    CHECK(length >= 0 && length <= (longs ? 9 : 16385));
    CHECK(reference_count < sizeof(references) / sizeof(references[0]));
    fake_array *array = calloc(1, sizeof(*array));
    CHECK(array != NULL);
    array->length = length;
    array->longs = longs;
    references[reference_count++] = array;
    return array;
}

static jbyteArray bytes(const uint8_t *data, size_t length)
{
    CHECK(length <= 16385);
    fake_array *array = array_new((jsize)length, false);
    if (length != 0) memcpy(array->data.bytes, data, length);
    return (jbyteArray)array;
}

static void release_references(void)
{
    CHECK(owned_frame == NULL);
    for (size_t i = 0; i < reference_count; ++i) { free(references[i]); references[i] = NULL; }
    reference_count = 0;
    pending_exception = false;
    fail_new = false; fail_set = false; fail_get = false; fail_frame = false;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending_exception ? JNI_TRUE : JNI_FALSE;
}
static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input != NULL && !pending_exception);
    return ((fake_array *)input)->length;
}
static fake_array *region(jarray input, jsize offset, jsize count, bool longs)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->longs == longs && !pending_exception);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length && count <= array->length - offset);
    return array;
}
static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, false);
    if (fail_get) { fail_get = false; pending_exception = true; return; }
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}
static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, false);
    if (fail_set) { fail_set = false; pending_exception = true; return; }
    memcpy(array->data.bytes + (size_t)offset, data, (size_t)count);
}
static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending_exception);
    if (fail_new) { fail_new = false; pending_exception = true; return NULL; }
    return (jbyteArray)array_new(length, false);
}
static jlongArray JNICALL new_longs(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending_exception);
    if (fail_new) { fail_new = false; pending_exception = true; return NULL; }
    return (jlongArray)array_new(length, true);
}
static void JNICALL set_longs(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, true);
    if (fail_set) { fail_set = false; pending_exception = true; return; }
    memcpy(array->data.numbers + (size_t)offset, data, (size_t)count * sizeof(*data));
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .SetByteArrayRegion = set_bytes,
    .NewByteArray = new_bytes, .NewLongArray = new_longs, .SetLongArrayRegion = set_longs
};
static JNIEnv environment = &table;

static jlong open_owner(void)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    const uint8_t source[32] = {1};
    const jlong id = API(openSyncOwner)(&environment, NULL,
        bytes(fixture.candidate.address, 35), (jint)ZCL_MAINNET, bytes(source, sizeof(source)));
    CHECK(id > 0 && !pending_exception);
    return id;
}

static const jlong *snapshot(jlong id, jlong now)
{
    fake_array *result = (fake_array *)API(syncSnapshot)(&environment, NULL, id, now);
    CHECK(result != NULL && result->longs && result->length == 9 && !pending_exception);
    return result->data.numbers;
}

#ifndef ZCL_JNI_FUZZ
static void request_allocation_and_region_failure(void)
{
    const jlong id = open_owner();
    for (unsigned mode = 0; mode < 2; ++mode) {
        const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
        CHECK(token > 0);
        if (mode == 0) fail_new = true; else fail_set = true;
        CHECK(API(syncRequest)(&environment, NULL, id, token, 0) == NULL && pending_exception);
        pending_exception = false; /* Simulated VM catches the allocation/region exception. */
        const jlong *state = snapshot(id, 0);
        CHECK(state[0] == ZCL_OK && state[1] == 0 && state[2] == 0 && state[3] == ZCL_RESOURCE_EXHAUSTED);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
}

static void frame_allocation_and_region_failure(void)
{
    const jlong id = open_owner();
    const jbyteArray frame = bytes((const uint8_t *)"{}", 2);
    for (unsigned mode = 0; mode < 2; ++mode) {
        const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
        CHECK(token > 0 && API(syncRequest)(&environment, NULL, id, token, 0) != NULL);
        if (mode == 0) fail_frame = true; else fail_get = true;
        const jint expected = mode == 0 ? (jint)ZCL_RESOURCE_EXHAUSTED : (jint)ZCL_INVALID_ARGUMENT;
        CHECK(API(syncReply)(&environment, NULL, id, token, 0, frame) == expected);
        CHECK(pending_exception == (mode == 1));
        pending_exception = false;
        CHECK(snapshot(id, 0)[3] == expected && owned_frame == NULL);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
}

static void snapshot_failure_preserves_timeout(void)
{
    const jlong id = open_owner();
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 1, 1) > 0);
    fail_new = true;
    CHECK(API(syncSnapshot)(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    const jlong *state = snapshot(id, 1);
    CHECK(state[0] == ZCL_OK && state[2] == 0 && state[3] == ZCL_TIMED_OUT);
    fail_set = true;
    CHECK(API(syncSnapshot)(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(snapshot(id, 1)[0] == ZCL_CANCELLED);
    release_references();
}

int main(void)
{
    request_allocation_and_region_failure(); frame_allocation_and_region_failure();
    snapshot_failure_preserves_timeout();
    puts("JNI sync: allocation/region exceptions, full frame clearing, slot cleanup and retained timeout passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
    if (length == 0 || length > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    const jlong id = open_owner();
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
    CHECK(token > 0);
    const unsigned step = (unsigned)data[0] % 6 + 1;
    for (unsigned n = 1; n < step; ++n) {
        CHECK(API(syncRequest)(&environment, NULL, id, token, (jlong)n) != NULL);
        static char frame[4096]; /* Single-threaded public fuzz fixture only. */
        const size_t count = sync_fixture_reply(ZCL_MAINNET, n, n, frame, sizeof(frame));
        CHECK(API(syncReply)(&environment, NULL, id, token, (jlong)n,
            bytes((const uint8_t *)frame, count)) == ZCL_OK);
    }
    if ((data[0] & 0x80) != 0) fail_new = true;
    if ((data[0] & 0x40) != 0) fail_set = true;
    jbyteArray request = API(syncRequest)(&environment, NULL, id, token, (jlong)step);
    pending_exception = false; fail_new = false; fail_set = false;
    if (request != NULL) {
        const jbyteArray input = bytes(data + 1, length - 1);
        fail_get = (data[0] & 0x20) != 0;
        fail_frame = (data[0] & 0x10) != 0;
        const jint status = API(syncReply)(&environment, NULL, id, token, (jlong)step, input);
        CHECK(status >= ZCL_OK && status <= ZCL_TLS_FAILURE);
        pending_exception = false; fail_get = false; fail_frame = false;
    }
    const jlong *state = snapshot(id, (jlong)step);
    CHECK(state[0] == ZCL_OK && state[1] >= 0 && state[1] <= 2);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(API(failSyncAttempt)(&environment, NULL, id, token, (jint)ZCL_CANCELLED) == ZCL_CANCELLED);
    release_references();
    return 0;
}
#endif
