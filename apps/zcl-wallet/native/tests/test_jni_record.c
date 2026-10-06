/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_wallet_record_pack
#undef zcl_wallet_record_parse
#include "jni_support.h"
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI record check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jbyteArray JNICALL API(packWalletRecord)(JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray);
JNIEXPORT jobjectArray JNICALL API(unpackWalletRecord)(JNIEnv *, jclass, jbyteArray);

typedef enum { BYTES, OBJECTS, BYTE_CLASS } object_kind;
typedef struct fake_object {
    object_kind kind;
    jsize length;
    bool local;
    uint8_t bytes[141];
    struct fake_object *elements[4];
} fake_object;
static fake_object inputs[3], record_input, references[6];
static uint8_t headers[10][80];
static bool headers_ready, pending;
static size_t reference_count, live, peak;
static unsigned vm_calls, fail_call, allocation_fault_mode;
typedef struct { uintptr_t identity; size_t capacity; bool cleared; } native_span;
static native_span spans[4];
static size_t span_count;

static void track(const void *pointer, size_t capacity)
{
    CHECK(pointer != NULL && span_count < sizeof(spans) / sizeof(spans[0]));
    spans[span_count++] = (native_span){(uintptr_t)pointer, capacity, false};
}

void zcl_jni_record_test_zero(void *pointer, size_t length);
void zcl_jni_record_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    for (size_t i = 0; i < span_count; ++i) {
        if (spans[i].identity != (uintptr_t)pointer) continue;
        CHECK(!spans[i].cleared && spans[i].capacity == length);
        spans[i].cleared = true;
        spans[i].identity = 0; /* Never retain a pointer past native scope exit. */
    }
}

zcl_status zcl_jni_record_test_pack(const uint8_t *, size_t, const uint8_t *, size_t,
    const uint8_t *, size_t, uint8_t *, size_t, size_t *);
zcl_status zcl_jni_record_test_pack(const uint8_t *header, size_t header_len,
    const uint8_t *iv, size_t iv_len, const uint8_t *ciphertext, size_t ciphertext_len,
    uint8_t *record, size_t capacity, size_t *length)
{
    track(record, capacity);
    return zcl_wallet_record_pack(header, header_len, iv, iv_len, ciphertext,
        ciphertext_len, record, capacity, length);
}

zcl_status zcl_jni_record_test_parse(const uint8_t *, size_t, zcl_wallet_record *);
zcl_status zcl_jni_record_test_parse(const uint8_t *bytes, size_t length, zcl_wallet_record *record)
{
    track(record, sizeof(*record));
    return zcl_wallet_record_parse(bytes, length, record);
}

static void verify_cleanup(void)
{
    for (size_t i = 0; i < span_count; ++i)
        CHECK(spans[i].cleared && spans[i].identity == 0);
}

static bool vm_fault(void)
{
    CHECK(!pending);
    ++vm_calls;
    if (vm_calls != fail_call) return false;
    pending = true;
    return true;
}

static fake_object *new_reference(object_kind kind, jsize length)
{
    const bool fault = vm_fault();
    if (fault && allocation_fault_mode != 3) {
        if (allocation_fault_mode == 2) pending = false;
        return NULL;
    }
    CHECK(reference_count < sizeof(references) / sizeof(references[0]));
    fake_object *object = &references[reference_count++];
    *object = (fake_object){.kind = kind, .length = length, .local = true};
    ++live;
    if (peak < live) peak = live;
    CHECK(peak <= 2); /* One returned container plus one temporary local. */
    return object;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    const fake_object *object = (fake_object *)input;
    CHECK(object != NULL && object->local && object->kind == BYTES);
    return vm_fault() ? 0 : object->length;
}

static fake_object *region(jbyteArray input, jsize offset, jsize count)
{
    fake_object *object = (fake_object *)input;
    CHECK(object != NULL && object->local && object->kind == BYTES && !pending);
    CHECK(offset >= 0 && count >= 0 && offset <= object->length && count <= object->length - offset);
    CHECK(offset <= 141 && count <= 141 - offset);
    return object;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *bytes)
{
    (void)env;
    const fake_object *object = region(input, offset, count);
    CHECK(bytes != NULL);
    const size_t capacity = object == &record_input ? 140 :
        object == &inputs[0] ? 80 : object == &inputs[1] ? 12 : 48;
    track(bytes, capacity); /* Includes partial VM writes and unused tails. */
    if (vm_fault()) { if (count > 0) bytes[0] = 42; return; }
    memcpy(bytes, object->bytes + (size_t)offset, (size_t)count);
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(length > 0 && length <= 140);
    return (jbyteArray)new_reference(BYTES, length);
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    fake_object *object = region(input, offset, count);
    CHECK(bytes != NULL);
    memcpy(object->bytes + (size_t)offset, bytes, (size_t)count);
    (void)vm_fault(); /* A failed publication may already have copied bytes. */
}

static jclass JNICALL find_class(JNIEnv *env, const char *name)
{
    (void)env;
    CHECK(name != NULL && strcmp(name, "[B") == 0);
    return (jclass)new_reference(BYTE_CLASS, 0);
}

static jobjectArray JNICALL new_objects(JNIEnv *env, jsize length, jclass type, jobject initial)
{
    (void)env;
    const fake_object *object = (fake_object *)type;
    CHECK(object != NULL && object->local && object->kind == BYTE_CLASS);
    CHECK(length == 4 && initial == NULL);
    return (jobjectArray)new_reference(OBJECTS, length);
}

static void JNICALL set_element(JNIEnv *env, jobjectArray input, jsize index, jobject value)
{
    (void)env;
    fake_object *array = (fake_object *)input, *element = (fake_object *)value;
    CHECK(array != NULL && array->local && array->kind == OBJECTS && array->length == 4);
    CHECK(element != NULL && element->local && element->kind == BYTES);
    CHECK(index >= 0 && index < 4);
    CHECK(!pending);
    array->elements[(size_t)index] = element;
    (void)vm_fault();
}

static void JNICALL delete_reference(JNIEnv *env, jobject reference)
{
    (void)env;
    CHECK(reference != NULL && live > 0);
    for (size_t i = 0; i < reference_count; ++i) {
        if ((jobject)&references[i] != reference) continue;
        CHECK(references[i].local);
        references[i].local = false;
        --live;
        return; /* DeleteLocalRef is permitted while an exception is pending. */
    }
    CHECK(false);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .NewByteArray = new_bytes,
    .SetByteArrayRegion = set_bytes, .FindClass = find_class,
    .NewObjectArray = new_objects, .SetObjectArrayElement = set_element,
    .DeleteLocalRef = delete_reference
};
static JNIEnv environment = &table;

static void prepare_headers(void)
{
    if (headers_ready) return;
    const uint8_t entropy[32] = {0}, blinding[32] = {1};
    for (size_t i = 0; i < 10; ++i) {
        const zcl_network network = i < 5 ? ZCL_MAINNET : ZCL_TESTNET;
        CHECK(zcl_wallet_header_create(entropy, 16 + (i % 5) * 4, network,
            blinding, sizeof(blinding), headers[i], sizeof(headers[i])) == ZCL_OK);
    }
    headers_ready = true;
}

static void prepare(unsigned profile)
{
    CHECK(profile < 10);
    prepare_headers();
    reference_count = live = peak = 0;
    span_count = 0;
    memset(spans, 0, sizeof(spans));
    vm_calls = fail_call = allocation_fault_mode = 0;
    pending = false;
    memset(references, 0, sizeof(references));
    inputs[0] = (fake_object){.kind = BYTES, .length = 80, .local = true};
    memcpy(inputs[0].bytes, headers[profile], 80);
    inputs[1] = (fake_object){.kind = BYTES, .length = 12, .local = true};
    memset(inputs[1].bytes, 0x37, 12);
    inputs[2] = (fake_object){.kind = BYTES, .length = (jsize)(32 + (profile % 5) * 4), .local = true};
    memset(inputs[2].bytes, 0xa5, (size_t)inputs[2].length);
    record_input = (fake_object){.kind = BYTES, .local = true};
    size_t length = 0;
    CHECK(zcl_wallet_record_pack(inputs[0].bytes, 80, inputs[1].bytes, 12,
        inputs[2].bytes, (size_t)inputs[2].length, record_input.bytes, 140, &length) == ZCL_OK);
    record_input.length = (jsize)length;
}

static fake_object *invoke(bool unpack, JNIEnv *env)
{
    fake_object *result = unpack
        ? (fake_object *)API(unpackWalletRecord)(env, NULL, (jbyteArray)&record_input)
        : (fake_object *)API(packWalletRecord)(env, NULL,
            (jbyteArray)&inputs[0], (jbyteArray)&inputs[1], (jbyteArray)&inputs[2]);
    verify_cleanup();
    return result;
}

static void check_bytes(const fake_object *object, const uint8_t *bytes, size_t length)
{
    CHECK(object != NULL && object->kind == BYTES && object->length >= 0);
    CHECK((size_t)object->length == length && length <= sizeof(object->bytes));
    CHECK(memcmp(object->bytes, bytes, length) == 0);
}

static void verify_success(bool unpack, const fake_object *result)
{
    CHECK(!pending && result->local && live == 1);
    if (!unpack) {
        uint8_t expected[140];
        size_t length = 0;
        CHECK(zcl_wallet_record_pack(inputs[0].bytes, (size_t)inputs[0].length,
            inputs[1].bytes, (size_t)inputs[1].length, inputs[2].bytes,
            (size_t)inputs[2].length, expected, sizeof(expected), &length) == ZCL_OK);
        check_bytes(result, expected, length);
        CHECK(reference_count == 1 && peak == 1);
        return;
    }
    zcl_wallet_record record;
    CHECK(zcl_wallet_record_parse(record_input.bytes, (size_t)record_input.length, &record) == ZCL_OK);
    CHECK(result->kind == OBJECTS && result->length == 4 && reference_count == 6 && peak == 2);
    check_bytes(result->elements[0], record.header, sizeof(record.header));
    check_bytes(result->elements[1], record.iv, sizeof(record.iv));
    check_bytes(result->elements[2], record.ciphertext, record.ciphertext_len);
    const uint8_t network = (uint8_t)record.info.network;
    check_bytes(result->elements[3], &network, 1);
    for (size_t i = 0; i < 4; ++i) CHECK(!result->elements[i]->local);
}

static bool run(bool unpack)
{
    fake_object before[4];
    memcpy(before, inputs, sizeof(inputs));
    memcpy(&before[3], &record_input, sizeof(record_input));
    const fake_object *result = invoke(unpack, &environment);
    CHECK(memcmp(before, inputs, sizeof(inputs)) == 0);
    CHECK(memcmp(&before[3], &record_input, sizeof(record_input)) == 0);
    if (result != NULL) verify_success(unpack, result);
    CHECK(live <= 2 && reference_count <= 6);
    /* Return releases remaining locals through the real JNI frame. The fixed
     * fake pool models that boundary on the next prepare(), not a C heap free. */
    return result != NULL;
}

#ifndef ZCL_JNI_RECORD_FUZZ
static void exact_results(void)
{
    for (unsigned profile = 0; profile < 10; ++profile) {
        prepare(profile); CHECK(run(false) && vm_calls == 8);
        prepare(profile); CHECK(run(true) && vm_calls == 16);
    }
}

static void vm_failures(void)
{
    for (unsigned operation = 0; operation < 2; ++operation) {
        const bool unpack = operation != 0;
        const unsigned calls = unpack ? 16 : 8;
        for (unsigned failure = 1; failure <= calls; ++failure) {
            prepare(9); fail_call = failure;
            CHECK(!run(unpack) && pending && vm_calls == failure);
        }
    }
    const unsigned allocations[] = {3, 4, 5, 8, 11, 14};
    for (unsigned mode = 2; mode <= 3; ++mode) {
        prepare(0); fail_call = 7; allocation_fault_mode = mode;
        CHECK(!run(false) && pending == (mode == 3) && vm_calls == 7);
        for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); ++i) {
            prepare(0); fail_call = allocations[i]; allocation_fault_mode = mode;
            CHECK(!run(true) && pending == (mode == 3) && vm_calls == allocations[i]);
        }
    }
}

static void pending_and_null(void)
{
    for (unsigned operation = 0; operation < 2; ++operation) {
        prepare(0); pending = true;
        CHECK(!run(operation != 0) && vm_calls == 0 && reference_count == 0 && pending);
        prepare(0);
        CHECK(invoke(operation != 0, NULL) == NULL && vm_calls == 0 && reference_count == 0);
    }
    prepare(0);
    CHECK(API(unpackWalletRecord)(&environment, NULL, NULL) == NULL && vm_calls == 0);
    for (size_t missing = 0; missing < 3; ++missing) {
        prepare(0);
        jbyteArray arguments[3] = {(jbyteArray)&inputs[0], (jbyteArray)&inputs[1], (jbyteArray)&inputs[2]};
        arguments[missing] = NULL;
        CHECK(API(packWalletRecord)(&environment, NULL, arguments[0], arguments[1], arguments[2]) == NULL);
        verify_cleanup();
        CHECK(vm_calls == missing * 2 && reference_count == 0 && !pending);
    }
}

static void invalid_inputs(void)
{
    for (size_t field = 0; field < 4; ++field) {
        prepare(0);
        const fake_object *source = field == 3 ? &record_input : &inputs[field];
        const jsize lengths[] = {-1, 0, source->length - 1, source->length + 1, INT32_MAX};
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            prepare(0);
            fake_object *object = field == 3 ? &record_input : &inputs[field];
            object->length = lengths[i];
            CHECK(!run(field == 3) && !pending && reference_count == 0);
        }
    }
    const size_t offsets[] = {0, 4, 5, 6, 7, 8, 12, 44, 79};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        prepare(0); inputs[0].bytes[offsets[i]] ^= 0xff;
        CHECK(!run(false) && !pending && reference_count == 0 && vm_calls == 2);
        prepare(0); record_input.bytes[offsets[i]] ^= 0xff;
        CHECK(!run(true) && !pending && reference_count == 0);
    }
}

static void invalid_iv_admission(void)
{
    const jsize lengths[] = {0, 1, 11};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        prepare(0); inputs[1].length = lengths[i];
        CHECK(!run(false) && !pending && reference_count == 0 && vm_calls == 4);
    }
}

int main(void)
{
    exact_results(); vm_failures(); pending_and_null(); invalid_inputs(); invalid_iv_admission();
    puts("JNI record reference and exception checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 143) return 0;
    const bool unpack = (data[0] & 1) != 0;
    prepare((data[0] >> 1) % 10);
    fail_call = data[1] % 18;
    pending = (data[1] & 0x80) != 0;
    allocation_fault_mode = (data[1] >> 5) % 4;
    if ((data[0] & 0x80) != 0) {
        fake_object *object = unpack ? &record_input : &inputs[(data[0] >> 5) % 3];
        object->length = (jsize)(size - 2);
        memcpy(object->bytes, data + 2, size - 2);
    }
    (void)run(unpack);
    return 0;
}
#endif
