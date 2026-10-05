/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_wallet_record.h"

static zcl_status admit_wallet_header(const uint8_t *header, size_t length)
{
    zcl_wallet_info info = {0};
    const zcl_status status = zcl_wallet_header_parse(header, length, &info);
    zcl_secure_zero(&info, sizeof(info));
    return status;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_createWalletHeader(JNIEnv *env, jclass type,
                                                           jbyteArray input, jint chain)
{
    (void)type;
    uint8_t entropy[32] = {0}, blinding[32] = {0}, header[80] = {0};
    size_t entropy_len = 0;
    zcl_network network;
    bool ready = false;
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (zcl_random_bytes(blinding, sizeof(blinding)) != ZCL_OK)
        goto cleanup;
    if (zcl_wallet_header_create(entropy, entropy_len, network, blinding, sizeof(blinding),
                                 header, sizeof(header)) != ZCL_OK)
        goto cleanup;
    ready = true;
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    return ready ? zcl_jni_new_bytes(env, header, sizeof(header)) : NULL;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_recoveredWalletAddress(JNIEnv *env, jclass type,
                                                               jbyteArray header_input, jbyteArray entropy_input)
{
    (void)type;
    uint8_t entropy[32] = {0}, blinding[32] = {0}, header[80] = {0}, address[35] = {0};
    size_t entropy_len = 0, header_len = 0;
    bool ready = false;
    if (zcl_jni_read_bytes(env, header_input, header, sizeof(header), &header_len) != ZCL_OK)
        goto cleanup;
    if (admit_wallet_header(header, header_len) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, entropy_input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (zcl_random_bytes(blinding, sizeof(blinding)) != ZCL_OK)
        goto cleanup;
    if (zcl_wallet_recovered_address(header, header_len, entropy, entropy_len, blinding, sizeof(blinding),
                                     address, sizeof(address)) != ZCL_OK)
        goto cleanup;
    ready = true;
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    return ready ? zcl_jni_new_bytes(env, address, sizeof(address)) : NULL;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_packWalletRecord(JNIEnv *env, jclass type,
    jbyteArray header_input, jbyteArray iv_input, jbyteArray ciphertext_input)
{
    (void)type;
    uint8_t header[80] = {0}, iv[12] = {0}, ciphertext[48] = {0}, record[140] = {0};
    size_t header_len = 0, iv_len = 0, ciphertext_len = 0, record_len = 0;
    jbyteArray output = NULL;
    if (zcl_jni_read_bytes(env, header_input, header, sizeof(header), &header_len) != ZCL_OK)
        goto cleanup;
    if (admit_wallet_header(header, header_len) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, iv_input, iv, sizeof(iv), &iv_len) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, ciphertext_input, ciphertext, sizeof(ciphertext), &ciphertext_len) != ZCL_OK)
        goto cleanup;
    if (zcl_wallet_record_pack(header, header_len, iv, iv_len, ciphertext, ciphertext_len,
                               record, sizeof(record), &record_len) != ZCL_OK)
        goto cleanup;
    output = zcl_jni_new_bytes(env, record, record_len);
cleanup:
    zcl_secure_zero(header, sizeof(header));
    zcl_secure_zero(iv, sizeof(iv));
    zcl_secure_zero(ciphertext, sizeof(ciphertext));
    zcl_secure_zero(record, sizeof(record));
    return output;
}

static bool set_part(JNIEnv *env, jobjectArray array, jsize index, const uint8_t *data, size_t length)
{
    jbyteArray part = zcl_jni_new_bytes(env, data, length);
    if (part == NULL)
        return false;
    (*env)->SetObjectArrayElement(env, array, index, part);
    (*env)->DeleteLocalRef(env, part);
    return !(*env)->ExceptionCheck(env);
}

static jobjectArray new_parts(JNIEnv *env, const zcl_wallet_record *record)
{
    jclass byte_array = (*env)->FindClass(env, "[B");
    if (byte_array == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    jobjectArray parts = (*env)->NewObjectArray(env, 4, byte_array, NULL);
    (*env)->DeleteLocalRef(env, byte_array);
    if (parts == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    uint8_t network = (uint8_t)record->info.network;
    if (!set_part(env, parts, 0, record->header, sizeof(record->header)))
        return NULL;
    if (!set_part(env, parts, 1, record->iv, sizeof(record->iv)))
        return NULL;
    if (!set_part(env, parts, 2, record->ciphertext, record->ciphertext_len))
        return NULL;
    if (!set_part(env, parts, 3, &network, sizeof(network)))
        return NULL;
    return parts;
}

JNIEXPORT jobjectArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_unpackWalletRecord(JNIEnv *env, jclass type, jbyteArray input)
{
    (void)type;
    uint8_t bytes[140] = {0};
    size_t length = 0;
    zcl_wallet_record record = {0};
    jobjectArray output = NULL;
    if (zcl_jni_read_bytes(env, input, bytes, sizeof(bytes), &length) != ZCL_OK)
        goto cleanup;
    if (zcl_wallet_record_parse(bytes, length, &record) != ZCL_OK)
        goto cleanup;
    output = new_parts(env, &record);
cleanup:
    zcl_secure_zero(bytes, sizeof(bytes));
    zcl_secure_zero(&record, sizeof(record));
    return output;
}
