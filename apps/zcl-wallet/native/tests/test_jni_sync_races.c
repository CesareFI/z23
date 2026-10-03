/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _XOPEN_SOURCE 700
#include "jni_support.h"
#include "zcl_sync_watch.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI sync race check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openSyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jint JNICALL API(closeSyncOwner)(JNIEnv *, jclass, jlong);
JNIEXPORT jlong JNICALL API(beginSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(failSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jint);
JNIEXPORT jbyteArray JNICALL API(syncRequest)(JNIEnv *, jclass, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(syncReply)(JNIEnv *, jclass, jlong, jlong, jlong, jbyteArray);
JNIEXPORT jlongArray JNICALL API(syncSnapshot)(JNIEnv *, jclass, jlong, jlong);

/* Public-only fake VM. Each thread owns one result slot, consumed before its
 * next JNI call. Input arrays are local to open_owner. No borrowed pointer,
 * fault switch or VM result is shared between threads. Real JVM/device tests
 * separately qualify Android/VM behavior. This fixture tests the C registry. */
typedef struct {
    jsize length;
    bool numbers;
    union { uint8_t bytes[ZCL_ELECTRUM_REQUEST_MAX + 1]; jlong values[10]; } data;
} fake_array;
static _Thread_local fake_array result;
static _Thread_local unsigned byte_reads;

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input != NULL);
    return ((const fake_array *)input)->length;
}

static fake_array *region(jarray input, jsize offset, jsize count, bool numbers)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->numbers == numbers);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length);
    CHECK(count <= array->length - offset);
    const size_t maximum = numbers ? 10 : ZCL_ELECTRUM_REQUEST_MAX + 1;
    CHECK((size_t)array->length <= maximum);
    return array;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    const fake_array *array = region(input, offset, count, false);
    CHECK(output != NULL);
    ++byte_reads;
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    fake_array *array = region(input, offset, count, false);
    CHECK(bytes != NULL && array == &result);
    memcpy(array->data.bytes + (size_t)offset, bytes, (size_t)count);
}

static void JNICALL set_numbers(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *numbers)
{
    (void)env;
    fake_array *array = region(input, offset, count, true);
    CHECK(numbers != NULL && array == &result);
    memcpy(array->data.values + (size_t)offset, numbers, (size_t)count * sizeof(*numbers));
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(length >= 0 && (size_t)length <= ZCL_ELECTRUM_REQUEST_MAX + 1);
    result = (fake_array){.length = length};
    return (jbyteArray)&result;
}

static jlongArray JNICALL new_numbers(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(length == 10);
    result = (fake_array){.length = length, .numbers = true};
    return (jlongArray)&result;
}

#if defined(__ANDROID__)
typedef struct JNINativeInterface sync_jni_interface;
#else
typedef struct JNINativeInterface_ sync_jni_interface;
#endif
static const sync_jni_interface table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .SetByteArrayRegion = set_bytes,
    .NewByteArray = new_bytes, .NewLongArray = new_numbers, .SetLongArrayRegion = set_numbers
};
static JNIEnv environment = &table; /* Read-only table pointer shared by all calls. */

static jlong open_owner(void)
{
    const zcl_address address = {.network = ZCL_MAINNET, .kind = ZCL_P2PKH};
    fake_array encoded = {0}, source = {.length = 32};
    size_t length = 0;
    CHECK(zcl_address_encode(&address, encoded.data.bytes, sizeof(encoded.data.bytes), &length) == ZCL_OK);
    CHECK(length == 35);
    encoded.length = (jsize)length;
    const jlong id = API(openSyncOwner)(&environment, NULL, (jbyteArray)&encoded, 0, (jbyteArray)&source);
    CHECK(id > 0);
    return id;
}

static void snapshot(jlong id, bool retired)
{
    const fake_array *packet = (fake_array *)API(syncSnapshot)(&environment, NULL, id, 2);
    CHECK(packet == &result && packet->numbers && packet->length == 10);
    const jlong status = packet->data.values[0];
    if (retired) CHECK(status == ZCL_CANCELLED);
    CHECK(status == ZCL_OK || status == ZCL_CANCELLED);
    if (status == ZCL_CANCELLED) {
        for (size_t i = 1; i < 10; ++i) CHECK(packet->data.values[i] == 0);
    } else {
        CHECK(packet->data.values[1] == ZCL_BALANCE_UNAVAILABLE);
        CHECK(packet->data.values[2] == 1 && packet->data.values[3] == ZCL_OK);
    }
}

static void request(jlong id, jlong token, bool retired)
{
    const fake_array *packet = (fake_array *)API(syncRequest)(&environment, NULL, id, token, 2);
    CHECK(packet == &result && !packet->numbers && packet->length > 0);
    const uint8_t status = packet->data.bytes[0];
    if (retired) CHECK(status == ZCL_CANCELLED);
    CHECK(status == ZCL_OK || status == ZCL_BUSY || status == ZCL_CANCELLED);
    if (status == ZCL_OK) {
        CHECK(packet->length > 1);
        CHECK(packet->data.bytes[(size_t)packet->length - 1] == '\n');
    } else CHECK(packet->length == 1);
}

typedef struct {
    pthread_barrier_t start, retired;
    jlong id, token;
} race;

static void meet(pthread_barrier_t *barrier)
{
    const int status = pthread_barrier_wait(barrier);
    CHECK(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *query_owner(void *argument)
{
    race *state = argument;
    CHECK(state != NULL);
    /* Only barriers mutate; IDs were published before pthread_create. */
    meet(&state->start);
    for (unsigned i = 0; i < 64; ++i) {
        snapshot(state->id, false);
        request(state->id, state->token, false);
    }
    meet(&state->retired);
    for (unsigned i = 0; i < 64; ++i) {
        snapshot(state->id, true);
        request(state->id, state->token, true);
        CHECK(API(failSyncAttempt)(&environment, NULL, state->id, state->token, ZCL_CANCELLED) == ZCL_CANCELLED);
    }
    return NULL;
}

static void replace_while_querying(void)
{
    race state = {0};
    state.id = open_owner();
    state.token = API(beginSyncAttempt)(&environment, NULL, state.id, 1, 30000, 1);
    CHECK(state.token == 1);
    CHECK(pthread_barrier_init(&state.start, NULL, 3) == 0);
    CHECK(pthread_barrier_init(&state.retired, NULL, 3) == 0);
    pthread_t readers[2];
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&readers[i], NULL, query_owner, &state) == 0);
    meet(&state.start);
    CHECK(API(closeSyncOwner)(&environment, NULL, state.id) == ZCL_OK);
    const jlong replacement = open_owner();
    CHECK(replacement > state.id);
    CHECK(API(beginSyncAttempt)(&environment, NULL, replacement, 1, 30000, 1) == state.token);
    meet(&state.retired);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(readers[i], NULL) == 0);
    /* Old callbacks could carry an identical attempt token. They must neither
     * observe nor cancel the replacement owner's active attempt. */
    snapshot(replacement, false);
    request(replacement, state.token, false);
    CHECK(result.data.bytes[0] == ZCL_OK);
    CHECK(API(closeSyncOwner)(&environment, NULL, replacement) == ZCL_OK);
    CHECK(pthread_barrier_destroy(&state.start) == 0);
    CHECK(pthread_barrier_destroy(&state.retired) == 0);
}

static void *retired_replies(void *argument)
{
    race *state = argument;
    CHECK(state != NULL);
    fake_array input = {.length = 1, .data.bytes = {'x'}};
    byte_reads = 0;
    /* Stable public input and per-thread VM observation, never shared. */
    meet(&state->start);
    for (unsigned i = 0; i < 256; ++i)
        CHECK(API(syncReply)(&environment, NULL, state->id, state->token,
            INT64_MAX, (jbyteArray)&input) == ZCL_CANCELLED);
    CHECK(byte_reads == 0 && input.data.bytes[0] == 'x');
    meet(&state->retired);
    for (unsigned i = 0; i < 64; ++i)
        CHECK(API(syncReply)(&environment, NULL, state->id, state->token,
            INT64_MAX, (jbyteArray)&input) == ZCL_CANCELLED);
    CHECK(byte_reads == 0);
    return NULL;
}

static void replacement_snapshot(jlong id)
{
    const fake_array *packet = (fake_array *)API(syncSnapshot)(&environment, NULL, id, 2);
    CHECK(packet == &result && packet->numbers && packet->length == 10);
    CHECK(packet->data.values[0] == ZCL_OK);
    CHECK(packet->data.values[1] == ZCL_BALANCE_UNAVAILABLE);
    /* The preceding cancellation remains visible during a new refresh. */
    CHECK(packet->data.values[2] == 1 && packet->data.values[3] == ZCL_CANCELLED);
}

static void replace_attempts_while_replying(void)
{
    race state = {0};
    state.id = open_owner();
    state.token = API(beginSyncAttempt)(&environment, NULL, state.id, 1, 30000, 1);
    CHECK(state.token == 1);
    CHECK(API(failSyncAttempt)(&environment, NULL, state.id, state.token, ZCL_CANCELLED) == ZCL_CANCELLED);
    const jlong first = API(beginSyncAttempt)(&environment, NULL, state.id, 2, 30000, 1);
    CHECK(first > state.token);
    request(state.id, first, false);
    CHECK(result.data.bytes[0] == ZCL_OK);
    CHECK(pthread_barrier_init(&state.start, NULL, 3) == 0);
    CHECK(pthread_barrier_init(&state.retired, NULL, 3) == 0);
    pthread_t readers[2];
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&readers[i], NULL, retired_replies, &state) == 0);
    meet(&state.start);
    /* Keep one replacement active throughout the first reply phase, making
     * wrong-token admission observable independently of thread scheduling. */
    for (unsigned i = 0; i < 64; ++i) replacement_snapshot(state.id);
    meet(&state.retired);
    CHECK(API(failSyncAttempt)(&environment, NULL, state.id, first, ZCL_CANCELLED) == ZCL_CANCELLED);
    for (unsigned i = 0; i < 64; ++i) {
        const jlong current = API(beginSyncAttempt)(&environment, NULL, state.id, 2, 30000, 1);
        CHECK(current > state.token);
        request(state.id, current, false);
        CHECK(result.data.bytes[0] == ZCL_OK);
        replacement_snapshot(state.id);
        CHECK(API(failSyncAttempt)(&environment, NULL, state.id, current, ZCL_CANCELLED) == ZCL_CANCELLED);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, state.id) == ZCL_OK);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(readers[i], NULL) == 0);
    CHECK(pthread_barrier_destroy(&state.start) == 0);
    CHECK(pthread_barrier_destroy(&state.retired) == 0);
}

int main(void)
{
    for (unsigned i = 0; i < 16; ++i) {
        replace_while_querying();
        replace_attempts_while_replying();
    }
    puts("JNI sync concurrent closure, replacement and retired callbacks passed");
    return 0;
}
