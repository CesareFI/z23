/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"

/* No retained Java references, pins, heap allocation, or wallet logic here.
 * At most 18 public Java bytes cross this boundary. Failure uses a negative status;
 * every successful monetary value fits in signed jlong. */
JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_parseAmount(JNIEnv *env, jclass type,
                                                   jbyteArray input)
{
    (void)type;
    uint8_t bytes[ZCL_AMOUNT_TEXT_MAX] = {0};
    size_t length = 0;
    zcl_status status = zcl_jni_read_bytes(env, input, bytes, sizeof(bytes), &length);
    if (status != ZCL_OK)
        return -(jlong)(status == ZCL_OUT_OF_RANGE ? ZCL_INVALID_ENCODING : status);
    uint64_t amount = 0;
    status = zcl_amount_parse(bytes, length, &amount);
    if (status != ZCL_OK)
        return -(jlong)status;
    return (jlong)amount;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_formatAmount(JNIEnv *env, jclass type,
                                                    jlong amount)
{
    (void)type;
    if (env == NULL || amount < 0)
        return NULL;
    uint8_t bytes[17] = {0};
    size_t length = 0;
    if (zcl_amount_format((uint64_t)amount, bytes, sizeof(bytes), &length) != ZCL_OK)
        return NULL;
    return zcl_jni_new_bytes(env, bytes, length);
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_formatAmountDelta(JNIEnv *env, jclass type,
                                                         jlong delta)
{
    (void)type;
    if (env == NULL)
        return NULL;
    uint8_t bytes[18] = {0};
    size_t length = 0;
    /* JNI jlong and int64_t both have exactly the signed 64-bit range. */
    if (zcl_amount_delta_format((int64_t)delta, bytes, sizeof(bytes), &length) != ZCL_OK)
        return NULL;
    return zcl_jni_new_bytes(env, bytes, length);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_changeAmount(JNIEnv *env, jclass type,
                                                    jlong left, jlong right,
                                                    jboolean subtract)
{
    (void)env;
    (void)type;
    if (subtract != JNI_FALSE && subtract != JNI_TRUE)
        return -(jlong)ZCL_INVALID_ARGUMENT;
    if (left < 0 || right < 0)
        return -(jlong)ZCL_OUT_OF_RANGE;
    uint64_t result = 0;
    zcl_status status;
    if (subtract == JNI_TRUE)
        status = zcl_amount_subtract((uint64_t)left, (uint64_t)right, &result);
    else
        status = zcl_amount_add((uint64_t)left, (uint64_t)right, &result);
    if (status != ZCL_OK)
        return -(jlong)status;
    return (jlong)result;
}
