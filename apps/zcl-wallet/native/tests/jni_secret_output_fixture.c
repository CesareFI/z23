/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <jni.h>
#include <stdlib.h>

/* Host-JVM test only. Forward the actual production JNI adapter's array calls
 * to the real VM, then raise the caller's fixture exception after copying a
 * selected prefix. Never inject into an app library or create test entropy.
 * All state belongs to this invocation; no globals, pins or retained refs. */
typedef struct {
    JNIEnv table;
    JNIEnv *actual;
    jthrowable failure;
    jsize prefix;
} fixture_env;

JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_NativeCore_recoveryPhrase(
    JNIEnv *, jclass, jbyteArray, jcharArray);
JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_NativeCore_restoreEntropy(
    JNIEnv *, jclass, jcharArray, jbyteArray);
JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_NativeCore_packCameraPlane(
    JNIEnv *, jclass, jobject, jint, jint, jint, jint, jint, jint, jbyteArray);

static JNIEnv *live_env(JNIEnv *env)
{
    fixture_env *fixture = (fixture_env *)env; /* table is the first member. */
    if ((*fixture->actual)->ExceptionCheck(fixture->actual)) abort();
    return fixture->actual;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    JNIEnv *actual = ((fixture_env *)env)->actual;
    return (*actual)->ExceptionCheck(actual);
}

static jsize JNICALL array_length(JNIEnv *env, jarray array)
{
    JNIEnv *actual = live_env(env);
    return (*actual)->GetArrayLength(actual, array);
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize start, jsize length, jbyte *output)
{
    JNIEnv *actual = live_env(env);
    (*actual)->GetByteArrayRegion(actual, input, start, length, output);
}

static void JNICALL get_chars(JNIEnv *env, jcharArray input, jsize start, jsize length, jchar *output)
{
    JNIEnv *actual = live_env(env);
    (*actual)->GetCharArrayRegion(actual, input, start, length, output);
}

static jlong JNICALL buffer_capacity(JNIEnv *env, jobject buffer)
{
    JNIEnv *actual = live_env(env);
    return (*actual)->GetDirectBufferCapacity(actual, buffer);
}

static void *JNICALL buffer_address(JNIEnv *env, jobject buffer)
{
    JNIEnv *actual = live_env(env);
    return (*actual)->GetDirectBufferAddress(actual, buffer);
}

static void raise_failure(fixture_env *fixture)
{
    JNIEnv *actual = fixture->actual;
    if ((*actual)->ExceptionCheck(actual)) return;
    if ((*actual)->Throw(actual, fixture->failure) != JNI_OK) abort();
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray output, jsize start, jsize length, const jbyte *bytes)
{
    JNIEnv *actual = live_env(env);
    fixture_env *fixture = (fixture_env *)env;
    const jsize count = fixture->prefix < length ? fixture->prefix : length;
    (*actual)->SetByteArrayRegion(actual, output, start, count, bytes);
    raise_failure(fixture);
}

static void JNICALL set_chars(JNIEnv *env, jcharArray output, jsize start, jsize length, const jchar *chars)
{
    JNIEnv *actual = live_env(env);
    fixture_env *fixture = (fixture_env *)env;
    const jsize count = fixture->prefix < length ? fixture->prefix : length;
    (*actual)->SetCharArrayRegion(actual, output, start, count, chars);
    raise_failure(fixture);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .GetCharArrayRegion = get_chars,
    .SetByteArrayRegion = set_bytes, .SetCharArrayRegion = set_chars,
    .GetDirectBufferCapacity = buffer_capacity, .GetDirectBufferAddress = buffer_address
};

JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_SecretOutputNativeFixture_phrase(
    JNIEnv *env, jclass type, jbyteArray input, jcharArray output, jthrowable failure, jint prefix)
{
    if (env == NULL || failure == NULL || prefix < 0 || prefix > 215) return 0;
    if ((*env)->ExceptionCheck(env)) return 0;
    fixture_env fixture = {.table = &table, .actual = env, .failure = failure, .prefix = prefix};
    return Java_org_zclassic_wallet_core_NativeCore_recoveryPhrase(&fixture.table, type, input, output);
}

JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_SecretOutputNativeFixture_entropy(
    JNIEnv *env, jclass type, jcharArray input, jbyteArray output, jthrowable failure, jint prefix)
{
    if (env == NULL || failure == NULL || prefix < 0 || prefix > 32) return 0;
    if ((*env)->ExceptionCheck(env)) return 0;
    fixture_env fixture = {.table = &table, .actual = env, .failure = failure, .prefix = prefix};
    return Java_org_zclassic_wallet_core_NativeCore_restoreEntropy(&fixture.table, type, input, output);
}

JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_SecretOutputNativeFixture_camera(
    JNIEnv *env, jclass type, jobject plane, jbyteArray output, jthrowable failure, jint prefix)
{
    if (env == NULL || failure == NULL || prefix < 0 || prefix > 446) return 0;
    if ((*env)->ExceptionCheck(env)) return 0;
    fixture_env fixture = {.table = &table, .actual = env, .failure = failure, .prefix = prefix};
    return Java_org_zclassic_wallet_core_NativeCore_packCameraPlane(&fixture.table, type,
        plane, 0, 441, 21, 21, 21, 1, output);
}
