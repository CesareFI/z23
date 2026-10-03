/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_review_internal.h"
#include "transaction_review_internal.h"
#include "transaction_source_internal.h"
#include "zcl_keys.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(ZCL_V4_SOURCE_MAX <= INT32_MAX, "Source regions fit jsize");
_Static_assert(ZCL_TX_INPUT_MAX <= SIZE_MAX / ZCL_V4_SOURCE_MAX, "Source aggregate fits size_t");
#define SOURCE_TOTAL_MAX (ZCL_TX_INPUT_MAX * ZCL_V4_SOURCE_MAX)

static void release_references(JNIEnv *env, zcl_jni_full_sources *copy)
{
    for (size_t i = 0; i < copy->count; ++i) {
        if (copy->arrays[i] != NULL) (*env)->DeleteLocalRef(env, copy->arrays[i]);
        copy->arrays[i] = NULL;
    }
}

static zcl_status capture_one(JNIEnv *env, jobjectArray previous,
    zcl_jni_full_sources *copy, size_t index)
{
    copy->arrays[index] = (jbyteArray)(*env)->GetObjectArrayElement(env, previous, (jsize)index);
    if ((*env)->ExceptionCheck(env) || copy->arrays[index] == NULL) return ZCL_INVALID_ARGUMENT;
    const jsize length = (*env)->GetArrayLength(env, copy->arrays[index]);
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (length <= 0) return ZCL_OUT_OF_RANGE;
    if ((size_t)length > ZCL_V4_SOURCE_MAX || (size_t)length > SOURCE_TOTAL_MAX - copy->total)
        return ZCL_RESOURCE_EXHAUSTED;
    copy->lengths[index] = (size_t)length;
    copy->total += (size_t)length;
    return ZCL_OK;
}

static zcl_status capture_sources(JNIEnv *env, jobjectArray previous, zcl_jni_full_sources *copy)
{
    const jsize count = (*env)->GetArrayLength(env, previous);
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (count <= 0 || (size_t)count > ZCL_TX_INPUT_MAX) return ZCL_OUT_OF_RANGE;
    copy->count = (size_t)count;
    for (size_t i = 0; i < copy->count; ++i) {
        const zcl_status status = capture_one(env, previous, copy, i);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status copy_sources(JNIEnv *env, zcl_jni_full_sources *copy)
{
    copy->bytes = malloc(copy->total); /* Positive, <=816000 after complete capture. */
    if (copy->bytes == NULL) return ZCL_RESOURCE_EXHAUSTED;
    memset(copy->bytes, 0, copy->total);
    size_t used = 0;
    for (size_t i = 0; i < copy->count; ++i) {
        const size_t length = copy->lengths[i];
        if (length > copy->total - used) return ZCL_RESOURCE_EXHAUSTED;
        (*env)->GetByteArrayRegion(env, copy->arrays[i], 0, (jsize)length, (jbyte *)(copy->bytes + used));
        if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
        copy->sources[i].wire = copy->bytes + used;
        copy->sources[i].length = length;
        used += length;
    }
    return ZCL_OK;
}

zcl_status zcl_jni_full_sources_copy(JNIEnv *env, jobjectArray previous, zcl_jni_full_sources *copy)
{
    zcl_status status = capture_sources(env, previous, copy);
    if (status == ZCL_OK) status = copy_sources(env, copy);
    release_references(env, copy); /* Permitted with a pending JNI exception. */
    return status;
}

void zcl_jni_full_sources_clear(zcl_jni_full_sources *copy)
{
    uint8_t *bytes = copy->bytes;
    const size_t total = copy->total;
    zcl_secure_zero(copy, sizeof(*copy));
    if (bytes != NULL) {
        zcl_secure_zero(bytes, total);
        free(bytes);
    }
}

zcl_status zcl_jni_open_full_review(JNIEnv *env, zcl_review_owner *owner,
    jbyteArray draft, jobjectArray previous, zcl_network network,
    uint64_t fee, uint64_t now, uint64_t *id)
{
    uint8_t wire[ZCL_TX_WIRE_MAX] = {0};
    size_t length = 0;
    zcl_jni_full_sources copy = {0};
    zcl_status status = zcl_jni_read_bytes(env, draft, wire, sizeof(wire), &length);
    /* Admit the owned draft and immutable source-array count before a source
     * allocation. The existing C codec owns every wire predicate; full review
     * still revalidates and assesses the same owned bytes after source capture. */
    if (status == ZCL_OK) status = zcl_jni_review_admit(env, previous, wire, length);
    if (status == ZCL_OK) status = zcl_jni_full_sources_copy(env, previous, &copy);
    if (status == ZCL_OK)
        status = zcl_review_open_full_sources(owner, wire, length, network,
            copy.sources, copy.count, fee, now, id);
    zcl_secure_zero(wire, sizeof(wire));
    zcl_jni_full_sources_clear(&copy);
    return status;
}
