/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_random_bytes
#include "jni_support.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI key check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jbyteArray JNICALL API(createEntropy)(JNIEnv *, jclass);
JNIEXPORT jcharArray JNICALL API(recoveryPhrase)(JNIEnv *, jclass, jbyteArray);
JNIEXPORT jbyteArray JNICALL API(restoreEntropy)(JNIEnv *, jclass, jcharArray);
JNIEXPORT jboolean JNICALL API(confirmRecoveryPhrase)(JNIEnv *, jclass, jbyteArray, jcharArray);
JNIEXPORT jbyteArray JNICALL API(receivingAddress)(JNIEnv *, jclass, jbyteArray, jint, jint);

typedef enum { BYTES, CHARS } array_kind;
typedef struct {
    array_kind kind;
    jsize length;
    union { uint8_t bytes[216]; jchar chars[216]; } data;
} fake_array;
typedef struct { const void *pointer; size_t length; bool cleared; } touched_span;
static fake_array entropy, phrase, result_array;
static touched_span touched[8];
static size_t touched_count;
static bool pending, fail_random, null_without_exception;
static unsigned fail_call, vm_calls, random_calls;
static const char known_phrase[] = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";

static void track(const void *pointer, size_t length)
{
    if (length == 0) return;
    CHECK(pointer != NULL);
    for (size_t i = 0; i < touched_count; ++i) {
        if (touched[i].pointer != pointer) continue;
        CHECK(!touched[i].cleared);
        if (touched[i].length < length) touched[i].length = length;
        return;
    }
    CHECK(touched_count < sizeof(touched) / sizeof(touched[0]));
    touched[touched_count++] = (touched_span){pointer, length, false};
}

void zcl_jni_key_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL && length <= 430);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    for (size_t i = 0; i < touched_count; ++i) {
        if (touched[i].pointer != pointer) continue;
        CHECK(length >= touched[i].length);
        touched[i].cleared = true;
        touched[i].pointer = NULL; /* Do not retain a pointer past this span's lifetime. */
        touched[i].length = 0;
    }
}

zcl_status zcl_jni_key_test_random(uint8_t *output, size_t length)
{
    CHECK(output != NULL && length == 32 && !pending);
    ++random_calls;
    track(output, length);
    if (fail_random) return ZCL_IO_FAILURE;
    memset(output, 0x42, length); /* Public test bytes, never the shipped RNG. */
    return ZCL_OK;
}

static bool vm_fault(void)
{
    CHECK(!pending); /* A JNI call forbidden with an outstanding exception. */
    ++vm_calls;
    if (vm_calls != fail_call) return false;
    pending = true;
    return true;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input != NULL);
    return vm_fault() ? 0 : ((fake_array *)input)->length;
}

static fake_array *region(jarray input, jsize start, jsize length, array_kind kind)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == kind);
    CHECK(start >= 0 && length >= 0 && start <= array->length && length <= array->length - start);
    CHECK(start <= 216 && length <= 216 - start);
    return array;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize start, jsize length, jbyte *output)
{
    (void)env;
    const fake_array *array = region(input, start, length, BYTES);
    track(output, (size_t)length);
    if (vm_fault()) { if (length > 0) output[0] = 42; return; }
    memcpy(output, array->data.bytes + (size_t)start, (size_t)length);
}

static void JNICALL get_chars(JNIEnv *env, jcharArray input, jsize start, jsize length, jchar *output)
{
    (void)env;
    const fake_array *array = region(input, start, length, CHARS);
    track(output, (size_t)length * sizeof(*output));
    if (vm_fault()) { if (length > 0) output[0] = 0x1234; return; }
    memcpy(output, array->data.chars + (size_t)start, (size_t)length * sizeof(*output));
}

static fake_array *new_array(jsize length, array_kind kind)
{
    CHECK(length >= 0 && length <= 215);
    if (vm_fault()) {
        if (null_without_exception) pending = false;
        return NULL;
    }
    result_array = (fake_array){.kind = kind, .length = length};
    return &result_array;
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    return (jbyteArray)new_array(length, BYTES);
}

static jcharArray JNICALL new_chars(JNIEnv *env, jsize length)
{
    (void)env;
    return (jcharArray)new_array(length, CHARS);
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize start, jsize length, const jbyte *bytes)
{
    (void)env;
    fake_array *array = region(input, start, length, BYTES);
    track(bytes, (size_t)length);
    memcpy(array->data.bytes + (size_t)start, bytes, (size_t)length);
    (void)vm_fault();
}

static void JNICALL set_chars(JNIEnv *env, jcharArray input, jsize start, jsize length, const jchar *chars)
{
    (void)env;
    fake_array *array = region(input, start, length, CHARS);
    track(chars, (size_t)length * sizeof(*chars));
    memcpy(array->data.chars + (size_t)start, chars, (size_t)length * sizeof(*chars));
    (void)vm_fault();
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .GetCharArrayRegion = get_chars,
    .NewByteArray = new_bytes, .NewCharArray = new_chars,
    .SetByteArrayRegion = set_bytes, .SetCharArrayRegion = set_chars
};
static JNIEnv environment = &table;

static void prepare(void)
{
    touched_count = 0;
    memset(touched, 0, sizeof(touched));
    pending = fail_random = null_without_exception = false;
    fail_call = vm_calls = random_calls = 0;
    entropy = (fake_array){.kind = BYTES, .length = 16};
    phrase = (fake_array){.kind = CHARS, .length = (jsize)(sizeof(known_phrase) - 1)};
    for (size_t i = 0; i < sizeof(known_phrase) - 1; ++i) phrase.data.chars[i] = (jchar)known_phrase[i];
    memset(&result_array, 0, sizeof(result_array));
}

static void verify_cleanup(void)
{
    for (size_t i = 0; i < touched_count; ++i) CHECK(touched[i].cleared);
}

/* Confirmation is the only boolean entry; translate its result for the common
 * cleanup/fault runner without manufacturing an array reference. */
static bool invoke(unsigned operation, JNIEnv *env, jbyteArray bytes, jcharArray chars, jint network, jint index)
{
    switch (operation) {
    case 0: return API(createEntropy)(env, NULL) != NULL;
    case 1: return API(recoveryPhrase)(env, NULL, bytes) != NULL;
    case 2: return API(restoreEntropy)(env, NULL, chars) != NULL;
    case 3: return API(confirmRecoveryPhrase)(env, NULL, bytes, chars) == JNI_TRUE;
    default: return API(receivingAddress)(env, NULL, bytes, network, index) != NULL;
    }
}

static bool run(unsigned operation)
{
    const bool accepted = invoke(operation, &environment, (jbyteArray)&entropy, (jcharArray)&phrase, 0, 0);
    verify_cleanup();
    return accepted;
}

#ifndef ZCL_JNI_KEYS_FUZZ
static void pending_helpers(void)
{
    prepare();
    pending = true;
    uint8_t bytes[32];
    memset(bytes, 0xa5, sizeof(bytes));
    size_t length = SIZE_MAX;
    CHECK(zcl_jni_read_bytes(&environment, (jbyteArray)&entropy, bytes, sizeof(bytes), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(length == SIZE_MAX && vm_calls == 0 && pending);
    for (size_t i = 0; i < sizeof(bytes); ++i) CHECK(bytes[i] == 0xa5);
    CHECK(zcl_jni_new_bytes(&environment, bytes, sizeof(bytes)) == NULL && vm_calls == 0 && pending);
}

static void pending_and_null_entries(void)
{
    for (unsigned operation = 0; operation < 5; ++operation) {
        prepare(); pending = true;
        CHECK(!run(operation) && pending && vm_calls == 0 && random_calls == 0);
        prepare();
        CHECK(!invoke(operation, NULL, (jbyteArray)&entropy, (jcharArray)&phrase, 0, 0));
        verify_cleanup();
        CHECK(vm_calls == 0 && random_calls == 0);
        if (operation == 0) continue;
        prepare();
        CHECK(!invoke(operation, &environment, NULL, NULL, 0, 0));
        verify_cleanup();
        CHECK(vm_calls == 0 && random_calls == 0);
    }
}

static void exact_results(void)
{
    for (unsigned operation = 0; operation < 5; ++operation) {
        prepare(); CHECK(run(operation));
        if (operation == 0) {
            CHECK(result_array.kind == BYTES && result_array.length == 32 && random_calls == 1);
            for (size_t i = 0; i < 32; ++i) CHECK(result_array.data.bytes[i] == 0x42);
        } else if (operation == 1) {
            CHECK(result_array.kind == CHARS && result_array.length == phrase.length);
            CHECK(memcmp(result_array.data.chars, phrase.data.chars, (size_t)phrase.length * sizeof(jchar)) == 0);
        } else if (operation == 2) {
            CHECK(result_array.kind == BYTES && result_array.length == 16);
            for (size_t i = 0; i < 16; ++i) CHECK(result_array.data.bytes[i] == 0);
        } else if (operation == 4) {
            uint8_t expected[35], blinding[32] = {1};
            size_t length = 0;
            CHECK(zcl_receive_from_entropy(entropy.data.bytes, 16, ZCL_MAINNET, 0,
                blinding, sizeof(blinding), expected, sizeof(expected), &length) == ZCL_OK);
            CHECK(result_array.kind == BYTES && result_array.length == 35);
            CHECK(length == 35 && memcmp(result_array.data.bytes, expected, 35) == 0);
        }
        CHECK(entropy.length == 16 && phrase.length == (jsize)(sizeof(known_phrase) - 1));
        for (size_t i = 0; i < sizeof(entropy.data.bytes); ++i) CHECK(entropy.data.bytes[i] == 0);
    }
}

static void vm_failures(void)
{
    const unsigned calls[] = {2, 4, 4, 4, 4};
    for (unsigned operation = 0; operation < 5; ++operation) {
        for (unsigned failure = 1; failure <= calls[operation]; ++failure) {
            prepare(); fail_call = failure;
            CHECK(!run(operation) && pending && vm_calls == failure);
        }
        if (operation == 3) continue;
        prepare(); fail_call = operation == 0 ? 1 : 3;
        null_without_exception = true;
        CHECK(!run(operation) && !pending);
    }
    for (unsigned operation = 0; operation < 5; operation += 4) {
        prepare(); fail_random = true;
        CHECK(!run(operation) && random_calls == 1 && !pending);
    }
}

static void invalid_inputs(void)
{
    const jsize invalid[] = {-1, 0, 33, 216, INT32_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (unsigned operation = 1; operation < 5; ++operation) {
            prepare();
            if (operation == 2) phrase.length = invalid[i];
            else entropy.length = invalid[i];
            CHECK(!run(operation));
        }
    }
    for (unsigned operation = 2; operation <= 3; ++operation) {
        prepare(); phrase.data.chars[phrase.length - 1] = 0x100;
        CHECK(!run(operation) && !pending);
        prepare(); phrase.data.chars[0] = 0;
        CHECK(!run(operation) && !pending);
    }
    const jint indexes[] = {-1, INT32_MIN};
    for (size_t i = 0; i < 2; ++i) {
        prepare();
        CHECK(!invoke(4, &environment, (jbyteArray)&entropy, NULL, 0, indexes[i]));
        verify_cleanup(); CHECK(vm_calls == 0 && random_calls == 0);
    }
    prepare();
    CHECK(!invoke(4, &environment, (jbyteArray)&entropy, NULL, 2, 0));
    verify_cleanup(); CHECK(vm_calls == 0 && random_calls == 0);
}

int main(void)
{
    pending_helpers(); pending_and_null_entries(); exact_results(); vm_failures(); invalid_inputs();
    puts("JNI key exception and secret cleanup checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void verify_phrase(const fake_array *words, const fake_array *bytes)
{
    CHECK(words->kind == CHARS && words->length > 0 && words->length <= 215);
    CHECK(bytes->kind == BYTES && bytes->length >= 16 && bytes->length <= 32);
    uint8_t ascii[215];
    for (size_t i = 0; i < (size_t)words->length; ++i) {
        CHECK(words->data.chars[i] <= 0x7f);
        ascii[i] = (uint8_t)words->data.chars[i];
    }
    CHECK(zcl_mnemonic_confirm(bytes->data.bytes, (size_t)bytes->length,
        ascii, (size_t)words->length) == ZCL_OK);
    zcl_secure_zero(ascii, sizeof(ascii));
}

static void verify_accepted(unsigned operation)
{
    CHECK(!pending);
    switch (operation) {
    case 0:
        CHECK(result_array.kind == BYTES && result_array.length == 32);
        for (size_t i = 0; i < 32; ++i) CHECK(result_array.data.bytes[i] == 0x42);
        break;
    case 1: verify_phrase(&result_array, &entropy); break;
    case 2: verify_phrase(&phrase, &result_array); break;
    case 3: verify_phrase(&phrase, &entropy); break;
    default: {
        zcl_address address;
        CHECK(result_array.kind == BYTES && result_array.length == 35);
        CHECK(zcl_address_parse(result_array.data.bytes, 35, ZCL_MAINNET, &address) == ZCL_OK);
        CHECK(address.kind == ZCL_P2PKH);
        break;
    }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 217) return 0;
    prepare();
    const unsigned operation = data[0] % 5;
    fail_call = data[1] % 8;
    pending = (data[1] & 0x80) != 0;
    fail_random = (data[1] & 0x40) != 0;
    null_without_exception = (data[1] & 0x20) != 0;
    if ((data[0] & 0x80) != 0) {
        phrase.length = (jsize)(size - 2);
        for (size_t i = 2; i < size; ++i) phrase.data.chars[i - 2] = (jchar)data[i];
        if (size > 2 && (data[0] & 0x40) != 0) phrase.data.chars[0] |= 0x100;
    }
    if ((data[0] & 0x20) != 0) {
        entropy.length = (jsize)((size - 2) % 34);
        for (size_t i = 0; i < (size_t)entropy.length; ++i) entropy.data.bytes[i] = data[2 + i];
    }
    const bool accepted = run(operation);
    if (accepted) verify_accepted(operation);
    return 0;
}
#endif
