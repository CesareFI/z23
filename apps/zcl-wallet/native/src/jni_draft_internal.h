/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_JNI_DRAFT_INTERNAL_H
#define ZCL_JNI_DRAFT_INTERNAL_H
#include "jni_support.h"
#include "zcl_transaction_draft.h"

#define ZCL_DRAFT_PARAMETER_MAX ((size_t)3 + 2 * ZCL_TX_INPUT_MAX + ZCL_TX_OUTPUT_MAX)

/* One fixed allocation owns all borrowed previous bytes until construction
 * returns. No JVM ref, pin, pointer or draft owner survives this operation. */
typedef struct {
    zcl_draft_request request;
    uint8_t previous[ZCL_TX_INPUT_MAX][ZCL_TX_WIRE_MAX];
} zcl_jni_draft_inputs;

/* Private preparation only: discard/clear the entire request on any failure.
 * Fields: lock/expiry/fee, input index/sequence pairs, then output values. */
zcl_status zcl_jni_draft_fields(const jlong *values, size_t count, zcl_draft_request *request);
zcl_status zcl_jni_draft_count(JNIEnv *env, jarray array, size_t maximum, size_t *count);
zcl_status zcl_jni_draft_parameters(JNIEnv *env, jlongArray parameters, zcl_draft_request *request);
zcl_status zcl_jni_draft_destinations(JNIEnv *env, jobjectArray destinations, zcl_draft_request *request);
/* Separate bounded frame for the2200-byte transaction above the JNI wire frame. */
zcl_status zcl_jni_draft_wire(const zcl_draft_request *request,
                             uint8_t *wire, size_t capacity, size_t *length);
zcl_status zcl_jni_draft_full_wire(const zcl_draft_request *request,
                             uint8_t *wire, size_t capacity, size_t *length);
#endif
