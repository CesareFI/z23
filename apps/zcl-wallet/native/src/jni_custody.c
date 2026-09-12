/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_custody.h"

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_authenticationWindowMillis(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    _Static_assert(ZCL_AUTH_WINDOW_MS <= INT64_MAX, "Authentication delay must fit Java long");
    return (jlong)ZCL_AUTH_WINDOW_MS;
}

JNIEXPORT jboolean JNICALL
Java_org_zclassic_wallet_core_NativeCore_authenticationWindowOpen(JNIEnv *env, jclass type,
    jlong started, jlong now)
{
    (void)env;
    (void)type;
    if (started < 0 || now < 0) return JNI_FALSE;
    return zcl_authentication_window_check((uint64_t)started, (uint64_t)now) == ZCL_OK ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_zclassic_wallet_core_NativeCore_acceptWrappingPolicy(JNIEnv *env, jclass type,
    jint bits, jint hardware, jint flags, jint seconds, jint methods)
{
    (void)env;
    (void)type;
    if (bits < 0 || hardware < 0 || flags < 0 || methods < 0)
        return JNI_FALSE;
    zcl_wrapping_policy policy = {(uint32_t)bits, (uint32_t)hardware, (uint32_t)flags,
                                  (int32_t)seconds, (uint32_t)methods};
    return zcl_wrapping_policy_check(&policy) == ZCL_OK ? JNI_TRUE : JNI_FALSE;
}
