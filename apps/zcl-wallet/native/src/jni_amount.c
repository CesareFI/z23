/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <jni.h>

/* No retained Java references, pins, heap allocation, or wallet logic here.
 * At most 17 Java bytes cross this boundary. Failure uses a negative status;
 * every successful monetary value fits in signed jlong. */
JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_parseAmount(JNIEnv *env, jclass type,
                                                   jbyteArray input)
{
    (void)type;
    if (env == NULL || input == NULL)
        return -(jlong)ZCL_INVALID_ARGUMENT;
    jsize length = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env))
        return -(jlong)ZCL_INVALID_ARGUMENT;
    if (length <= 0 || (size_t)length > ZCL_AMOUNT_TEXT_MAX)
        return -(jlong)ZCL_INVALID_ENCODING;
    jbyte bytes[17] = {0};
    (*env)->GetByteArrayRegion(env, input, 0, length, bytes);
    if ((*env)->ExceptionCheck(env))
        return -(jlong)ZCL_INVALID_ARGUMENT;
    uint64_t amount = 0;
    zcl_status status = zcl_amount_parse((const uint8_t *)bytes, (size_t)length, &amount);
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
    /* Core contract bounds length to 17, so conversion to jsize is exact. */
    jbyteArray result = (*env)->NewByteArray(env, (jsize)length);
    if (result == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    (*env)->SetByteArrayRegion(env, result, 0, (jsize)length, (const jbyte *)bytes);
    if ((*env)->ExceptionCheck(env))
        return NULL;
    return result;
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_changeAmount(JNIEnv *env, jclass type,
                                                    jlong left, jlong right,
                                                    jboolean subtract)
{
    (void)env;
    (void)type;
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
