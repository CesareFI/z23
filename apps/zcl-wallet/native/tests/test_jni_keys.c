/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_random_bytes
#undef zcl_mnemonic_decode
#include "jni_support.h"
#include "zcl_keys.h"
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI key check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jint JNICALL API(createEntropy)(JNIEnv *, jclass, jbyteArray);
JNIEXPORT jint JNICALL API(recoveryPhrase)(JNIEnv *, jclass, jbyteArray, jcharArray);
JNIEXPORT jint JNICALL API(restoreEntropy)(JNIEnv *, jclass, jcharArray, jbyteArray);
JNIEXPORT jboolean JNICALL API(confirmRecoveryPhrase)(JNIEnv *, jclass, jbyteArray, jcharArray);
JNIEXPORT jbyteArray JNICALL API(receivingAddress)(JNIEnv *, jclass, jbyteArray, jint, jint);
JNIEXPORT jbyteArray JNICALL API(createWalletHeader)(JNIEnv *, jclass, jbyteArray, jint);
JNIEXPORT jbyteArray JNICALL API(recoveredWalletAddress)(JNIEnv *, jclass, jbyteArray, jbyteArray);

typedef enum { BYTES, CHARS } array_kind;
typedef struct {
    array_kind kind;
    jsize length;
    union { uint8_t bytes[216]; jchar chars[216]; } data;
} fake_array;
/* Missing-wipe mutations can outlive the touched stack buffer. Retain only
 * integer identity; inspect bytes solely through a live zeroizer argument. */
typedef struct { uintptr_t identity; size_t length; bool cleared; } touched_span;
static fake_array entropy, phrase, header, result_array;
static uint8_t known_header[80];
static bool header_ready;
static touched_span touched[8];
static size_t touched_count;
static bool pending, fail_random, null_without_exception, array_with_exception;
static unsigned fail_call, vm_calls, random_calls, entropy_reads, active_operation;
static size_t transfer_prefix = SIZE_MAX;
static size_t transferred_count = SIZE_MAX;
#define OPERATION_COUNT 7u
static const char known_phrase[] = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
static void verify_cleanup(void);
zcl_status zcl_jni_key_test_decode(const uint8_t *text, size_t text_len,
    uint8_t *output, size_t capacity, size_t *length);

static void track(const void *pointer, size_t length)
{
    if (length == 0) return;
    CHECK(pointer != NULL);
    const uintptr_t identity = (uintptr_t)pointer;
    for (size_t i = 0; i < touched_count; ++i) {
        if (touched[i].identity != identity) continue;
        CHECK(!touched[i].cleared);
        if (touched[i].length < length) touched[i].length = length;
        return;
    }
    CHECK(touched_count < sizeof(touched) / sizeof(touched[0]));
    touched[touched_count++] = (touched_span){identity, length, false};
}

zcl_status zcl_jni_key_test_decode(const uint8_t *text, size_t text_len,
    uint8_t *output, size_t capacity, size_t *length)
{
    /* This target substitutes only the JNI call. Its byte-text scratch owns
     * all 215 bytes; the real decoder and published vectors stay unchanged. */
    CHECK(active_operation == 2 && text_len <= 215);
    track(text, 215);
    return zcl_mnemonic_decode(text, text_len, output, capacity, length);
}

void zcl_jni_key_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL && length <= 430);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    const uintptr_t identity = (uintptr_t)pointer;
    for (size_t i = 0; i < touched_count; ++i) {
        if (touched[i].identity != identity) continue;
        CHECK(length >= touched[i].length);
        touched[i].cleared = true;
        touched[i].identity = (uintptr_t)NULL;
        touched[i].length = 0;
    }
}

zcl_status zcl_jni_key_test_random(uint8_t *output, size_t length)
{
    CHECK(output != NULL && length == 32 && !pending);
    ++random_calls;
    track(output, length);
    output[0] = 0x42; /* Even a failed provider may have touched its output. */
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
    /* Header bytes are public. Every entropy destination in these adapters
     * owns 32 bytes, including the tail beyond a shorter supported input. */
    if (array != &header) {
        CHECK(length <= 32);
        track(output, 32);
        if (array == &entropy) ++entropy_reads;
    }
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
    CHECK(active_operation >= 4); /* Secret results must be caller-owned before entry. */
    CHECK(length >= 0 && length <= 215);
    /* Public results need no secret scratch across a VM allocation, including
     * one that fails or returns an array with a pending exception. Both the
     * entropy copy and RNG blinding must already have been fully erased. */
    CHECK(touched_count == 2);
    verify_cleanup();
    if (vm_fault()) {
        if (null_without_exception) pending = false;
        if (!array_with_exception) return NULL;
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
    if (active_operation == 2) {
        CHECK(touched_count == 2); /* UTF-16 read scratch and consumed byte text. */
        verify_cleanup();
    }
    /* These two entries publish only public metadata/address bytes. Keep all
     * existing key-entry scratch obligations, including their public output. */
    if (active_operation < 5) track(bytes, (size_t)length);
    size_t copied = (size_t)length;
    if (vm_calls + 1 == fail_call && copied > transfer_prefix) copied = transfer_prefix;
    memcpy(array->data.bytes + (size_t)start, bytes, copied);
    transferred_count = copied;
    (void)vm_fault();
}

static void JNICALL set_chars(JNIEnv *env, jcharArray input, jsize start, jsize length, const jchar *chars)
{
    (void)env;
    fake_array *array = region(input, start, length, CHARS);
    CHECK(active_operation == 1 && touched_count == 1);
    verify_cleanup(); /* Consumed input entropy must precede the VM transfer. */
    track(chars, (size_t)length * sizeof(*chars));
    size_t copied = (size_t)length;
    if (vm_calls + 1 == fail_call && copied > transfer_prefix) copied = transfer_prefix;
    memcpy(array->data.chars + (size_t)start, chars, copied * sizeof(*chars));
    transferred_count = copied;
    (void)vm_fault();
}

#if defined(__ANDROID__)
typedef struct JNINativeInterface key_jni_interface;
#else
typedef struct JNINativeInterface_ key_jni_interface;
#endif
static const key_jni_interface table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .GetCharArrayRegion = get_chars,
    .NewByteArray = new_bytes, .NewCharArray = new_chars,
    .SetByteArrayRegion = set_bytes, .SetCharArrayRegion = set_chars
};
static JNIEnv environment = &table;

static void prepare_header(void)
{
    if (!header_ready) {
        const uint8_t zero_entropy[16] = {0}, blinding[32] = {1};
        CHECK(zcl_wallet_header_create(zero_entropy, sizeof(zero_entropy), ZCL_MAINNET,
            blinding, sizeof(blinding), known_header, sizeof(known_header)) == ZCL_OK);
        header_ready = true;
    }
    header = (fake_array){.kind = BYTES, .length = (jsize)sizeof(known_header)};
    memcpy(header.data.bytes, known_header, sizeof(known_header));
}

static void prepare(void)
{
    touched_count = 0;
    memset(touched, 0, sizeof(touched));
    pending = fail_random = null_without_exception = array_with_exception = false;
    fail_call = vm_calls = random_calls = entropy_reads = 0;
    transfer_prefix = SIZE_MAX;
    transferred_count = SIZE_MAX;
    active_operation = 0;
    entropy = (fake_array){.kind = BYTES, .length = 16};
    phrase = (fake_array){.kind = CHARS, .length = (jsize)(sizeof(known_phrase) - 1)};
    for (size_t i = 0; i < sizeof(known_phrase) - 1; ++i) phrase.data.chars[i] = (jchar)known_phrase[i];
    memset(&result_array, 0, sizeof(result_array));
    prepare_header();
}

static void verify_cleanup(void)
{
    for (size_t i = 0; i < touched_count; ++i) CHECK(touched[i].cleared);
}

static jint secret_to(unsigned operation, JNIEnv *env, jbyteArray bytes, jcharArray chars, jarray output)
{
    if (operation == 0) return API(createEntropy)(env, NULL, (jbyteArray)output);
    if (operation == 1) return API(recoveryPhrase)(env, NULL, bytes, (jcharArray)output);
    return API(restoreEntropy)(env, NULL, chars, (jbyteArray)output);
}

static bool secret_entry(unsigned operation, JNIEnv *env, jbyteArray bytes, jcharArray chars)
{
    CHECK(operation <= 2);
    result_array = (fake_array){.kind = operation == 1 ? CHARS : BYTES,
        .length = operation == 1 ? 215 : 32};
    const jint written = secret_to(operation, env, bytes, chars, (jarray)&result_array);
    CHECK(written >= 0 && written <= result_array.length);
    if (written == 0) return false;
    /* Model the managed caller's bounded result view, retaining full backing. */
    result_array.length = written;
    return true;
}

/* Confirmation is the only boolean entry; translate its result for the common
 * cleanup/fault runner without manufacturing an array reference. */
static bool invoke(unsigned operation, JNIEnv *env, jbyteArray bytes, jcharArray chars, jint network, jint index)
{
    CHECK(operation < OPERATION_COUNT);
    active_operation = operation;
    switch (operation) {
    case 0: case 1: case 2: return secret_entry(operation, env, bytes, chars);
    case 3: return API(confirmRecoveryPhrase)(env, NULL, bytes, chars) == JNI_TRUE;
    case 4: return API(receivingAddress)(env, NULL, bytes, network, index) != NULL;
    case 5: return API(createWalletHeader)(env, NULL, bytes, network) != NULL;
    default: return API(recoveredWalletAddress)(env, NULL, (jbyteArray)&header, bytes) != NULL;
    }
}

static bool run(unsigned operation)
{
    fake_array inputs[3];
    memcpy(&inputs[0], &entropy, sizeof(entropy));
    memcpy(&inputs[1], &phrase, sizeof(phrase));
    memcpy(&inputs[2], &header, sizeof(header));
    const bool accepted = invoke(operation, &environment, (jbyteArray)&entropy, (jcharArray)&phrase, 0, 0);
    verify_cleanup();
    CHECK(memcmp(&inputs[0], &entropy, sizeof(entropy)) == 0);
    CHECK(memcmp(&inputs[1], &phrase, sizeof(phrase)) == 0);
    CHECK(memcmp(&inputs[2], &header, sizeof(header)) == 0);
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
    for (unsigned operation = 0; operation < OPERATION_COUNT; ++operation) {
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
        CHECK(vm_calls == (operation == 6 ? 2u : 0u) && random_calls == 0);
    }
    prepare();
    CHECK(API(recoveredWalletAddress)(&environment, NULL, NULL, (jbyteArray)&entropy) == NULL);
    verify_cleanup(); CHECK(vm_calls == 0 && random_calls == 0);
}

static void check_receive_result(void)
{
    uint8_t expected[35], blinding[32] = {1};
    size_t length = 0;
    CHECK(zcl_receive_from_entropy(entropy.data.bytes, 16, ZCL_MAINNET, 0,
        blinding, sizeof(blinding), expected, sizeof(expected), &length) == ZCL_OK);
    CHECK(result_array.kind == BYTES && result_array.length == 35);
    CHECK(length == 35 && memcmp(result_array.data.bytes, expected, 35) == 0);
}

static void check_exact_result(unsigned operation)
{
    if (operation == 0) {
        CHECK(result_array.kind == BYTES && result_array.length == 32 && random_calls == 1);
        for (size_t i = 0; i < 32; ++i) CHECK(result_array.data.bytes[i] == 0x42);
    } else if (operation == 1) {
        CHECK(result_array.kind == CHARS && result_array.length == phrase.length);
        CHECK(memcmp(result_array.data.chars, phrase.data.chars, (size_t)phrase.length * sizeof(jchar)) == 0);
    } else if (operation == 2) {
        CHECK(result_array.kind == BYTES && result_array.length == 16);
        for (size_t i = 0; i < 16; ++i) CHECK(result_array.data.bytes[i] == 0);
    } else if (operation == 4 || operation == 6) {
        check_receive_result();
    } else if (operation == 5) {
        CHECK(result_array.kind == BYTES && result_array.length == 80);
        CHECK(memcmp(result_array.data.bytes, known_header, sizeof(known_header)) == 0);
    }
}

static void exact_results(void)
{
    for (unsigned operation = 0; operation < OPERATION_COUNT; ++operation) {
        prepare(); CHECK(run(operation));
        check_exact_result(operation);
        CHECK(entropy.length == 16 && phrase.length == (jsize)(sizeof(known_phrase) - 1));
        for (size_t i = 0; i < sizeof(entropy.data.bytes); ++i) CHECK(entropy.data.bytes[i] == 0);
        CHECK(header.length == 80 && memcmp(header.data.bytes, known_header, sizeof(known_header)) == 0);
    }
}

static void public_result(unsigned operation, zcl_network network, size_t entropy_len)
{
    prepare();
    entropy.length = (jsize)entropy_len;
    for (size_t i = 0; i < entropy_len; ++i) entropy.data.bytes[i] = (uint8_t)(i + 1);
    const fake_array before = entropy;
    const uint8_t blinding[32] = {1};
    uint8_t expected[80];
    CHECK(zcl_wallet_header_create(entropy.data.bytes, entropy_len, network,
        blinding, sizeof(blinding), expected, sizeof(expected)) == ZCL_OK);
    memcpy(header.data.bytes, expected, sizeof(expected));
    CHECK(invoke(operation, &environment, (jbyteArray)&entropy, NULL, (jint)network, 0));
    verify_cleanup();
    CHECK(memcmp(&entropy, &before, sizeof(entropy)) == 0);
    CHECK(memcmp(header.data.bytes, expected, sizeof(expected)) == 0);
    const jsize length = operation == 5 ? 80 : 35;
    const size_t offset = operation == 5 ? 0 : 44;
    CHECK(result_array.kind == BYTES && result_array.length == length);
    CHECK(memcmp(result_array.data.bytes, expected + offset, (size_t)length) == 0);
}

static void public_results(void)
{
    const zcl_network networks[] = {ZCL_MAINNET, ZCL_TESTNET};
    const size_t lengths[] = {16, 20, 24, 28, 32};
    for (size_t n = 0; n < sizeof(networks) / sizeof(networks[0]); ++n)
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
            for (unsigned operation = 4; operation < OPERATION_COUNT; ++operation)
                public_result(operation, networks[n], lengths[i]);
}

static void vm_failures(void)
{
    const unsigned calls[] = {2, 4, 4, 4, 4, 4, 6};
    for (unsigned operation = 0; operation < OPERATION_COUNT; ++operation) {
        for (unsigned failure = 1; failure <= calls[operation]; ++failure) {
            prepare(); fail_call = failure;
            CHECK(!run(operation) && pending && vm_calls == failure);
        }
        if (operation <= 3) continue; /* No VM allocation in secret/boolean entries. */
        prepare(); fail_call = operation == 0 ? 1 : (operation == 6 ? 5 : 3);
        null_without_exception = true;
        CHECK(!run(operation) && !pending);
    }
    const unsigned random_operations[] = {0, 4, 5, 6};
    for (size_t i = 0; i < sizeof(random_operations) / sizeof(random_operations[0]); ++i) {
        prepare(); fail_random = true;
        CHECK(!run(random_operations[i]) && random_calls == 1 && !pending);
    }
}

static void allocation_with_exception(void)
{
    const unsigned allocation_calls[] = {0, 0, 0, 0, 3, 3, 5};
    for (unsigned operation = 0; operation < OPERATION_COUNT; ++operation) {
        if (allocation_calls[operation] == 0) continue; /* Boolean confirmation has no allocation. */
        prepare();
        fail_call = allocation_calls[operation];
        array_with_exception = true;
        CHECK(!run(operation));
        CHECK(pending && vm_calls == fail_call);
        const uint8_t *bytes = (const uint8_t *)&result_array.data;
        for (size_t i = 0; i < sizeof(result_array.data); ++i) CHECK(bytes[i] == 0);
    }
}

static void invalid_inputs(void)
{
    const jsize invalid[] = {-1, 0, 33, 216, INT32_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (unsigned operation = 1; operation < OPERATION_COUNT; ++operation) {
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
    for (unsigned operation = 4; operation <= 5; ++operation) {
        prepare();
        CHECK(!invoke(operation, &environment, (jbyteArray)&entropy, NULL, 2, 0));
        verify_cleanup(); CHECK(vm_calls == 0 && random_calls == 0);
    }

}

static void invalid_entropy_profiles(void)
{
    const jsize invalid[] = {1, 15, 17, 31};
    for (unsigned operation = 4; operation <= 6; ++operation) {
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            prepare(); entropy.length = invalid[i];
            CHECK(!run(operation) && !pending && entropy_reads == 1 && random_calls == 0);
        }
    }
}

static void invalid_headers(void)
{
    const jsize lengths[] = {-1, 0, 79, 81, 216, INT32_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        prepare(); header.length = lengths[i];
        CHECK(!run(6) && !pending);
    }
    const size_t offsets[] = {0, 8, 9, 10, 11, 12, 44, 79};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        prepare(); header.data.bytes[offsets[i]] ^= 0xff;
        CHECK(!run(6) && !pending && vm_calls == 2 &&
            entropy_reads == 0 && random_calls == 0);
    }
    prepare(); entropy.data.bytes[0] = 1;
    CHECK(!run(6) && random_calls == 1 && !pending);
}

static void full_phrase_inputs(void)
{
    const jchar endings[] = {'a', 0x1234};
    for (unsigned operation = 2; operation <= 3; ++operation) {
        for (size_t ending = 0; ending < sizeof(endings) / sizeof(endings[0]); ++ending) {
            prepare();
            phrase.length = 215;
            for (size_t i = 0; i < 215; ++i) phrase.data.chars[i] = 'a';
            phrase.data.chars[214] = endings[ending];
            /* Both are malformed, but JNI first copies all 430 bytes. Cover
             * full scratch erasure after decoding and after ASCII refusal. */
            CHECK(!run(operation) && !pending);
        }
    }
}

static void destination_refusals(void)
{
    const jsize lengths[] = {INT32_MIN, -1, 0, 1, 31, 33, 214, 216, INT32_MAX};
    for (unsigned operation = 0; operation <= 2; ++operation) {
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            prepare(); active_operation = operation;
            result_array.kind = operation == 1 ? CHARS : BYTES;
            result_array.length = lengths[i];
            memset(&result_array.data, 0xa5, sizeof(result_array.data));
            const fake_array before = result_array;
            CHECK(secret_to(operation, &environment, (jbyteArray)&entropy, (jcharArray)&phrase,
                (jarray)&result_array) == 0);
            CHECK(!pending && random_calls == 0 && memcmp(&before, &result_array, sizeof(before)) == 0);
            CHECK(touched_count == 0); /* Refuse unusable output before copying any secret input. */
            verify_cleanup();
        }
        prepare(); active_operation = operation;
        CHECK(secret_to(operation, &environment, (jbyteArray)&entropy, (jcharArray)&phrase, NULL) == 0);
        CHECK(!pending && random_calls == 0);
        CHECK(touched_count == 0);
        verify_cleanup();
    }
}

static void transfer_failures(void)
{
    /* The fake VM copies a selected prefix BEFORE raising its exception.
     * Output remains in this caller-owned object, so managed finally can erase
     * it without a JNI call under the pending exception. JVM tests prove that
     * finally; this fixture proves native cleanup and unchanged VM exception. */
    const size_t prefixes[] = {0, 1, 7, 16, 31, 32, 46, 92, 93, 215};
    for (unsigned operation = 0; operation <= 2; ++operation) {
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
            prepare(); fail_call = operation == 0 ? 2 : 4; transfer_prefix = prefixes[i];
            CHECK(!run(operation) && pending && vm_calls == fail_call);
            CHECK(result_array.length == (operation == 1 ? 215 : 32));
            CHECK(result_array.kind == (operation == 1 ? CHARS : BYTES));
            if (operation == 0 && prefixes[i] > 0) CHECK(result_array.data.bytes[0] == 0x42);
            if (operation == 1 && prefixes[i] > 0) CHECK(result_array.data.chars[0] == 'a');
            zcl_secure_zero(&result_array.data, sizeof(result_array.data));
            CHECK(pending && vm_calls == fail_call); /* Native did not clear the exception. */
        }
    }
}

#endif

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
    case 5: {
        zcl_wallet_info info;
        CHECK(result_array.kind == BYTES && result_array.length == 80);
        CHECK(zcl_wallet_header_parse(result_array.data.bytes, 80, &info) == ZCL_OK);
        CHECK(info.network == ZCL_MAINNET && info.entropy_len == (size_t)entropy.length);
        break;
    }
    case 6: {
        zcl_wallet_info info;
        CHECK(zcl_wallet_header_parse(header.data.bytes, (size_t)header.length, &info) == ZCL_OK);
        CHECK(info.entropy_len == (size_t)entropy.length);
        CHECK(result_array.kind == BYTES && result_array.length == 35);
        CHECK(memcmp(info.address, result_array.data.bytes, sizeof(info.address)) == 0);
        break;
    }
    default: {
        zcl_address address;
        CHECK(result_array.kind == BYTES && result_array.length == 35);
        CHECK(zcl_address_parse(result_array.data.bytes, 35, ZCL_MAINNET, &address) == ZCL_OK);
        CHECK(address.kind == ZCL_P2PKH);
        break;
    }
    }
}

static bool fuzz_input(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 217) return false;
    prepare();
    const unsigned operation = data[0] % OPERATION_COUNT;
    fail_call = data[1] % 8;
    /* Preserve the existing two-byte format. Its unused flag bit opts into
     * a bounded output prefix; the third byte may also supply input payload. */
    if (size > 2 && (data[1] & 0x08) != 0) transfer_prefix = data[2];
    pending = (data[1] & 0x80) != 0;
    fail_random = (data[1] & 0x40) != 0;
    array_with_exception = (data[1] & 0x10) != 0;
    null_without_exception = (data[1] & 0x20) != 0 && !array_with_exception;
    if ((data[0] & 0x80) != 0) {
        phrase.length = (jsize)(size - 2);
        for (size_t i = 2; i < size; ++i) phrase.data.chars[i - 2] = (jchar)data[i];
        if (size > 2 && (data[0] & 0x40) != 0) phrase.data.chars[0] |= 0x100;
    }
    if ((data[0] & 0x20) != 0) {
        entropy.length = (jsize)((size - 2) % 34);
        for (size_t i = 0; i < (size_t)entropy.length; ++i) entropy.data.bytes[i] = data[2 + i];
    }
    if (operation == 6 && (data[0] & 0x10) != 0) {
        header.length = (jsize)(size - 2);
        memcpy(header.data.bytes, data + 2, size - 2);
    }
    const bool accepted = run(operation);
    if (accepted) verify_accepted(operation);
    return accepted;
}

#ifdef ZCL_JNI_KEYS_FUZZ
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    (void)fuzz_input(data, size);
    return 0;
}
#else
static void fuzz_transfer_case(unsigned operation, uint8_t prefix)
{
    const uint8_t data[] = {(uint8_t)operation, operation == 0 ? 10 : 12, prefix};
    CHECK(!fuzz_input(data, sizeof(data)) && pending);
    CHECK(vm_calls == (operation == 0 ? 2u : 4u));
    const size_t length = operation == 1 ? sizeof(known_phrase) - 1 : (operation == 0 ? 32u : 16u);
    const size_t copied = prefix < length ? prefix : length;
    CHECK(transferred_count == copied);
    for (size_t i = 0; i < 216; ++i) {
        if (operation == 1) {
            const jchar expected = i < copied ? (jchar)known_phrase[i] : 0;
            CHECK(result_array.data.chars[i] == expected);
        } else {
            const uint8_t expected = operation == 0 && i < copied ? 0x42 : 0;
            CHECK(result_array.data.bytes[i] == expected);
        }
    }
    zcl_secure_zero(&result_array.data, sizeof(result_array.data));
}

static void fuzz_transfers(void)
{
    const uint8_t prefixes[] = {0, 1, 7, 16, 31, 32, 46, 92, 93, 215, 255};
    for (unsigned operation = 0; operation <= 2; ++operation)
        for (size_t i = 0; i < sizeof(prefixes); ++i)
            fuzz_transfer_case(operation, prefixes[i]);
}

int main(void)
{
    pending_helpers(); pending_and_null_entries(); exact_results(); public_results(); vm_failures();
    allocation_with_exception(); invalid_inputs(); invalid_entropy_profiles();
    invalid_headers(); full_phrase_inputs();
    destination_refusals(); transfer_failures(); fuzz_transfers();
    puts("JNI key exception and secret cleanup checks passed");
    return 0;
}
#endif
