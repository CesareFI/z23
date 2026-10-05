/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_change_reservation.h"
#include "zcl_storage.h"
#include "change_custody_internal.h"

/* Private JNI transport: one status byte; on success one pending byte and
 * the validated ciphertext record. This is not a disk or network format. */
_Static_assert(ZCL_IO_UNCERTAIN <= UINT8_MAX, "JNI status fits one byte");

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_readWalletStorage(JNIEnv *env, jclass type, jbyteArray path_input)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    uint8_t path[1024] = {0}, packet[142] = {0};
    size_t path_len = 0, record_len = 0;
    bool pending = false;
    zcl_status status = zcl_jni_read_bytes(env, path_input, path, sizeof(path), &path_len);
    if (status == ZCL_OK)
        status = zcl_storage_read(path, path_len, packet + 2, sizeof(packet) - 2, &record_len, &pending);
    zcl_secure_zero(path, sizeof(path));
    jbyteArray result = NULL;
    if (!(*env)->ExceptionCheck(env)) {
        packet[0] = (uint8_t)status;
        packet[1] = pending ? 1 : 0;
        result = zcl_jni_new_bytes(env, packet, status == ZCL_OK ? record_len + 2 : 1);
    }
    zcl_secure_zero(packet, sizeof(packet));
    return result;
}

static jint write_record(JNIEnv *env, jbyteArray path_input, jbyteArray record_input, bool promote)
{
    uint8_t path[1024] = {0}, record[140] = {0};
    size_t path_len = 0, record_len = 0;
    zcl_status status = zcl_jni_read_bytes(env, path_input, path, sizeof(path), &path_len);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, record_input, record, sizeof(record), &record_len);
    if (status == ZCL_OK)
        status = promote ? zcl_storage_promote(path, path_len, record, record_len) :
            zcl_storage_create(path, path_len, record, record_len);
    zcl_secure_zero(record, sizeof(record));
    zcl_secure_zero(path, sizeof(path));
    return (jint)status;
}

static zcl_status admit_wallet_record(const uint8_t *record, size_t length)
{
    zcl_wallet_record parsed = {0};
    const zcl_status status = zcl_wallet_record_parse(record, length, &parsed);
    zcl_secure_zero(&parsed, sizeof(parsed));
    return status;
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
        status = admit_wallet_record(record, record_len);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, entropy_input, entropy, sizeof(entropy), &entropy_len);
    if (status == ZCL_OK)
        status = zcl_wallet_change_create_owned(path, path_len, record, record_len,
            entropy, entropy_len, sizeof(entropy));
    /* A VM read can partially write before throwing. Clear the complete native
     * secret span on every exit, without clearing the pending VM exception. */
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(record, sizeof(record));
    zcl_secure_zero(path, sizeof(path));
    return (jint)status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_promoteWalletStorage(JNIEnv *env, jclass type,
                                                             jbyteArray path, jbyteArray record)
{
    (void)type;
    return write_record(env, path, record, true);
}
