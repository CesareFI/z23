/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_change_reservation.h"
#include "zcl_storage.h"

/* Private JNI transport: one status byte; on success one pending byte and
 * the validated ciphertext record. This is not a disk or network format. */
_Static_assert(ZCL_IO_UNCERTAIN <= UINT8_MAX, "JNI status fits one byte");

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_readWalletStorage(JNIEnv *env, jclass type, jbyteArray path_input)
{
    (void)type;
    uint8_t path[1024] = {0}, packet[142] = {0};
    size_t path_len = 0, record_len = 0;
    bool pending = false;
    zcl_status status = zcl_jni_read_bytes(env, path_input, path, sizeof(path), &path_len);
    if (status == ZCL_OK)
        status = zcl_storage_read(path, path_len, packet + 2, sizeof(packet) - 2, &record_len, &pending);
    if ((*env)->ExceptionCheck(env))
        return NULL;
    packet[0] = (uint8_t)status;
    packet[1] = pending ? 1 : 0;
    return zcl_jni_new_bytes(env, packet, status == ZCL_OK ? record_len + 2 : 1);
}

static jint write_record(JNIEnv *env, jbyteArray path_input, jbyteArray record_input, bool promote)
{
    uint8_t path[1024] = {0}, record[140] = {0};
    size_t path_len = 0, record_len = 0;
    zcl_status status = zcl_jni_read_bytes(env, path_input, path, sizeof(path), &path_len);
    if (status != ZCL_OK)
        return (jint)status;
    status = zcl_jni_read_bytes(env, record_input, record, sizeof(record), &record_len);
    if (status != ZCL_OK)
        return (jint)status;
    if (promote)
        return (jint)zcl_storage_promote(path, path_len, record, record_len);
    return (jint)zcl_storage_create(path, path_len, record, record_len);
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_createWalletStorage(JNIEnv *env, jclass type,
                                                            jbyteArray path, jbyteArray record)
{
    (void)type;
    return write_record(env, path, record, false);
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_createFreshWalletStorage(JNIEnv *env, jclass type,
    jbyteArray path_input, jbyteArray record_input, jbyteArray entropy_input)
{
    (void)type;
    uint8_t path[1024] = {0}, record[140] = {0}, entropy[32] = {0};
    size_t path_len = 0, record_len = 0, entropy_len = 0;
    zcl_status status = zcl_jni_read_bytes(env, path_input, path, sizeof(path), &path_len);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, record_input, record, sizeof(record), &record_len);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, entropy_input, entropy, sizeof(entropy), &entropy_len);
    if (status == ZCL_OK)
        status = zcl_wallet_change_create(path, path_len, record, record_len, entropy, entropy_len);
    /* A VM read can partially write before throwing. Clear the complete native
     * secret span on every exit, without clearing the pending VM exception. */
    zcl_secure_zero(entropy, sizeof(entropy));
    return (jint)status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_promoteWalletStorage(JNIEnv *env, jclass type,
                                                             jbyteArray path, jbyteArray record)
{
    (void)type;
    return write_record(env, path, record, true);
}
