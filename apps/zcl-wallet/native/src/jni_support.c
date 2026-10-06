/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"

#include <limits.h>

zcl_status zcl_jni_read_bytes(JNIEnv *env, jbyteArray input, uint8_t *bytes,
                              size_t capacity, size_t *length)
{
    if (env == NULL || input == NULL || bytes == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    jsize count = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    if (count < 0 || (size_t)count > capacity)
        return ZCL_OUT_OF_RANGE;
    (*env)->GetByteArrayRegion(env, input, 0, count, (jbyte *)bytes);
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    *length = (size_t)count;
    return ZCL_OK;
}

zcl_status zcl_jni_read_exact_bytes(JNIEnv *env, jbyteArray input, uint8_t *bytes,
                                    size_t length)
{
    if (env == NULL || input == NULL || bytes == NULL)
        return ZCL_INVALID_ARGUMENT;
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    const jsize count = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    if (count < 0 || (size_t)count != length)
        return ZCL_OUT_OF_RANGE;
    (*env)->GetByteArrayRegion(env, input, 0, count, (jbyte *)bytes);
    return (*env)->ExceptionCheck(env) ? ZCL_INVALID_ARGUMENT : ZCL_OK;
}

jbyteArray zcl_jni_new_bytes(JNIEnv *env, const uint8_t *bytes, size_t length)
{
    if (env == NULL || bytes == NULL || length > (size_t)INT32_MAX)
        return NULL;
    if ((*env)->ExceptionCheck(env))
        return NULL;
    jbyteArray result = (*env)->NewByteArray(env, (jsize)length);
    if (result == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    (*env)->SetByteArrayRegion(env, result, 0, (jsize)length, (const jbyte *)bytes);
    if ((*env)->ExceptionCheck(env))
        return NULL;
    return result;
}

zcl_status zcl_jni_network(jint value, zcl_network *network)
{
    if (network == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (value != (jint)ZCL_MAINNET && value != (jint)ZCL_TESTNET)
        return ZCL_UNSUPPORTED;
    *network = (zcl_network)value;
    return ZCL_OK;
}
