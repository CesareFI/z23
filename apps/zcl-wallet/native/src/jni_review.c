/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_review_internal.h"
#include "zcl_keys.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* One process-wide unsigned draft, serialized under this mutex. No native
 * pointer, key, socket or Java reference escapes. IDs survive every clear and
 * cannot repeat within this process. Managed owners cancel only their own ID. */
static zcl_review_owner review = {0};
static pthread_mutex_t review_lock = PTHREAD_MUTEX_INITIALIZER;
_Static_assert(ZCL_REVIEW_ID_MAX <= INT64_MAX, "Review IDs fit jlong");
_Static_assert(ZCL_TLS_FAILURE <= UINT8_MAX, "Review wire statuses fit a byte");

static zcl_status unlock_review(zcl_status status)
{
    return pthread_mutex_unlock(&review_lock) == 0 ? status : ZCL_IO_UNCERTAIN;
}

static zcl_status cancel_review(jlong id)
{
    if (id <= 0) return ZCL_CANCELLED;
    if (pthread_mutex_lock(&review_lock) != 0) return ZCL_IO_FAILURE;
    return unlock_review(zcl_review_cancel(&review, (uint64_t)id));
}

static zcl_status copy_previous(JNIEnv *env, jobjectArray previous, zcl_jni_review_inputs *inputs)
{
    const jsize count = (*env)->GetArrayLength(env, previous);
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (count <= 0 || (size_t)count > ZCL_TX_INPUT_MAX) return ZCL_OUT_OF_RANGE;
    inputs->count = (size_t)count;
    for (jsize i = 0; i < count; ++i) {
        jobject element = (*env)->GetObjectArrayElement(env, previous, i);
        zcl_status status = ZCL_INVALID_ARGUMENT;
        size_t length = 0;
        if (element != NULL && !(*env)->ExceptionCheck(env))
            status = zcl_jni_read_bytes(env, (jbyteArray)element, inputs->previous[(size_t)i],
                ZCL_TX_WIRE_MAX, &length);
        if (element != NULL) (*env)->DeleteLocalRef(env, element);
        if (status != ZCL_OK) return status;
        inputs->sources[(size_t)i].wire = inputs->previous[(size_t)i];
        inputs->sources[(size_t)i].length = length;
    }
    return ZCL_OK;
}

static zcl_status open_copied(JNIEnv *env, jbyteArray draft, jobjectArray previous,
    zcl_network network, uint64_t fee, uint64_t now, uint64_t *id)
{
    zcl_jni_review_inputs *inputs = malloc(sizeof(*inputs));
    if (inputs == NULL) return ZCL_RESOURCE_EXHAUSTED;
    memset(inputs, 0, sizeof(*inputs));
    zcl_status status = zcl_jni_read_bytes(env, draft, inputs->draft,
        sizeof(inputs->draft), &inputs->draft_length);
    if (status == ZCL_OK) status = copy_previous(env, previous, inputs);
    if (status == ZCL_OK)
        status = zcl_review_open(&review, inputs->draft, inputs->draft_length, network,
            inputs->sources, inputs->count, fee, now, id);
    zcl_secure_zero(inputs, sizeof(*inputs));
    free(inputs);
    return status;
}

static zcl_status opening_arguments(JNIEnv *env, jbyteArray draft, jobjectArray previous,
    jint chain, jlong fee, jlong now, zcl_network *network)
{
    if (env == NULL || draft == NULL || previous == NULL) return ZCL_INVALID_ARGUMENT;
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (fee < 0 || (uint64_t)fee > ZCL_MAX_MONEY ||
        now < 0 || now > INT64_MAX - (jlong)ZCL_REVIEW_LIFETIME_MS)
        return ZCL_OUT_OF_RANGE;
    return zcl_jni_network(chain, network);
}

static jlong open_review(JNIEnv *env, jbyteArray draft, jobjectArray previous,
    jint chain, jlong fee, jlong now, bool full_sources)
{
    zcl_network network;
    zcl_status status = opening_arguments(env, draft, previous, chain, fee, now, &network);
    if (status != ZCL_OK) return -(jlong)status;
    if (pthread_mutex_lock(&review_lock) != 0) return -(jlong)ZCL_IO_FAILURE;
    uint64_t id = 0;
    if (review.data.id != 0) status = ZCL_BUSY;
    else if (full_sources)
        status = zcl_jni_open_full_review(env, &review, draft, previous, network, (uint64_t)fee, (uint64_t)now, &id);
    else status = open_copied(env, draft, previous, network, (uint64_t)fee, (uint64_t)now, &id);
    status = unlock_review(status);
    return status == ZCL_OK ? (jlong)id : -(jlong)status;
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_openReview(JNIEnv *env, jclass type,
    jbyteArray draft, jobjectArray previous, jint chain, jlong fee, jlong now)
{
    (void)type;
    return open_review(env, draft, previous, chain, fee, now, false);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_openFullSourceReview(JNIEnv *env, jclass type,
    jbyteArray draft, jobjectArray previous, jint chain, jlong fee, jlong now)
{
    (void)type;
    return open_review(env, draft, previous, chain, fee, now, true);
}

static zcl_status preparation_arguments(JNIEnv *env, jobjectArray previous, jobjectArray destinations,
    jlongArray parameters, jint chain, jlong now, zcl_network *network)
{
    if (env == NULL || previous == NULL || destinations == NULL || parameters == NULL)
        return ZCL_INVALID_ARGUMENT;
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (now < 0 || now > INT64_MAX - (jlong)ZCL_REVIEW_LIFETIME_MS) return ZCL_OUT_OF_RANGE;
    return zcl_jni_network(chain, network);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_prepareFullSourceReview(JNIEnv *env, jclass type,
    jobjectArray previous, jobjectArray destinations, jlongArray parameters, jint chain, jlong now)
{
    (void)type;
    zcl_network network;
    zcl_status status = preparation_arguments(env, previous, destinations, parameters, chain, now, &network);
    if (status != ZCL_OK) return -(jlong)status;
    if (pthread_mutex_lock(&review_lock) != 0) return -(jlong)ZCL_IO_FAILURE;
    uint64_t id = 0;
    status = review.data.id != 0 ? ZCL_BUSY
        : zcl_jni_prepare_full_review(env, &review, previous, destinations, parameters, network, (uint64_t)now, &id);
    status = unlock_review(status);
    return status == ZCL_OK ? (jlong)id : -(jlong)status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_cancelReview(JNIEnv *env, jclass type, jlong id)
{
    (void)env; (void)type;
    return (jint)cancel_review(id);
}

static zcl_status read_numbers(jlong id, jlong now)
{
    if (id <= 0) return ZCL_CANCELLED;
    return now < 0 ? ZCL_OUT_OF_RANGE : ZCL_OK;
}

static zcl_status snapshot_numbers(jlong id, jlong now, jlong *values, size_t *length)
{
    zcl_status status = read_numbers(id, now);
    if (status != ZCL_OK) return status;
    if (pthread_mutex_lock(&review_lock) != 0) return ZCL_IO_FAILURE;
    zcl_review_snapshot snapshot = {0};
    status = zcl_review_snapshot_get(&review, (uint64_t)id, (uint64_t)now, &snapshot);
    if (status == ZCL_OK) status = zcl_jni_review_values(&snapshot, values, length);
    zcl_secure_zero(&snapshot, sizeof(snapshot));
    return unlock_review(status);
}

static jlongArray new_numbers(JNIEnv *env, const jlong *values, size_t length)
{
    if (length > ZCL_REVIEW_PACKET_MAX) return NULL;
    jlongArray result = (*env)->NewLongArray(env, (jsize)length);
    if (result == NULL || (*env)->ExceptionCheck(env)) return NULL;
    (*env)->SetLongArrayRegion(env, result, 0, (jsize)length, values);
    return (*env)->ExceptionCheck(env) ? NULL : result;
}

JNIEXPORT jlongArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_reviewSnapshot(JNIEnv *env, jclass type, jlong id, jlong now)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
    jlong values[ZCL_REVIEW_PACKET_MAX] = {0};
    size_t length = 1;
    const zcl_status status = snapshot_numbers(id, now, values, &length);
    if (status != ZCL_OK) {
        memset(values, 0, sizeof(values));
        values[0] = (jlong)status;
        length = 1;
    }
    jlongArray result = new_numbers(env, values, length);
    zcl_secure_zero(values, sizeof(values));
    if (result == NULL) (void)cancel_review(id);
    return result;
}

static zcl_status copy_wire(jlong id, jlong now, uint8_t *wire, size_t capacity, size_t *length)
{
    zcl_status status = read_numbers(id, now);
    if (status != ZCL_OK) return status;
    if (pthread_mutex_lock(&review_lock) != 0) return ZCL_IO_FAILURE;
    status = zcl_review_copy_wire(&review, (uint64_t)id, (uint64_t)now, wire, capacity, length);
    return unlock_review(status);
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_reviewWire(JNIEnv *env, jclass type, jlong id, jlong now)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
    uint8_t bytes[ZCL_TX_WIRE_MAX + 1] = {0};
    size_t length = 0;
    const zcl_status status = copy_wire(id, now, bytes + 1, sizeof(bytes) - 1, &length);
    bytes[0] = (uint8_t)status;
    jbyteArray result = zcl_jni_new_bytes(env, bytes, status == ZCL_OK ? length + 1 : 1);
    zcl_secure_zero(bytes, sizeof(bytes));
    if (result == NULL) (void)cancel_review(id);
    return result;
}
