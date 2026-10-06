/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#include "jni_support.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI address check failed at %d\n", __LINE__); abort(); } } while (0)
#define API Java_org_zclassic_wallet_core_NativeCore_encodeAddress
JNIEXPORT jbyteArray JNICALL API(JNIEnv *, jclass, jbyteArray, jint);
JNIEXPORT jbyteArray JNICALL Java_org_zclassic_wallet_core_NativeCore_encodeBase58(
    JNIEnv *, jclass, jbyteArray);
JNIEXPORT jbyteArray JNICALL Java_org_zclassic_wallet_core_NativeCore_decodeBase58(
    JNIEnv *, jclass, jbyteArray);

/* One bounded fake result reference per call, never retained by production.
 * Real JVM/device tests separately qualify actual VM reference lifetimes. */
typedef struct { jsize length; uint8_t bytes[184]; } fake_array;
static struct { uint64_t before; fake_array value; uint64_t after; } result_box;
static bool pending;
static unsigned fault, active, clears_128, clears_184;

void zcl_jni_address_test_zero(void *pointer, size_t length);
void zcl_jni_address_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL && (length == 128 || length == 184));
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if (length == 128) ++clears_128;
    else ++clears_184;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input != NULL && !pending);
    if (fault == 1) { pending = true; return 0; }
    return ((const fake_array *)input)->length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    const fake_array *array = (const fake_array *)input;
    CHECK(array != NULL && !pending && offset == 0 && count >= 0 && count <= 184);
    CHECK(count == array->length && count <= 184 && output != NULL);
    if (fault == 2) {
        if (count > 0) output[0] = 42;
        pending = true;
        return;
    }
    memcpy(output, array->bytes, (size_t)count);
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize count)
{
    (void)env;
    CHECK(!pending && count > 0 && count <= 184);
    if (active == 1) CHECK(clears_128 == 1 && clears_184 == 0);
    if (active == 2) CHECK(clears_128 == 0 && clears_184 == 1);
    if (fault == 3 || fault == 5) { pending = fault == 3; return NULL; }
    result_box.value.length = count;
    return (jbyteArray)&result_box.value;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *data)
{
    (void)env;
    CHECK(input == (jbyteArray)&result_box.value && !pending && offset == 0 && count > 0 && count <= 184);
    CHECK(result_box.value.length == count && data != NULL);
    memcpy(result_box.value.bytes, data, (size_t)count);
    if (fault == 4) pending = true;
}

static const struct JNINativeInterface_ vm_table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes
};
static JNIEnv vm = &vm_table;

static void exercise(fake_array *input, jint network, unsigned selected_fault)
{
    fake_array original;
    memcpy(&original, input, sizeof(original));
    memset(&result_box, 0xa5, sizeof(result_box));
    fault = selected_fault;
    pending = fault == 6;
    const jbyteArray result = API(&vm, NULL, (jbyteArray)input, network);
    CHECK(memcmp(&original, input, sizeof(original)) == 0);
    CHECK(result_box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && result_box.after == result_box.before);
    const bool valid = input->length == 21 && (input->bytes[0] == 1 || input->bytes[0] == 2)
        && (network == 0 || network == 1) && fault == 0;
    if (!valid) { CHECK(result == NULL); pending = false; return; }
    CHECK(result == (jbyteArray)&result_box.value && !pending && result_box.value.length == 35);
    zcl_address address;
    CHECK(zcl_address_parse(result_box.value.bytes, 35, (zcl_network)network, &address) == ZCL_OK);
    CHECK((uint8_t)address.kind == input->bytes[0] && memcmp(address.hash, input->bytes + 1, 20) == 0);
    for (size_t i = 35; i < sizeof(result_box.value.bytes); ++i) CHECK(result_box.value.bytes[i] == 0xa5);
}

static void regressions(void)
{
    fake_array input = {21, {0}};
    for (jint network = 0; network <= 1; ++network) {
        for (unsigned kind = 1; kind <= 2; ++kind) {
            input.bytes[0] = (uint8_t)kind;
            for (unsigned selected = 0; selected <= 6; ++selected) exercise(&input, network, selected);
        }
    }
    for (unsigned kind = 0; kind <= UINT8_MAX; ++kind) {
        input.bytes[0] = (uint8_t)kind;
        exercise(&input, 0, 0);
    }
    input.bytes[0] = 1;
    const jsize lengths[] = {-1, 0, 1, 20, 21, 22, 64, INT32_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        input.length = lengths[i];
        exercise(&input, 0, 0);
    }
    input.length = 21;
    exercise(&input, -1, 0);
    exercise(&input, 2, 0);
    exercise(&input, INT32_MAX, 0);
    CHECK(API(NULL, NULL, (jbyteArray)&input, 0) == NULL);
    CHECK(API(&vm, NULL, NULL, 0) == NULL);
}

static void base58_retirement(void)
{
    fake_array payload = {78, {0}}, encoded = {0};
    for (size_t i = 0; i < 78; ++i) payload.bytes[i] = (uint8_t)(i + 1);
    for (unsigned operation = 1; operation <= 2; ++operation) {
        for (unsigned selected = 0; selected <= 5; ++selected) {
            const fake_array *input = operation == 1 ? &payload : &encoded;
            active = operation; fault = selected; pending = false;
            clears_128 = clears_184 = 0;
            memset(&result_box, 0, sizeof(result_box));
            jbyteArray result = operation == 1
                ? Java_org_zclassic_wallet_core_NativeCore_encodeBase58(&vm, NULL, (jbyteArray)input)
                : Java_org_zclassic_wallet_core_NativeCore_decodeBase58(&vm, NULL, (jbyteArray)input);
            CHECK(clears_128 == 1 && clears_184 == 1);
            CHECK(pending == (selected >= 1 && selected <= 4));
            CHECK(result == (selected == 0 ? (jbyteArray)&result_box.value : NULL));
            if (operation == 1 && selected == 0) encoded = result_box.value;
            if (operation == 2 && selected == 0)
                CHECK(result_box.value.length == 78 &&
                    memcmp(result_box.value.bytes, payload.bytes, 78) == 0);
        }
    }
    active = 0;
}

#ifndef ZCL_JNI_ADDRESS_FUZZ
int main(void)
{
    regressions();
    base58_retirement();
    puts("JNI address record, region and exception checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 64) return 0;
    static bool tested;
    if (!tested) { regressions(); tested = true; }
    fake_array input = {(jsize)size, {0}};
    if (size != 0) memcpy(input.bytes, data, size);
    exercise(&input, (jint)(input.bytes[0] % 4), input.bytes[1] % 7);
    input.length = 21;
    input.bytes[0] = (uint8_t)(1 + input.bytes[0] % 2);
    exercise(&input, (jint)(input.bytes[1] % 2), 0);
    exercise(&input, (jint)(input.bytes[1] % 2), 1 + input.bytes[2] % 6);
    return 0;
}
#endif
