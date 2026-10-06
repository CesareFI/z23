/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "mnemonic_words.h"
#include "zcl_keys.h"

/* The managed caller allocates and owns every secret destination BEFORE entry,
 * and clears it in finally on refusal/exception. Thus a failing VM transfer
 * cannot orphan a partially filled secret array. Native scratch is always
 * cleared; no JNI operation follows an observed pending exception. */
static bool secret_destination(JNIEnv *env, jarray output, jsize capacity)
{
    if (env == NULL || output == NULL) return false;
    if ((*env)->ExceptionCheck(env)) return false;
    const jsize length = (*env)->GetArrayLength(env, output);
    if ((*env)->ExceptionCheck(env)) return false;
    return length == capacity;
}

/* Each private writer receives a prevalidated, caller-owned destination.
 * Java array lengths cannot change; successful input reads leave no exception. */
static jint write_secret_bytes(JNIEnv *env, jbyteArray output, const uint8_t *bytes, size_t length)
{
    if (bytes == NULL || length == 0 || length > 32) return 0;
    (*env)->SetByteArrayRegion(env, output, 0, (jsize)length, (const jbyte *)bytes);
    return (*env)->ExceptionCheck(env) ? 0 : (jint)length;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_createEntropy(JNIEnv *env, jclass type, jbyteArray output)
{
    (void)type;
    if (!secret_destination(env, output, 32)) return 0;
    uint8_t entropy[32] = {0};
    jint length = 0;
    if (zcl_random_bytes(entropy, sizeof(entropy)) == ZCL_OK) {
        (*env)->SetByteArrayRegion(env, output, 0, 32, (const jbyte *)entropy);
        if (!(*env)->ExceptionCheck(env)) length = 32;
    }
    zcl_secure_zero(entropy, sizeof(entropy));
    return length;
}

static jint write_phrase(JNIEnv *env, jcharArray output, const uint8_t *text, size_t length)
{
    if (text == NULL || length == 0 || length > 215) return 0;
    jchar chars[215] = {0};
    jint written = 0;
    for (size_t i = 0; i < length; ++i)
        chars[i] = (jchar)text[i];
    (*env)->SetCharArrayRegion(env, output, 0, (jsize)length, chars);
    if (!(*env)->ExceptionCheck(env)) written = (jint)length;
    zcl_secure_zero(chars, sizeof(chars));
    return written;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_recoveryPhrase(JNIEnv *env, jclass type,
    jbyteArray input, jcharArray output)
{
    (void)type;
    if (input == NULL || !secret_destination(env, output, 215)) return 0;
    uint8_t entropy[32] = {0}, text[215] = {0};
    size_t entropy_len = 0, text_len = 0;
    jint written = 0;
    bool ready = false;
    if (zcl_jni_read_bytes(env, input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (zcl_mnemonic_encode(entropy, entropy_len, text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    ready = true;
cleanup:
    /* Phrase encoding has consumed the entropy. Only its secret result is
     * needed while the VM transfers into the caller-owned destination. */
    zcl_secure_zero(entropy, sizeof(entropy));
    if (ready) written = write_phrase(env, output, text, text_len);
    zcl_secure_zero(text, sizeof(text));
    return written;
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

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_restoreEntropy(JNIEnv *env, jclass type,
    jcharArray input, jbyteArray output)
{
    (void)type;
    if (input == NULL || !secret_destination(env, output, 32)) return 0;
    uint8_t entropy[32] = {0}, text[215] = {0};
    size_t entropy_len = 0, text_len = 0;
    jint written = 0;
    bool ready = false;
    if (read_phrase(env, input, text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    if (zcl_mnemonic_decode(text, text_len, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    ready = true;
cleanup:
    /* Decoding has consumed the phrase. Retire that byte copy before the VM
     * transfers entropy; the UTF-16 read scratch was already cleared. */
    zcl_secure_zero(text, sizeof(text));
    if (ready) written = write_secret_bytes(env, output, entropy, entropy_len);
    zcl_secure_zero(entropy, sizeof(entropy));
    return written;
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
    bool ready = false;
    if (index < 0 || zcl_jni_network(chain, &network) != ZCL_OK)
        goto cleanup;
    if (zcl_jni_read_bytes(env, input, entropy, sizeof(entropy), &entropy_len) != ZCL_OK)
        goto cleanup;
    if (!zcl_entropy_length_valid(entropy_len))
        goto cleanup;
    if (zcl_random_bytes(blinding, sizeof(blinding)) != ZCL_OK)
        goto cleanup;
    if (zcl_receive_from_entropy(entropy, entropy_len, network, (uint32_t)index,
                                 blinding, sizeof(blinding), text, sizeof(text), &text_len) != ZCL_OK)
        goto cleanup;
    ready = true;
cleanup:
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    /* Only public bytes survive a potentially blocking/failing VM allocation. */
    if (ready) output = zcl_jni_new_bytes(env, text, text_len);
    zcl_secure_zero(text, sizeof(text));
    return output;
}
