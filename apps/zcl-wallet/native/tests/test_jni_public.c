/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_qr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Public JNI check failed at %d\n", __LINE__); abort(); } } while (0)
#define QR Java_org_zclassic_wallet_core_NativeCore_receiveQr
#define PAYMENT Java_org_zclassic_wallet_core_NativeCore_parsePayment
JNIEXPORT jbyteArray JNICALL QR(JNIEnv *, jclass, jbyteArray, jint);
JNIEXPORT jbyteArray JNICALL PAYMENT(JNIEnv *, jclass, jbyteArray, jint);

/* One fixed fake return reference. This tests projection and VM refusal;
 * real-VM reference handling and independent QR decoding have device fixtures.
 * All mutable fixture state is confined to this single-threaded host process. */
typedef struct { jsize length; uint8_t bytes[1682]; } fake_array;
static struct { uint64_t before; fake_array value; uint64_t after; } result_box;
static fake_array source;
static uint8_t expected_modules[ZCL_RECEIVE_QR_MODULES_MAX];
static size_t expected_side;
static zcl_payment_request expected_payment;
static bool pending;
static unsigned fault, calls;

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input == (jarray)&source && !pending);
    ++calls;
    if (fault == 1) { pending = true; return 0; }
    return source.length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    CHECK(input == (jbyteArray)&source && !pending && output != NULL);
    CHECK(offset == 0 && count >= 0 && count <= 1024 && count == source.length);
    ++calls;
    if (fault == 2) {
        if (count != 0) output[0] = 42; // Partial VM copy before throwing.
        pending = true;
        return;
    }
    memcpy(output, source.bytes, (size_t)count);
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize count)
{
    (void)env;
    CHECK(!pending && count > 0 && (size_t)count <= sizeof(result_box.value.bytes));
    ++calls;
    if (fault == 3 || fault == 5) { pending = fault == 3; return NULL; }
    result_box.value.length = count;
    if (fault == 6) pending = true; // A reference does not override an exception.
    return (jbyteArray)&result_box.value;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, const jbyte *input)
{
    (void)env;
    CHECK(array == (jbyteArray)&result_box.value && !pending && input != NULL);
    CHECK(offset == 0 && count > 0 && count == result_box.value.length);
    CHECK((size_t)count <= sizeof(result_box.value.bytes));
    ++calls;
    if (fault == 4) {
        result_box.value.bytes[0] = (uint8_t)input[0];
        pending = true;
        return;
    }
    memcpy(result_box.value.bytes, input, (size_t)count);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes
};
static JNIEnv vm = &table;

static jbyteArray invoke(unsigned operation, JNIEnv *env, jbyteArray input, jint network)
{
    CHECK(operation < 2);
    return operation == 0 ? QR(env, NULL, input, network) : PAYMENT(env, NULL, input, network);
}

/* The C result is the projection oracle, not an independent parser/QR oracle. */
static bool reference(unsigned operation, jint network)
{
    if (source.length < 0 || source.length > 1024 || network < 0 || network > 1) return false;
    const size_t length = (size_t)source.length;
    if (operation == 0)
        return zcl_receive_qr(source.bytes, length, (zcl_network)network, expected_modules,
            sizeof(expected_modules), &expected_side) == ZCL_OK;
    return zcl_payment_parse(source.bytes, length, (zcl_network)network, &expected_payment) == ZCL_OK;
}

static uint64_t read_amount(const uint8_t *bytes)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}

static size_t check_payment(void)
{
    const uint8_t *bytes = result_box.value.bytes;
    const zcl_payment_request *request = &expected_payment;
    CHECK(request->label_len <= 200 && request->message_len <= 200);
    const size_t length = 49 + request->label_len + request->message_len;
    CHECK(result_box.value.length == (jsize)length && bytes[0] == 1 && bytes[1] <= 7);
    CHECK(((bytes[1] & 1) != 0) == request->has_amount);
    CHECK(((bytes[1] & 2) != 0) == request->has_label);
    CHECK(((bytes[1] & 4) != 0) == request->has_message);
    CHECK(memcmp(bytes + 2, request->address_text, 35) == 0);
    CHECK(read_amount(bytes + 37) == request->amount);
    CHECK(bytes[45] == request->label_len && bytes[46] == 0);
    CHECK(bytes[47] == request->message_len && bytes[48] == 0);
    CHECK(memcmp(bytes + 49, request->label, request->label_len) == 0);
    CHECK(memcmp(bytes + 49 + request->label_len, request->message, request->message_len) == 0);
    return length;
}

static void check_result(unsigned operation)
{
    size_t length;
    if (operation == 0) {
        CHECK(expected_side > 0 && expected_side <= 41);
        length = 1 + expected_side * expected_side;
        CHECK(result_box.value.length == (jsize)length);
        CHECK(result_box.value.bytes[0] == expected_side);
        CHECK(memcmp(result_box.value.bytes + 1, expected_modules, length - 1) == 0);
    } else length = check_payment();
    for (size_t i = length; i < sizeof(result_box.value.bytes); ++i)
        CHECK(result_box.value.bytes[i] == 0xa5);
}

static void exercise(unsigned operation, jint network, unsigned selected)
{
    CHECK(selected <= 7);
    fake_array original;
    memcpy(&original, &source, sizeof(original));
    const bool valid = reference(operation, network);
    memset(&result_box, 0xa5, sizeof(result_box));
    fault = selected;
    calls = 0;
    pending = selected == 7;
    const jbyteArray result = invoke(operation, &vm, (jbyteArray)&source, network);
    CHECK(memcmp(&original, &source, sizeof(original)) == 0);
    CHECK(result_box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && result_box.after == result_box.before);
    if (valid) {
        const unsigned expected_calls[] = {4, 1, 2, 3, 4, 3, 3, 0};
        CHECK(calls == expected_calls[selected]);
        CHECK(pending == (selected != 0 && selected != 5));
    }
    if (!valid || selected != 0) CHECK(result == NULL);
    else { CHECK(result == (jbyteArray)&result_box.value); check_result(operation); }
    if (selected == 7) CHECK(calls == 0 && pending);
    pending = false;
}

static void load(const char *text)
{
    const size_t length = strlen(text); // Fixed public fixture literals only.
    CHECK(length <= 1024);
    memset(&source, 0, sizeof(source));
    memcpy(source.bytes, text, length);
    source.length = (jsize)length;
}

static void valid_cases(void)
{
    static const char *const addresses[] = {
        "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF", "t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ",
        "tmHMBeeYRuc2eVicLNfP15YLxbQsooCA6jb", "t2Fbo6DBKKVYw1SfrY8bEgz56hYEhywhEN6"
    };
    for (unsigned row = 0; row < 4; ++row) {
        load(addresses[row]);
        for (unsigned operation = 0; operation < 2; ++operation) {
            CHECK(reference(operation, (jint)(row / 2)));
            for (unsigned selected = 0; selected <= 7; ++selected)
                exercise(operation, (jint)(row / 2), selected);
            exercise(operation, (jint)(1 - row / 2), 0);
        }
    }
    load("zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=Caf%C3%A9&message=A%20B");
    CHECK(reference(1, 0));
    for (unsigned selected = 0; selected <= 7; ++selected) exercise(1, 0, selected);
    load("zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=21000000&label=");
    size_t length = (size_t)source.length;
    memset(source.bytes + length, 'L', 200);
    length += 200;
    memcpy(source.bytes + length, "&message=", 9);
    length += 9;
    memset(source.bytes + length, 'm', 200);
    source.length = (jsize)(length + 200);
    CHECK(reference(1, 0));
    for (unsigned selected = 0; selected <= 7; ++selected) exercise(1, 0, selected);
    CHECK(expected_payment.label_len == 200 && expected_payment.message_len == 200);
}

static void invalid_cases(void)
{
    const jsize lengths[] = {-1, 0, 1, 34, 36, 1024, 1025, INT32_MAX};
    for (unsigned operation = 0; operation < 2; ++operation) {
        load("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF");
        CHECK(invoke(operation, NULL, (jbyteArray)&source, 0) == NULL);
        CHECK(invoke(operation, &vm, NULL, 0) == NULL);
        exercise(operation, -1, 0);
        exercise(operation, 2, 0);
        exercise(operation, INT32_MAX, 0);
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            source.length = lengths[i];
            exercise(operation, 0, 0);
        }
        source.length = 35;
        for (size_t i = 0; i < 35; ++i) {
            const uint8_t old = source.bytes[i];
            source.bytes[i] = 0;
            exercise(operation, 0, 0);
            source.bytes[i] = old;
        }
    }
}

static void payment_record_refusals(void)
{
    zcl_payment_request inconsistent = {0};
    calls = 0;
    pending = false;
    CHECK(zcl_jni_payment_record(&vm, NULL) == NULL);
    CHECK(calls == 0 && !pending);
    inconsistent.label_len = 1;
    CHECK(zcl_jni_payment_record(&vm, &inconsistent) == NULL);
    CHECK(calls == 0 && !pending);
    inconsistent.label_len = 0;
    inconsistent.message_len = 1;
    CHECK(zcl_jni_payment_record(&vm, &inconsistent) == NULL);
    CHECK(calls == 0 && !pending);
}

#ifndef ZCL_JNI_PUBLIC_FUZZ
int main(void)
{
    valid_cases(); invalid_cases(); payment_record_refusals();
    puts("Public QR/payment JNI projection, bounds and VM fault checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4 || size > 1029) return 0;
    static bool tested;
    if (!tested) { valid_cases(); invalid_cases(); tested = true; }
    memset(&source, 0, sizeof(source));
    source.length = (jsize)(size - 4);
    memcpy(source.bytes, data + 4, size - 4);
    if (data[3] == 1) source.length = -1;
    if (data[3] == 2) source.length = INT32_MAX;
    const unsigned operation = data[0] % 2;
    const jint network = (jint)(data[1] % 4) - 1;
    exercise(operation, network, data[2] % 8);
    exercise(operation, network, 0);
    return 0;
}
#endif
