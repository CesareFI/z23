/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _XOPEN_SOURCE 700
#include "jni_review_internal.h"
#include "assessment_fixture.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI review race check at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openReview)(JNIEnv *, jclass, jbyteArray, jobjectArray, jint, jlong, jlong);
JNIEXPORT jint JNICALL API(cancelReview)(JNIEnv *, jclass, jlong);
JNIEXPORT jlongArray JNICALL API(reviewSnapshot)(JNIEnv *, jclass, jlong, jlong);
JNIEXPORT jbyteArray JNICALL API(reviewWire)(JNIEnv *, jclass, jlong, jlong);

/* Public synthetic inputs are immutable after setup. Each thread owns its VM
 * result and consumes it before its next call; no result or borrowed reference
 * crosses a thread boundary. This tests native locking, not a real VM. */
typedef enum { BYTES, NUMBERS, OBJECTS } array_kind;
typedef struct fake_array {
    jsize length;
    array_kind kind;
    union {
        uint8_t bytes[ZCL_TX_WIRE_MAX + 1];
        jlong numbers[ZCL_REVIEW_PACKET_MAX];
        struct fake_array *objects[2];
    } data;
} fake_array;
static assessment_fixture fixture;
static fake_array draft, sources, previous[2];
static _Thread_local fake_array result;
static _Thread_local unsigned borrowed;

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

static fake_array *region(jarray input, jsize offset, jsize count, array_kind kind)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == kind);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length);
    CHECK(count <= array->length - offset);
    const size_t maximum = kind == NUMBERS ? ZCL_REVIEW_PACKET_MAX : ZCL_TX_WIRE_MAX + 1;
    CHECK((size_t)array->length <= maximum);
    return array;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    const fake_array *array = region(input, offset, count, BYTES);
    CHECK(output != NULL);
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    CHECK(bytes != NULL && array == &result);
    memcpy(array->data.bytes + (size_t)offset, bytes, (size_t)count);
}

static void JNICALL set_numbers(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *numbers)
{
    (void)env;
    fake_array *array = region(input, offset, count, NUMBERS);
    CHECK(numbers != NULL && array == &result);
    memcpy(array->data.numbers + (size_t)offset, numbers, (size_t)count * sizeof(*numbers));
}

static jobject JNICALL get_object(JNIEnv *env, jobjectArray input, jsize index)
{
    (void)env;
    CHECK((fake_array *)input == &sources && index >= 0 && index < 2 && borrowed == 0);
    ++borrowed;
    return (jobject)sources.data.objects[(size_t)index];
}

static void JNICALL delete_reference(JNIEnv *env, jobject object)
{
    (void)env;
    CHECK((fake_array *)object == &previous[0] || (fake_array *)object == &previous[1]);
    CHECK(borrowed == 1);
    --borrowed;
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(length > 0 && (size_t)length <= ZCL_TX_WIRE_MAX + 1);
    result = (fake_array){.length = length, .kind = BYTES};
    return (jbyteArray)&result;
}

static jlongArray JNICALL new_numbers(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(length > 0 && (size_t)length <= ZCL_REVIEW_PACKET_MAX);
    result = (fake_array){.length = length, .kind = NUMBERS};
    return (jlongArray)&result;
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .SetByteArrayRegion = set_bytes,
    .GetObjectArrayElement = get_object, .DeleteLocalRef = delete_reference,
    .NewByteArray = new_bytes, .NewLongArray = new_numbers, .SetLongArrayRegion = set_numbers
};
static JNIEnv environment = &table;

static void setup(void)
{
    CHECK(assessment_fixture_init(&fixture));
    size_t length = 0;
    CHECK(zcl_transaction_serialize(&fixture.spending, draft.data.bytes, ZCL_TX_WIRE_MAX, &length) == ZCL_OK);
    CHECK(length <= ZCL_TX_WIRE_MAX);
    draft.length = (jsize)length;
    sources.length = 2; sources.kind = OBJECTS;
    for (size_t i = 0; i < 2; ++i) {
        CHECK(fixture.sources[i].length <= ZCL_TX_WIRE_MAX);
        previous[i].length = (jsize)fixture.sources[i].length;
        memcpy(previous[i].data.bytes, fixture.sources[i].wire, fixture.sources[i].length);
        sources.data.objects[i] = &previous[i];
    }
}

static jlong open_review(void)
{
    const jlong id = API(openReview)(&environment, NULL, (jbyteArray)&draft, (jobjectArray)&sources, 0, 500, 100);
    CHECK(id > 0 && borrowed == 0);
    return id;
}

static void snapshot(jlong id, jlong now, bool retired)
{
    const fake_array *packet = (fake_array *)API(reviewSnapshot)(&environment, NULL, id, now);
    CHECK(packet == &result && packet->kind == NUMBERS && packet->length > 0);
    const jlong status = packet->data.numbers[0];
    if (retired) CHECK(status == ZCL_CANCELLED);
    CHECK(status == ZCL_OK || status == ZCL_CANCELLED);
    if (status == ZCL_CANCELLED) { CHECK(packet->length == 1); return; }
    CHECK(packet->length == 68 && packet->data.numbers[1] == 90000);
    CHECK(packet->data.numbers[5] == draft.length);
    CHECK(packet->data.numbers[6] == 2 && packet->data.numbers[7] == 2);
    CHECK(packet->data.numbers[8] == 11000 && packet->data.numbers[9] == 10500);
    CHECK(packet->data.numbers[10] == 500 && packet->data.numbers[11] == 500);
}

static void wire(jlong id, jlong now, bool retired)
{
    const fake_array *packet = (fake_array *)API(reviewWire)(&environment, NULL, id, now);
    CHECK(packet == &result && packet->kind == BYTES && packet->length > 0);
    const uint8_t status = packet->data.bytes[0];
    if (retired) CHECK(status == ZCL_CANCELLED);
    CHECK(status == ZCL_OK || status == ZCL_CANCELLED);
    if (status == ZCL_CANCELLED) { CHECK(packet->length == 1); return; }
    CHECK(packet->length == draft.length + 1);
    CHECK(memcmp(packet->data.bytes + 1, draft.data.bytes, (size_t)draft.length) == 0);
}

typedef struct { pthread_barrier_t start, retired; jlong id; } race;

static void meet(pthread_barrier_t *barrier)
{
    const int status = pthread_barrier_wait(barrier);
    CHECK(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *read_review(void *argument)
{
    race *state = argument;
    CHECK(state != NULL);
    meet(&state->start);
    for (unsigned i = 0; i < 64; ++i) {
        snapshot(state->id, 100, false);
        wire(state->id, 100, false);
    }
    meet(&state->retired);
    for (unsigned i = 0; i < 64; ++i) {
        snapshot(state->id, INT64_MAX, true);
        wire(state->id, 0, true);
        CHECK(API(cancelReview)(&environment, NULL, state->id) == ZCL_CANCELLED);
    }
    CHECK(borrowed == 0);
    return NULL;
}

static void replace_while_reading(void)
{
    race state = {.id = open_review()};
    snapshot(state.id, 100, false);
    CHECK(result.data.numbers[0] == ZCL_OK);
    wire(state.id, 100, false);
    CHECK(result.data.bytes[0] == ZCL_OK);
    CHECK(pthread_barrier_init(&state.start, NULL, 3) == 0);
    CHECK(pthread_barrier_init(&state.retired, NULL, 3) == 0);
    pthread_t readers[2];
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_create(&readers[i], NULL, read_review, &state) == 0);
    meet(&state.start);
    CHECK(API(cancelReview)(&environment, NULL, state.id) == ZCL_OK);
    const jlong replacement = open_review();
    CHECK(replacement > state.id);
    meet(&state.retired);
    for (size_t i = 0; i < 2; ++i) CHECK(pthread_join(readers[i], NULL) == 0);
    /* Stale cancellation and hostile times must not affect the replacement. */
    snapshot(replacement, 100, false);
    CHECK(result.data.numbers[0] == ZCL_OK);
    wire(replacement, 100, false);
    CHECK(result.data.bytes[0] == ZCL_OK);
    CHECK(API(cancelReview)(&environment, NULL, replacement) == ZCL_OK);
    CHECK(pthread_barrier_destroy(&state.start) == 0);
    CHECK(pthread_barrier_destroy(&state.retired) == 0);
}

int main(void)
{
    setup();
    for (unsigned i = 0; i < 16; ++i) replace_while_reading();
    CHECK(puts("JNI review concurrent reads, cancellation and replacement checked") >= 0);
    return 0;
}
