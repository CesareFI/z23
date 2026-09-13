/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Only the test target redirects the JNI translation unit's allocator calls.
 * The harness itself uses normal allocation; libFuzzer/VM internals are untouched. */
#undef malloc
#undef free
#include "jni_support.h"
#include "sync_fixture.h"
#include "zcl_sync_watch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI sync check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openSyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jlong JNICALL API(openHistorySyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jint JNICALL API(closeSyncOwner)(JNIEnv *, jclass, jlong);
JNIEXPORT jlong JNICALL API(beginSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(failSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jint);
JNIEXPORT jbyteArray JNICALL API(syncRequest)(JNIEnv *, jclass, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(syncReply)(JNIEnv *, jclass, jlong, jlong, jlong, jbyteArray);
JNIEXPORT jlongArray JNICALL API(syncSnapshot)(JNIEnv *, jclass, jlong, jlong);
JNIEXPORT jlongArray JNICALL API(syncHistorySnapshot)(JNIEnv *, jclass, jlong, jlong);

/* Bounded fake VM local references for sanitizer/failure injection. This tests
 * native cleanup only; real JVM -Xcheck:jni and Android tests remain separate.
 * A failed JNI allocation/region operation leaves a pending fake exception.
 * Release all local references after each test/fuzz invocation, as a VM would.
 */
typedef struct {
    jsize length;
    bool longs;
    union { uint8_t bytes[16385]; jlong numbers[156]; } data;
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
    CHECK(length >= 0 && length <= (longs ? 156 : 16385));
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

static jlong open_owner_mode(bool history)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    const uint8_t source[32] = {1};
    const jbyteArray address = bytes(fixture.candidate.address, 35);
    const jbyteArray source_input = bytes(source, sizeof(source));
    const jlong id = history
        ? API(openHistorySyncOwner)(&environment, NULL, address, (jint)ZCL_MAINNET, source_input)
        : API(openSyncOwner)(&environment, NULL, address, (jint)ZCL_MAINNET, source_input);
    CHECK(id > 0 && !pending_exception);
    return id;
}

static jlong open_owner(void)
{
    return open_owner_mode(false);
}

static const jlong *snapshot(jlong id, jlong now)
{
    fake_array *result = (fake_array *)API(syncSnapshot)(&environment, NULL, id, now);
    CHECK(result != NULL && result->longs && result->length == 10 && !pending_exception);
    CHECK(result->data.numbers[9] >= 0 && result->data.numbers[9] <= (jlong)ZCL_SYNC_FRESH_MS);
    return result->data.numbers;
}

static const jlong *history_snapshot(jlong id, jlong now)
{
    fake_array *result = (fake_array *)API(syncHistorySnapshot)(&environment, NULL, id, now);
    CHECK(result != NULL && result->longs && result->length >= 12 && result->length <= 156 && !pending_exception);
    const jlong *values = result->data.numbers;
    CHECK(values[10] >= 0 && values[10] <= 1 && values[11] >= 0 && values[11] <= 16);
    CHECK(result->length == 12 + values[11] * 9);
    if (values[10] == 0) CHECK(values[11] == 0);
    for (size_t i = 0; i < (size_t)values[11]; ++i) {
        for (size_t word = 0; word < 8; ++word)
            CHECK(values[12 + i * 9 + word] >= 0 && values[12 + i * 9 + word] <= UINT32_MAX);
        CHECK(values[20 + i * 9] >= -1 && values[20 + i * 9] <= INT32_MAX);
    }
    return values;
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
    CHECK(API(syncSnapshot)(NULL, NULL, id, 1) == NULL);
    CHECK(snapshot(id, 0)[9] == 1); /* NULL env did not advance/expire the owner. */
    fail_new = true;
    CHECK(API(syncSnapshot)(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    const jlong *state = snapshot(id, 1);
    CHECK(state[0] == ZCL_OK && state[2] == 0 && state[3] == ZCL_TIMED_OUT && state[9] == 0);
    fail_set = true;
    CHECK(API(syncSnapshot)(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(snapshot(id, 1)[0] == ZCL_CANCELLED);
    release_references();
}

static void history_snapshot_failures(void)
{
    const jlong id = open_owner_mode(true);
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 100, UINT32_MAX - 5) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 1, UINT32_MAX - 6) > 0);
    CHECK(API(syncHistorySnapshot)(NULL, NULL, id, 1) == NULL);
    CHECK(history_snapshot(id, 0)[9] == 1);
    for (unsigned mode = 0; mode < 2; ++mode) {
        if (mode == 0) fail_new = true; else fail_set = true;
        CHECK(API(syncHistorySnapshot)(&environment, NULL, id, 1) == NULL && pending_exception);
        pending_exception = false;
        const jlong *state = history_snapshot(id, 1);
        CHECK(state[0] == ZCL_OK && state[3] == ZCL_TIMED_OUT && state[10] == 0);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(history_snapshot(id, 1)[0] == ZCL_CANCELLED);
    release_references();
}

static size_t maximum_history(char *frame, size_t capacity)
{
    static const char prefix[] = "{\"id\":6,\"result\":[";
    CHECK(capacity > sizeof(prefix));
    size_t used = sizeof(prefix) - 1;
    memcpy(frame, prefix, used);
    for (unsigned i = 0; i < 16; ++i) {
        const int count = snprintf(frame + used, capacity - used,
            "%s{\"tx_hash\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff%08x\",\"height\":%d}",
            i == 0 ? "" : ",", i, i % 2 == 0 ? 0 : -1);
        CHECK(count > 0 && (size_t)count < capacity - used);
        used += (size_t)count;
    }
    CHECK(capacity - used >= 2);
    memcpy(frame + used, "]}", 2);
    return used + 2;
}

static void history_packet_and_owner_replacement(void)
{
    const jlong id = open_owner_mode(true);
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
    CHECK(token > 0);
    release_references();
    for (unsigned step = 1; step <= 7; ++step) {
        CHECK(API(syncRequest)(&environment, NULL, id, token, step) != NULL);
        static char frame[4096];
        const unsigned phase = step == 6 ? ZCL_SYNC_HISTORY : (step == 7 ? ZCL_SYNC_TIP_AFTER : step);
        const size_t length = step == 6 ? maximum_history(frame, sizeof(frame))
            : sync_fixture_reply(ZCL_MAINNET, phase, step, frame, sizeof(frame));
        CHECK(API(syncReply)(&environment, NULL, id, token, step, bytes((const uint8_t *)frame, length)) == ZCL_OK);
        const jlong *state = history_snapshot(id, step);
        CHECK(state[10] == (step == 7 ? 1 : 0));
        release_references();
    }
    const jlong *state = history_snapshot(id, 7);
    CHECK(state[0] == ZCL_OK && state[1] == ZCL_BALANCE_UNVERIFIED && state[7] == 993 && state[11] == 16);
    for (size_t i = 0; i < 16; ++i) {
        for (size_t word = 0; word < 7; ++word) CHECK(state[12 + i * 9 + word] == UINT32_MAX);
        CHECK(state[19 + i * 9] == (jlong)i && state[20 + i * 9] == (i % 2 == 0 ? 0 : -1));
    }
    CHECK(history_snapshot(id, 60007)[1] == ZCL_BALANCE_STALE);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
    const jlong replacement = open_owner();
    CHECK(replacement > id && history_snapshot(replacement, 60007)[10] == 0);
    CHECK(API(failSyncAttempt)(&environment, NULL, id, token, ZCL_IO_FAILURE) == ZCL_CANCELLED);
    CHECK(history_snapshot(id, 60007)[0] == ZCL_CANCELLED);
    CHECK(API(closeSyncOwner)(&environment, NULL, replacement) == ZCL_OK);
    release_references();
}

int main(void)
{
    request_allocation_and_region_failure(); frame_allocation_and_region_failure();
    snapshot_failure_preserves_timeout();
    history_snapshot_failures(); history_packet_and_owner_replacement();
    puts("JNI sync: allocation/region exceptions, full frame clearing, slot cleanup and retained timeout passed");
    return 0;
}
#else
static void advance_fuzz_owner(jlong id, jlong token, bool history, unsigned step)
{
    for (unsigned n = 1; n < step; ++n) {
        CHECK(API(syncRequest)(&environment, NULL, id, token, (jlong)n) != NULL);
        static char frame[4096]; /* Single-threaded public fuzz fixture only. */
        const unsigned phase = history && n == 6 ? ZCL_SYNC_HISTORY : (history && n == 7 ? ZCL_SYNC_TIP_AFTER : n);
        const size_t count = sync_fixture_reply(ZCL_MAINNET, phase, n, frame, sizeof(frame));
        CHECK(API(syncReply)(&environment, NULL, id, token, (jlong)n,
            bytes((const uint8_t *)frame, count)) == ZCL_OK);
    }
}

static void inspect_fuzz_owner(jlong id, bool history, unsigned step)
{
    const jlong *state = snapshot(id, (jlong)step);
    CHECK(state[0] == ZCL_OK && state[1] >= 0 && state[1] <= 2);
    const jlong *complete = history_snapshot(id, (jlong)step);
    CHECK(complete[0] == ZCL_OK && complete[1] == state[1]);
    if (complete[10] != 0) CHECK(history && step == 7 && complete[11] == 2);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
    if (length == 0 || length > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    const bool history = (data[0] & 8) != 0;
    const jlong id = history ? open_owner_mode(true) : open_owner();
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
    CHECK(token > 0);
    const unsigned step = (unsigned)data[0] % (history ? 7u : 6u) + 1;
    advance_fuzz_owner(id, token, history, step);
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
    inspect_fuzz_owner(id, history, step);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(API(failSyncAttempt)(&environment, NULL, id, token, (jint)ZCL_CANCELLED) == ZCL_CANCELLED);
    release_references();
    return 0;
}
#endif
