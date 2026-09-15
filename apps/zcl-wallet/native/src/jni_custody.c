/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_custody.h"

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

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_custodyWindowRemainingMillis(JNIEnv *env, jclass type,
    jboolean setup, jlong started, jlong now)
{
    (void)env;
    (void)type;
    _Static_assert(ZCL_AUTH_WINDOW_MS <= INT64_MAX && ZCL_SETUP_WINDOW_MS <= INT64_MAX,
                   "Custody delays must fit Java long");
    if (setup != JNI_FALSE && setup != JNI_TRUE) return 0;
    if (started < 0 || now < 0) return 0;
    uint64_t remaining = 0;
    const zcl_status status = setup == JNI_TRUE
        ? zcl_setup_window_remaining((uint64_t)started, (uint64_t)now, &remaining)
        : zcl_authentication_window_remaining((uint64_t)started, (uint64_t)now, &remaining);
    return status == ZCL_OK ? (jlong)remaining : 0;
}
