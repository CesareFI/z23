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

static void version_reply(fake_array *frame)
{
    static const char text[] = "{\"id\":1,\"result\":[\"public fixture\",\"1.2\"]}";
    _Static_assert(sizeof(text) <= ZCL_ELECTRUM_REQUEST_MAX + 1, "Public fixture fits fake VM array");
    *frame = (fake_array){.length = (jsize)(sizeof(text) - 1)};
    memcpy(frame->data.bytes, text, sizeof(text) - 1);
}

typedef struct { race *state; jint first_status; } reply_call;

static void *reply_owner(void *argument)
{
    reply_call *call = argument;
    CHECK(call != NULL && call->state != NULL);
    race *state = call->state;
    fake_array frame;
    version_reply(&frame);
    meet(&state->start);
    call->first_status = API(syncReply)(&environment, NULL, state->id, state->token, 2, (jbyteArray)&frame);
    meet(&state->retired);
    /* Both duplicate replies have finished, or the owner has been replaced.
     * Neither retired path may advance a clock or consume the next request. */
    for (unsigned i = 0; i < 64; ++i)
        CHECK(API(syncReply)(&environment, NULL, state->id, state->token, INT64_MAX,
            (jbyteArray)&frame) == ZCL_CANCELLED);
    return NULL;
}

static void reply_outcomes(const reply_call calls[2], bool replaced)
{
    unsigned accepted = 0;
    for (size_t i = 0; i < 2; ++i) {
        const jint status = calls[i].first_status;
        CHECK(status == ZCL_OK || status == ZCL_INVALID_ENCODING || status == ZCL_CANCELLED);
        if (status == ZCL_OK) ++accepted;
        if (!replaced) CHECK(status != ZCL_CANCELLED);
    }
    CHECK(accepted <= 1);
    if (!replaced) CHECK(accepted == 1);
}

static void duplicate_reply_failure(jlong id)
{
    const fake_array *packet = (fake_array *)API(syncSnapshot)(&environment, NULL, id, 2);
    CHECK(packet == &result && packet->numbers && packet->length == 10);
    CHECK(packet->data.values[0] == ZCL_OK && packet->data.values[1] == ZCL_BALANCE_UNAVAILABLE);
    CHECK(packet->data.values[2] == 0 && packet->data.values[3] == ZCL_INVALID_ENCODING);
    CHECK(packet->data.values[9] == 0);
}

static void concurrent_replies(bool replace)
{
    race state = {0};
    state.id = open_owner();
    state.token = API(beginSyncAttempt)(&environment, NULL, state.id, 1, 30000, 1);
    CHECK(state.token == 1);
    request(state.id, state.token, false);
    CHECK(result.data.bytes[0] == ZCL_OK);
    CHECK(pthread_barrier_init(&state.start, NULL, 3) == 0);
    CHECK(pthread_barrier_init(&state.retired, NULL, 3) == 0);
    reply_call calls[2] = {{.state = &state}, {.state = &state}};
    pthread_t writers[2];
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&writers[i], NULL, reply_owner, &calls[i]) == 0);
    meet(&state.start);
    jlong current = state.id;
    if (replace) {
        CHECK(API(closeSyncOwner)(&environment, NULL, state.id) == ZCL_OK);
        current = open_owner();
        CHECK(current > state.id);
        CHECK(API(beginSyncAttempt)(&environment, NULL, current, 1, 30000, 1) == state.token);
        request(current, state.token, false);
        CHECK(result.data.bytes[0] == ZCL_OK);
    }
    meet(&state.retired);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(writers[i], NULL) == 0);
    reply_outcomes(calls, replace);
    if (replace) {
        snapshot(current, false);
        fake_array frame;
        version_reply(&frame);
        CHECK(API(syncReply)(&environment, NULL, current, state.token, 2, (jbyteArray)&frame) == ZCL_OK);
        request(current, state.token, false);
        CHECK(result.data.bytes[0] == ZCL_OK);
    } else duplicate_reply_failure(current);
    CHECK(API(closeSyncOwner)(&environment, NULL, current) == ZCL_OK);
    CHECK(pthread_barrier_destroy(&state.retired) == 0);
    CHECK(pthread_barrier_destroy(&state.start) == 0);
}

int main(void)
{
    for (unsigned i = 0; i < 16; ++i) {
        replace_while_querying();
        concurrent_replies(false);
        concurrent_replies(true);
    }
    puts("JNI sync concurrent requests, snapshots, replies, closure and replacement passed");
    return 0;
}
