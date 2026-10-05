/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_draft_internal.h"
#include "zcl_keys.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(ZCL_DRAFT_PARAMETER_MAX <= INT32_MAX, "Draft parameters fit jsize");
_Static_assert(ZCL_TLS_FAILURE <= UINT8_MAX, "Draft result statuses fit a byte");

zcl_status zcl_jni_draft_count(JNIEnv *env, jarray array, size_t maximum, size_t *count)
{
    if (array == NULL) return ZCL_INVALID_ARGUMENT;
    const jsize length = (*env)->GetArrayLength(env, array);
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (length <= 0 || (size_t)length > maximum) return ZCL_OUT_OF_RANGE;
    *count = (size_t)length;
    return ZCL_OK;
}

zcl_status zcl_jni_draft_parameters(JNIEnv *env, jlongArray parameters, zcl_draft_request *request)
{
    size_t count = 0;
    zcl_status status = zcl_jni_draft_count(env, parameters, ZCL_DRAFT_PARAMETER_MAX, &count);
    if (status != ZCL_OK) return status;
    if (count != 3 + 2 * request->input_count + request->output_count) return ZCL_INVALID_ARGUMENT;
    jlong values[ZCL_DRAFT_PARAMETER_MAX] = {0};
    (*env)->GetLongArrayRegion(env, parameters, 0, (jsize)count, values);
    status = (*env)->ExceptionCheck(env) ? ZCL_INVALID_ARGUMENT
        : zcl_jni_draft_fields(values, count, request);
    zcl_secure_zero(values, sizeof(values));
    return status;
}

static zcl_status read_element(JNIEnv *env, jobjectArray array, size_t index,
                                uint8_t *bytes, size_t capacity, size_t *length)
{
    jobject element = (*env)->GetObjectArrayElement(env, array, (jsize)index);
    zcl_status status = ZCL_INVALID_ARGUMENT;
    if (element != NULL && !(*env)->ExceptionCheck(env))
        status = zcl_jni_read_bytes(env, (jbyteArray)element, bytes, capacity, length);
    if (element != NULL) (*env)->DeleteLocalRef(env, element);
    return status;
}

static zcl_status copy_previous(JNIEnv *env, jobjectArray previous, zcl_jni_draft_inputs *inputs)
{
    for (size_t i = 0; i < inputs->request.input_count; ++i) {
        size_t length = 0;
        const zcl_status status = read_element(env, previous, i, inputs->previous[i],
            sizeof(inputs->previous[i]), &length);
        if (status != ZCL_OK) return status;
        inputs->request.inputs[i].previous.wire = inputs->previous[i];
        inputs->request.inputs[i].previous.length = length;
    }
    return ZCL_OK;
}

zcl_status zcl_jni_draft_destinations(JNIEnv *env, jobjectArray destinations, zcl_draft_request *request)
{
    for (size_t i = 0; i < request->output_count; ++i) {
        uint8_t text[35] = {0};
        size_t length = 0;
        zcl_status status = read_element(env, destinations, i, text, sizeof(text), &length);
        if (status == ZCL_OK)
            status = zcl_address_parse(text, length, request->network, &request->outputs[i].destination);
        zcl_secure_zero(text, sizeof(text));
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status build_copied(JNIEnv *env, jobjectArray previous, jobjectArray destinations,
    jlongArray parameters, zcl_network network, uint8_t *wire, size_t capacity, size_t *length)
{
    size_t input_count = 0, output_count = 0;
    zcl_status status = zcl_jni_draft_count(env, previous, ZCL_TX_INPUT_MAX, &input_count);
    if (status != ZCL_OK) return status;
    status = zcl_jni_draft_count(env, destinations, ZCL_TX_OUTPUT_MAX, &output_count);
    if (status != ZCL_OK) return status;
    zcl_draft_request request = {0};
    request.network = network;
    request.input_count = input_count;
    request.output_count = output_count;
    status = zcl_jni_draft_parameters(env, parameters, &request);
    if (status == ZCL_OK) status = zcl_jni_draft_destinations(env, destinations, &request);
    if (status != ZCL_OK) {
        zcl_secure_zero(&request, sizeof(request));
        return status;
    }
    zcl_jni_draft_inputs *inputs = malloc(sizeof(*inputs));
    if (inputs == NULL) {
        zcl_secure_zero(&request, sizeof(request));
        return ZCL_RESOURCE_EXHAUSTED;
    }
    memset(inputs, 0, sizeof(*inputs));
    inputs->request = request;
    zcl_secure_zero(&request, sizeof(request));
    if (status == ZCL_OK) status = copy_previous(env, previous, inputs);
    if (status == ZCL_OK) status = zcl_jni_draft_wire(&inputs->request, wire, capacity, length);
    zcl_secure_zero(inputs, sizeof(*inputs));
    free(inputs);
    return status;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_buildDraft(JNIEnv *env, jclass type,
    jobjectArray previous, jobjectArray destinations, jlongArray parameters, jint chain)
{
    (void)type;
    if (env == NULL) return NULL;
    if ((*env)->ExceptionCheck(env)) return NULL;
    uint8_t bytes[ZCL_TX_WIRE_MAX + 1] = {0};
    size_t length = 0;
    zcl_network network;
    zcl_status status = zcl_jni_network(chain, &network);
    if (status == ZCL_OK)
        status = build_copied(env, previous, destinations, parameters, network,
            bytes + 1, sizeof(bytes) - 1, &length);
    /* Construction input storage and all local refs have already been released.
     * Never call a Java allocator while an input read exception is pending. */
    bytes[0] = (uint8_t)status;
    jbyteArray result = zcl_jni_new_bytes(env, bytes, status == ZCL_OK ? length + 1 : 1);
    zcl_secure_zero(bytes, sizeof(bytes));
    return result;
}
