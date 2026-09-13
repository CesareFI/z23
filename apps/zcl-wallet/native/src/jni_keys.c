/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_keys.h"

/* Only newly allocated Java arrays cross this adapter. The VM owns returned
 * arrays; its caller clears them. Every native secret buffer is cleared before
 * return, including allocation failure or a pending Java exception. */
JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_createEntropy(JNIEnv *env, jclass type)
{
    (void)type;
    if (env == NULL) return NULL;
    if ((*env)->ExceptionCheck(env)) return NULL;
    uint8_t entropy[32] = {0};
    jbyteArray output = NULL;
    if (zcl_random_bytes(entropy, sizeof(entropy)) == ZCL_OK)
        output = zcl_jni_new_bytes(env, entropy, sizeof(entropy));
    zcl_secure_zero(entropy, sizeof(entropy));
    return output;
}

static jcharArray new_phrase(JNIEnv *env, const uint8_t *text, size_t length)
{
    if (env == NULL || text == NULL || length > 215)
        return NULL;
    if ((*env)->ExceptionCheck(env))
        return NULL;
    jchar chars[215] = {0};
    jcharArray output = NULL;
    for (size_t i = 0; i < length; ++i)
        chars[i] = (jchar)text[i];
    output = (*env)->NewCharArray(env, (jsize)length);
    if (output == NULL || (*env)->ExceptionCheck(env))
        goto cleanup;
    (*env)->SetCharArrayRegion(env, output, 0, (jsize)length, chars);
    if ((*env)->ExceptionCheck(env))
        output = NULL;
cleanup:
    zcl_secure_zero(chars, sizeof(chars));
    return output;
}

JNIEXPORT jcharArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_recoveryPhrase(JNIEnv *env, jclass type, jbyteArray input)
{
    (void)type;
    uint8_t entropy[32] = {0}, text[215] = {0};
    size_t entropy_len = 0, text_len = 0;
    jcharArray output = NULL;
    if (zcl_jni_read_bytes(env, input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (zcl_mnemonic_encode(entropy, entropy_len, text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    output = new_phrase(env, text, text_len);
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(text, sizeof(text));
    return output;
}

static zcl_status phrase_size(JNIEnv *env, jcharArray input, size_t capacity, jsize *length)
{
    if (env == NULL || input == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    jsize size = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env))
        return ZCL_INVALID_ARGUMENT;
    if (size < 0 || (size_t)size > 215 || (size_t)size > capacity)
        return ZCL_OUT_OF_RANGE;
    *length = size;
    return ZCL_OK;
}

static zcl_status read_phrase(JNIEnv *env, jcharArray input, uint8_t *text,
                              size_t capacity, size_t *length)
{
    if (text == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    jsize size = 0;
    zcl_status status = phrase_size(env, input, capacity, &size);
    if (status != ZCL_OK)
        return status;
    jchar chars[215] = {0};
    status = ZCL_INVALID_ENCODING;
    (*env)->GetCharArrayRegion(env, input, 0, size, chars);
    if ((*env)->ExceptionCheck(env))
        goto cleanup;
    for (size_t i = 0; i < (size_t)size; ++i) {
        if (chars[i] > 0x7f)
            goto cleanup;
        text[i] = (uint8_t)chars[i];
    }
    *length = (size_t)size;
    status = ZCL_OK;
cleanup:
    zcl_secure_zero(chars, sizeof(chars));
    return status;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_restoreEntropy(JNIEnv *env, jclass type, jcharArray input)
{
    (void)type;
    uint8_t entropy[32] = {0}, text[215] = {0};
    size_t entropy_len = 0, text_len = 0;
    jbyteArray output = NULL;
    if (read_phrase(env, input, text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    if (zcl_mnemonic_decode(text, text_len, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    output = zcl_jni_new_bytes(env, entropy, entropy_len);
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(text, sizeof(text));
    return output;
}

JNIEXPORT jboolean JNICALL
Java_org_zclassic_wallet_core_NativeCore_confirmRecoveryPhrase(JNIEnv *env, jclass type,
                                                              jbyteArray entropy_input, jcharArray phrase_input)
{
    (void)type;
    uint8_t entropy[32] = {0}, phrase[215] = {0};
    size_t entropy_len = 0, phrase_len = 0;
    jboolean result = JNI_FALSE;
    if (zcl_jni_read_bytes(env, entropy_input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (read_phrase(env, phrase_input, phrase, sizeof(phrase), &phrase_len) != ZCL_OK)
        goto cleanup;
    if (zcl_mnemonic_confirm(entropy, entropy_len, phrase, phrase_len) == ZCL_OK)
        result = JNI_TRUE;
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(phrase, sizeof(phrase));
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_receivingAddress(JNIEnv *env, jclass type,
                                                         jbyteArray input, jint chain, jint index)
{
    (void)type;
    uint8_t entropy[32] = {0}, blinding[32] = {0}, text[35] = {0};
    size_t entropy_len = 0, text_len = 0;
    zcl_network network;
    jbyteArray output = NULL;
    if (index < 0 || zcl_jni_network(chain, &network) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (zcl_random_bytes(blinding, sizeof(blinding)) != ZCL_OK)
        goto cleanup;
    if (zcl_receive_from_entropy(entropy, entropy_len, network, (uint32_t)index,
                                 blinding, sizeof(blinding), text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    output = zcl_jni_new_bytes(env, text, text_len);
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    zcl_secure_zero(text, sizeof(text));
    return output;
}
