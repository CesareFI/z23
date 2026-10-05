/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef malloc
#undef free
#undef zcl_secure_zero
#undef zcl_transaction_draft
#include "jni_draft_internal.h"
#include "draft_fixture.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI draft check failed at %d\n", __LINE__); abort(); } } while (0)
#define API Java_org_zclassic_wallet_core_NativeCore_buildDraft
JNIEXPORT jbyteArray JNICALL API(JNIEnv *, jclass, jobjectArray, jobjectArray, jlongArray, jint);

typedef enum { BYTES, NUMBERS, OBJECTS } array_kind;
typedef struct fake_array {
    jsize length;
    array_kind kind;
    union {
        uint8_t bytes[ZCL_TX_WIRE_MAX + 1];
        jlong numbers[ZCL_DRAFT_PARAMETER_MAX + 1];
        struct fake_array *objects[ZCL_TX_OUTPUT_MAX + 1];
    } data;
} fake_array;
typedef struct { fake_array *previous, *destinations, *parameters; jint network; } java_inputs;
static fake_array *references[64];
static size_t reference_count, borrowed;
static void *owned_inputs;
static bool pending, fail_malloc, fail_new, new_without_exception, fail_set, element_with_exception;
static bool new_failed;
static bool new_with_exception;
static unsigned fail_length, fail_element, fail_bytes, fail_longs;
static unsigned length_calls, element_calls, byte_calls, long_calls, new_calls, frees;
static assessment_fixture fixture;
static zcl_draft_request fixture_request;
static fake_array *destination_array;
static uintptr_t numbers_identity, transaction_identity, output_identity;
static unsigned destination_reads, destination_clears, number_clears;
static unsigned transaction_calls, transaction_clears, output_clears;

void zcl_jni_draft_test_zero(void *buffer, size_t length);
zcl_status zcl_jni_draft_test_construct(const zcl_draft_request *request, zcl_transparent_tx *transaction);

zcl_status zcl_jni_draft_test_construct(const zcl_draft_request *request, zcl_transparent_tx *transaction)
{
    CHECK(transaction_identity == 0 && transaction_calls == transaction_clears);
    transaction_identity = (uintptr_t)transaction;
    ++transaction_calls;
    /* Even a provider refusal that leaves dirty output must retire it. */
    memset(transaction, 0xa5, sizeof(*transaction));
    return zcl_transaction_draft(request, transaction);
}

static void scratch_retired(void)
{
    CHECK(numbers_identity == 0 && number_clears == long_calls);
    CHECK(destination_clears == destination_reads);
    CHECK(transaction_identity == 0 && transaction_clears == transaction_calls);
}

void zcl_jni_draft_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if (buffer == owned_inputs) {
        CHECK(length == sizeof(zcl_jni_draft_inputs));
        scratch_retired();
    } else if ((uintptr_t)buffer == transaction_identity) {
        CHECK(length == sizeof(zcl_transparent_tx));
        transaction_identity = 0;
        ++transaction_clears;
    } else if ((uintptr_t)buffer == numbers_identity) {
        CHECK(length == ZCL_DRAFT_PARAMETER_MAX * sizeof(jlong));
        numbers_identity = 0;
        ++number_clears;
    } else if (length == sizeof(zcl_draft_request)) {
        /* Admission scratch is public but must not survive its last use. */
    } else if (length == 35) {
        ++destination_clears;
    } else {
        CHECK(length == ZCL_TX_WIRE_MAX + 1);
        CHECK(output_identity == 0 || output_identity == (uintptr_t)buffer);
        output_identity = 0;
        ++output_clears;
    }
}

void *zcl_jni_draft_test_malloc(size_t size)
{
    CHECK(size == sizeof(zcl_jni_draft_inputs) && owned_inputs == NULL);
    if (fail_malloc) { fail_malloc = false; return NULL; }
    owned_inputs = malloc(size);
    CHECK(owned_inputs != NULL);
    memset(owned_inputs, 0xa5, size);
    return owned_inputs;
}

void zcl_jni_draft_test_free(void *pointer)
{
    CHECK(pointer != NULL && pointer == owned_inputs);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < sizeof(zcl_jni_draft_inputs); ++i) CHECK(bytes[i] == 0);
    owned_inputs = NULL;
    ++frees;
    free(pointer);
}

static bool fails(unsigned *ordinal)
{
    if (*ordinal == 0) return false;
    --*ordinal;
    if (*ordinal != 0) return false;
    pending = true;
    return true;
}

static size_t fake_capacity(array_kind kind)
{
    return kind == BYTES ? ZCL_TX_WIRE_MAX + 1 :
        (kind == NUMBERS ? ZCL_DRAFT_PARAMETER_MAX + 1 : ZCL_TX_OUTPUT_MAX + 1);
}

static fake_array *array_new(jsize length, array_kind kind)
{
    CHECK(length >= 0 && (size_t)length <= fake_capacity(kind));
    CHECK(reference_count < sizeof(references) / sizeof(references[0]));
    fake_array *array = calloc(1, sizeof(*array));
    CHECK(array != NULL);
    array->kind = kind;
    array->length = length;
    references[reference_count++] = array;
    return array;
}

static fake_array *byte_array(const uint8_t *bytes, size_t length)
{
    CHECK(bytes != NULL && length <= ZCL_TX_WIRE_MAX + 1);
    fake_array *array = array_new((jsize)length, BYTES);
    memcpy(array->data.bytes, bytes, length);
    return array;
}

static void release_references(void)
{
    CHECK(owned_inputs == NULL && borrowed == 0);
    scratch_retired();
    CHECK(output_identity == 0);
    destination_array = NULL;
    destination_reads = destination_clears = number_clears = 0;
    transaction_calls = transaction_clears = output_clears = 0;
    for (size_t i = 0; i < reference_count; ++i) { free(references[i]); references[i] = NULL; }
    reference_count = 0;
    pending = fail_malloc = fail_new = new_without_exception = fail_set = element_with_exception = false;
    new_failed = new_with_exception = false;
    fail_length = fail_element = fail_bytes = fail_longs = 0;
    length_calls = element_calls = byte_calls = long_calls = new_calls = frees = 0;
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
    ++length_calls;
    return fails(&fail_length) ? 0 : ((fake_array *)input)->length;
}

static jobject JNICALL get_element(JNIEnv *env, jobjectArray input, jsize index)
{
    (void)env;
    const fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == OBJECTS && !pending);
    CHECK(index >= 0 && index < array->length && (size_t)index < fake_capacity(OBJECTS));
    ++element_calls;
    if (array == destination_array) ++destination_reads;
    fake_array *element = array->data.objects[(size_t)index];
    if (fails(&fail_element) && !element_with_exception) return NULL;
    if (element != NULL) ++borrowed;
    return (jobject)element;
}

static void JNICALL delete_reference(JNIEnv *env, jobject reference)
{
    (void)env;
    CHECK(reference != NULL && borrowed > 0);
    --borrowed; /* DeleteLocalRef remains allowed with a pending exception. */
}

static fake_array *region(jarray input, jsize offset, jsize count, array_kind kind)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == kind && !pending);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length && count <= array->length - offset);
    CHECK((size_t)offset <= fake_capacity(kind) && (size_t)count <= fake_capacity(kind) - (size_t)offset);
    return array;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    ++byte_calls;
    if (fails(&fail_bytes)) {
        if (count > 0) output[0] = 42;
        return;
    }
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}

static void JNICALL get_longs(JNIEnv *env, jlongArray input, jsize offset, jsize count, jlong *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, NUMBERS);
    ++long_calls;
    CHECK(numbers_identity == 0 && number_clears + 1 == long_calls);
    numbers_identity = (uintptr_t)output;
    if (fails(&fail_longs)) {
        if (count > 0) output[0] = INT64_MAX;
        return;
    }
    memcpy(output, array->data.numbers + (size_t)offset, (size_t)count * sizeof(*output));
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending && owned_inputs == NULL && borrowed == 0);
    scratch_retired();
    CHECK(length > 0 && (size_t)length <= ZCL_TX_WIRE_MAX + 1);
    ++new_calls;
    if (fail_new) {
        fail_new = false;
        new_failed = true;
        pending = !new_without_exception;
        if (!new_with_exception) return NULL;
    }
    return (jbyteArray)array_new(length, BYTES);
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    CHECK(owned_inputs == NULL && borrowed == 0);
    CHECK(output_identity == 0);
    output_identity = (uintptr_t)bytes;
    memcpy(array->data.bytes + (size_t)offset, bytes, (size_t)count);
    if (fail_set) { fail_set = false; pending = true; }
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetObjectArrayElement = get_element, .DeleteLocalRef = delete_reference,
    .GetByteArrayRegion = get_bytes, .GetLongArrayRegion = get_longs,
    .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes
};
static JNIEnv environment = &table;

static void maximum_fixture(void)
{
    zcl_transparent_tx *previous = &fixture.previous[0];
    previous->input_count = ZCL_TX_INPUT_MAX;
    previous->output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        previous->inputs[i] = previous->inputs[0];
        previous->inputs[i].previous_index = (uint32_t)i;
        previous->inputs[i].script_len = ZCL_TX_INPUT_SCRIPT_MAX;
        memset(previous->inputs[i].script, 0x51, ZCL_TX_INPUT_SCRIPT_MAX);
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        previous->outputs[i] = previous->outputs[0];
        previous->outputs[i].value = 1000;
    }
    CHECK(assessment_fixture_rebind(&fixture, 0));
    CHECK(fixture.sources[0].length == ZCL_TX_WIRE_MAX);
    fixture_request.input_count = ZCL_TX_INPUT_MAX;
    fixture_request.output_count = ZCL_TX_OUTPUT_MAX;
    fixture_request.lock_time = UINT32_MAX;
    fixture_request.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    fixture_request.maximum_fee = 7880;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture_request.inputs[i].previous = fixture.sources[0];
        fixture_request.inputs[i].output_index = (uint32_t)i;
        fixture_request.inputs[i].sequence = UINT32_C(0x80000000) + (uint32_t)i;
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture_request.outputs[i] = fixture_request.outputs[0];
        fixture_request.outputs[i].destination.kind = i % 2 == 0 ? ZCL_P2PKH : ZCL_P2SH;
        fixture_request.outputs[i].value = (uint64_t)i;
        memset(fixture_request.outputs[i].destination.hash, (int)i, 20);
    }
}

static java_inputs prepare_fixture(bool maximum, zcl_network network)
{
    CHECK(reference_count == 0 && owned_inputs == NULL);
    CHECK(draft_fixture_init(&fixture_request, &fixture, network));
    if (maximum) maximum_fixture();
    const size_t input_count = fixture_request.input_count, output_count = fixture_request.output_count;
    java_inputs inputs = {array_new((jsize)input_count, OBJECTS), array_new((jsize)output_count, OBJECTS),
        array_new((jsize)(3 + 2 * input_count + output_count), NUMBERS), (jint)network};
    destination_array = inputs.destinations;
    jlong *values = inputs.parameters->data.numbers;
    values[0] = (jlong)fixture_request.lock_time;
    values[1] = (jlong)fixture_request.expiry_height;
    values[2] = (jlong)fixture_request.maximum_fee;
    for (size_t i = 0; i < input_count; ++i) {
        const zcl_draft_funding *funding = &fixture_request.inputs[i];
        inputs.previous->data.objects[i] = byte_array(funding->previous.wire, funding->previous.length);
        values[3 + 2 * i] = (jlong)funding->output_index;
        values[4 + 2 * i] = (jlong)funding->sequence;
    }
    for (size_t i = 0; i < output_count; ++i) {
        uint8_t address[35];
        size_t length = 0;
        CHECK(zcl_address_encode(&fixture_request.outputs[i].destination, address, sizeof(address), &length) == ZCL_OK);
        inputs.destinations->data.objects[i] = byte_array(address, length);
        values[3 + 2 * input_count + i] = (jlong)fixture_request.outputs[i].value;
    }
    return inputs;
}

static fake_array *run(const java_inputs *inputs)
{
    const unsigned before = output_clears;
    const unsigned expected = pending ? 0U : 1U;
    fake_array *result = (fake_array *)API(&environment, NULL, (jobjectArray)inputs->previous,
        (jobjectArray)inputs->destinations, (jlongArray)inputs->parameters, inputs->network);
    scratch_retired();
    CHECK(output_clears == before + expected && output_identity == 0);
    return result;
}

static void verify_outputs(const java_inputs *inputs, const zcl_transaction_assessment *assessment,
    size_t input_count, size_t output_count)
{
    const jlong *values = inputs->parameters->data.numbers;
    for (size_t i = 0; i < output_count; ++i) {
        const fake_array *text = inputs->destinations->data.objects[i];
        CHECK(text != NULL && text->kind == BYTES && text->length >= 0 && text->length <= 35);
        zcl_address expected;
        CHECK(zcl_address_parse(text->data.bytes, (size_t)text->length, (zcl_network)inputs->network, &expected) == ZCL_OK);
        CHECK(assessment->outputs[i].destination.kind == expected.kind);
        CHECK(memcmp(assessment->outputs[i].destination.hash, expected.hash, 20) == 0);
        CHECK((jlong)assessment->outputs[i].value == values[3 + 2 * input_count + i]);
    }
}

static void verify_success(const java_inputs *inputs, const fake_array *result)
{
    CHECK(inputs->previous != NULL && inputs->destinations != NULL && inputs->parameters != NULL);
    CHECK(inputs->previous->length > 0 && inputs->previous->length <= 8);
    CHECK(inputs->destinations->length > 0 && inputs->destinations->length <= 16);
    const size_t input_count = (size_t)inputs->previous->length, output_count = (size_t)inputs->destinations->length;
    CHECK((size_t)inputs->parameters->length == 3 + 2 * input_count + output_count);
    const jlong *values = inputs->parameters->data.numbers;
    zcl_transparent_tx tx;
    CHECK(zcl_transaction_parse(result->data.bytes + 1, (size_t)result->length - 1, &tx) == ZCL_OK);
    CHECK(tx.input_count == input_count && tx.output_count == output_count);
    CHECK((jlong)tx.lock_time == values[0] && (jlong)tx.expiry_height == values[1]);
    zcl_previous_transaction previous[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < input_count; ++i) {
        const fake_array *source = inputs->previous->data.objects[i];
        CHECK(source != NULL && source->kind == BYTES && source->length >= 0 && (size_t)source->length <= ZCL_TX_WIRE_MAX);
        previous[i].wire = source->data.bytes;
        previous[i].length = (size_t)source->length;
        CHECK((jlong)tx.inputs[i].previous_index == values[3 + 2 * i]);
        CHECK((jlong)tx.inputs[i].sequence == values[4 + 2 * i] && tx.inputs[i].script_len == 0);
    }
    CHECK(inputs->network == (jint)ZCL_MAINNET || inputs->network == (jint)ZCL_TESTNET);
    CHECK(values[2] >= 0);
    zcl_transaction_assessment assessment;
    CHECK(zcl_transaction_assess(&tx, (zcl_network)inputs->network, previous, input_count,
        (uint64_t)values[2], &assessment) == ZCL_OK);
    verify_outputs(inputs, &assessment, input_count, output_count);
}

static void verify_result(const java_inputs *inputs, const fake_array *result)
{
    CHECK(owned_inputs == NULL && borrowed == 0);
    if (result == NULL) { CHECK(pending || new_failed); return; }
    CHECK(!pending && result->kind == BYTES && result->length > 0 && (size_t)result->length <= ZCL_TX_WIRE_MAX + 1);
    if (result->data.bytes[0] != ZCL_OK) {
        CHECK(result->length == 1 && result->data.bytes[0] <= ZCL_TLS_FAILURE);
        return;
    }
    verify_success(inputs, result);
}

#ifndef ZCL_JNI_DRAFT_FUZZ
static void expect_status(const java_inputs *inputs, zcl_status status)
{
    const fake_array *result = run(inputs);
    verify_result(inputs, result);
    CHECK(result != NULL && result->length == 1 && result->data.bytes[0] == (uint8_t)status);
}

static void exact_and_maximum_inputs(void)
{
    for (int network = 0; network < 2; ++network) for (int maximum = 0; maximum < 2; ++maximum) {
        const java_inputs inputs = prepare_fixture(maximum != 0, (zcl_network)network);
        const fake_array *result = run(&inputs);
        verify_result(&inputs, result);
        CHECK(result != NULL && result->data.bytes[0] == ZCL_OK && frees == 1 && new_calls == 1);
        if (maximum == 0) {
            uint8_t expected[ZCL_TX_WIRE_MAX];
            size_t length = 0;
            CHECK(zcl_transaction_serialize(&fixture.spending, expected, sizeof(expected), &length) == ZCL_OK);
            CHECK((size_t)result->length == length + 1 && memcmp(result->data.bytes + 1, expected, length) == 0);
            CHECK(length_calls == 7 && element_calls == 4 && byte_calls == 4 && long_calls == 1);
        } else {
            CHECK(inputs.parameters->length == (jsize)ZCL_DRAFT_PARAMETER_MAX);
            CHECK(result->length == 886 && element_calls == 24 && byte_calls == 24);
        }
        release_references();
    }
}

static unsigned failure_frees(size_t kind, unsigned ordinal)
{
    if (kind == 3 || (kind == 0 && ordinal <= 3)) return 0;
    return 1;
}

static void read_failures(void)
{
    const unsigned counts[] = {7, 4, 4, 1};
    for (size_t kind = 0; kind < 4; ++kind) for (unsigned ordinal = 1; ordinal <= counts[kind]; ++ordinal) {
        const java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        if (kind == 0) fail_length = ordinal;
        if (kind == 1) fail_element = ordinal;
        if (kind == 2) fail_bytes = ordinal;
        if (kind == 3) fail_longs = ordinal;
        const fake_array *result = run(&inputs);
        verify_result(&inputs, result);
        CHECK(result == NULL && pending && new_calls == 0);
        CHECK(frees == failure_frees(kind, ordinal));
        release_references();
    }
    for (unsigned ordinal = 1; ordinal <= 4; ++ordinal) {
        const java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        fail_element = ordinal;
        element_with_exception = true;
        CHECK(run(&inputs) == NULL && pending && borrowed == 0 && frees == 1);
        release_references();
    }
}

static void allocation_and_publication_failures(void)
{
    java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
    fail_malloc = true;
    expect_status(&inputs, ZCL_RESOURCE_EXHAUSTED);
    CHECK(frees == 0);
    release_references();
    for (int mode = 0; mode < 3; ++mode) {
        inputs = prepare_fixture(false, ZCL_MAINNET);
        fail_new = true;
        new_without_exception = mode == 1;
        new_with_exception = mode == 2;
        CHECK(run(&inputs) == NULL && new_failed && frees == 1 && borrowed == 0);
        CHECK(pending == (mode != 1));
        release_references();
    }
    inputs = prepare_fixture(false, ZCL_MAINNET);
    fail_set = true;
    CHECK(run(&inputs) == NULL && pending && frees == 1 && borrowed == 0);
    release_references();
    inputs = prepare_fixture(false, ZCL_MAINNET);
    pending = true;
    CHECK(run(&inputs) == NULL && new_calls == 0 && length_calls == 0 && frees == 0);
    release_references();
    CHECK(API(NULL, NULL, NULL, NULL, NULL, 0) == NULL);
}

static void null_arguments(void)
{
    for (size_t field = 0; field < 5; ++field) {
        java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        if (field == 0) inputs.previous = NULL;
        if (field == 1) inputs.destinations = NULL;
        if (field == 2) inputs.parameters = NULL;
        if (field == 3) inputs.previous->data.objects[1] = NULL;
        if (field == 4) inputs.destinations->data.objects[1] = NULL;
        expect_status(&inputs, ZCL_INVALID_ARGUMENT);
        release_references();
    }
}

static void invalid_array_counts(void)
{
    const jsize invalid[] = {-1, 0, INT32_MAX};
    for (size_t field = 0; field < 3; ++field) for (size_t i = 0; i < 3; ++i) {
        java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        fake_array *array = field == 0 ? inputs.previous : (field == 1 ? inputs.destinations : inputs.parameters);
        array->length = invalid[i];
        expect_status(&inputs, ZCL_OUT_OF_RANGE);
        release_references();
    }
    for (size_t field = 0; field < 3; ++field) {
        java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        fake_array *array = field == 0 ? inputs.previous : (field == 1 ? inputs.destinations : inputs.parameters);
        array->length = field == 0 ? 9 : (field == 1 ? 17 : 36);
        expect_status(&inputs, ZCL_OUT_OF_RANGE);
        release_references();
    }
}

static void parameter_shapes_and_widths(void)
{
    for (jsize count = 1; count <= (jsize)ZCL_DRAFT_PARAMETER_MAX; ++count) {
        if (count == 9) continue;
        java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        inputs.parameters->length = count;
        expect_status(&inputs, ZCL_INVALID_ARGUMENT);
        CHECK(long_calls == 0 && element_calls == 0);
        release_references();
    }
    for (size_t field = 0; field < 9; ++field) for (int high = 0; high < 2; ++high) {
        const java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        inputs.parameters->data.numbers[field] = high == 0 ? INT64_MIN : INT64_MAX;
        expect_status(&inputs, ZCL_OUT_OF_RANGE);
        CHECK(element_calls == 0 && frees == 0);
        release_references();
    }
}

static void malformed_byte_arrays(void)
{
    const jsize lengths[] = {-1, 0, 1926, INT32_MAX};
    for (size_t field = 0; field < 2; ++field) for (size_t i = 0; i < 4; ++i) {
        const java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
        fake_array *array = field == 0 ? inputs.previous->data.objects[1] : inputs.destinations->data.objects[1];
        array->length = lengths[i];
        const fake_array *result = run(&inputs);
        verify_result(&inputs, result);
        CHECK(result != NULL && result->length == 1 && result->data.bytes[0] != ZCL_OK);
        release_references();
    }
    java_inputs inputs = prepare_fixture(false, ZCL_MAINNET);
    inputs.network = (jint)ZCL_TESTNET;
    expect_status(&inputs, ZCL_UNSUPPORTED);
    release_references();
    inputs = prepare_fixture(false, ZCL_MAINNET);
    inputs.network = 2;
    expect_status(&inputs, ZCL_UNSUPPORTED);
    CHECK(length_calls == 0 && frees == 0);
    release_references();
    inputs = prepare_fixture(false, ZCL_MAINNET);
    inputs.destinations->data.objects[1]->data.bytes[0] = 0xff;
    const fake_array *result = run(&inputs);
    verify_result(&inputs, result);
    CHECK(result != NULL && result->length == 1 && result->data.bytes[0] != ZCL_OK);
    release_references();
}

static void bounded_wire_helper(void)
{
    CHECK(draft_fixture_init(&fixture_request, &fixture, ZCL_MAINNET));
    for (size_t capacity = 0; capacity < 177; ++capacity) {
        uint8_t wire[ZCL_TX_WIRE_MAX];
        memset(wire, 0xa5, sizeof(wire));
        size_t length = 73;
        CHECK(zcl_jni_draft_wire(&fixture_request, wire, capacity, &length) != ZCL_OK);
        CHECK(length == 73);
        for (size_t i = 0; i < sizeof(wire); ++i) CHECK(wire[i] == 0xa5);
    }
    jlong values[ZCL_DRAFT_PARAMETER_MAX] = {0};
    CHECK(zcl_jni_draft_fields(NULL, 9, &fixture_request) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_jni_draft_fields(values, 9, NULL) == ZCL_INVALID_ARGUMENT);
    fixture_request.input_count = SIZE_MAX;
    CHECK(zcl_jni_draft_fields(values, 9, &fixture_request) == ZCL_OUT_OF_RANGE);
    fixture_request.input_count = 2;
    fixture_request.output_count = SIZE_MAX;
    CHECK(zcl_jni_draft_fields(values, 9, &fixture_request) == ZCL_OUT_OF_RANGE);
}

int main(void)
{
    exact_and_maximum_inputs();
    read_failures();
    allocation_and_publication_failures();
    null_arguments();
    invalid_array_counts();
    parameter_shapes_and_widths();
    malformed_byte_arrays();
    bounded_wire_helper();
    puts("JNI draft checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static uint8_t byte_at(const uint8_t *data, size_t size, size_t index)
{
    return index < size ? data[index] : 0;
}

static jlong signed_word(const uint8_t *data, size_t size)
{
    uint64_t word = 0;
    for (size_t i = 0; i < 8; ++i) word |= (uint64_t)byte_at(data, size, 3 + i) << (8 * i);
    return word <= INT64_MAX ? (jlong)word : -1 - (jlong)(UINT64_MAX - word);
}

static void mutate_source(fake_array *source, const uint8_t *data, size_t size)
{
    size_t length = size > 2 ? size - 2 : 0;
    if (length > sizeof(source->data.bytes)) length = sizeof(source->data.bytes);
    if (length != 0) memcpy(source->data.bytes, data + 2, length);
    source->length = (jsize)length;
}

static void mutate(java_inputs *inputs, const uint8_t *data, size_t size)
{
    const uint8_t selector = byte_at(data, size, 2);
    switch (byte_at(data, size, 1) % 12) {
    case 0:
        inputs->parameters->data.numbers[selector % (size_t)inputs->parameters->length] = signed_word(data, size);
        break;
    case 1:
        mutate_source(inputs->previous->data.objects[0], data, size);
        break;
    case 2:
        inputs->destinations->data.objects[0]->data.bytes[selector % 35] = byte_at(data, size, 3);
        break;
    case 3:
        inputs->previous->data.objects[0]->length = selector % 2 == 0 ? -1 : INT32_MAX;
        break;
    case 4:
        inputs->destinations->length = selector % 2 == 0 ? 0 : 17;
        break;
    case 5: fail_length = (unsigned)(selector % 30) + 1; break;
    case 6: fail_element = (unsigned)(selector % 24) + 1; element_with_exception = (selector & 1) != 0; break;
    case 7: fail_bytes = (unsigned)(selector % 24) + 1; break;
    case 8: fail_longs = 1; break;
    case 9:
        fail_new = true;
        new_without_exception = selector % 3 == 1;
        new_with_exception = selector % 3 == 2;
        break;
    case 10: fail_set = true; break;
    default: fail_malloc = true; break;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_TX_WIRE_MAX + 3) return 0;
    const uint8_t flags = byte_at(data, size, 0);
    java_inputs inputs = prepare_fixture((flags & 1) != 0, (flags & 2) != 0 ? ZCL_TESTNET : ZCL_MAINNET);
    mutate(&inputs, data, size);
    const fake_array *result = run(&inputs);
    verify_result(&inputs, result);
    release_references();
    return 0;
}
#endif
