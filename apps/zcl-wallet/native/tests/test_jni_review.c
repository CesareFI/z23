/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef malloc
#undef free
#undef zcl_secure_zero
#undef zcl_review_snapshot_get
#undef zcl_review_open_full_sources
#undef zcl_transaction_draft_full_sources
#include "jni_review_internal.h"
#include "jni_draft_internal.h"
#include "transaction_draft_internal.h"
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include "assessment_fixture.h"
#include "source_assessment_fixture.h"
#include "transaction_source_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI review check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openReview)(JNIEnv *, jclass, jbyteArray, jobjectArray, jint, jlong, jlong);
JNIEXPORT jlong JNICALL API(openFullSourceReview)(JNIEnv *, jclass, jbyteArray, jobjectArray, jint, jlong, jlong);
JNIEXPORT jlong JNICALL API(prepareFullSourceReview)(JNIEnv *, jclass, jobjectArray, jobjectArray, jlongArray, jint, jlong);
JNIEXPORT jint JNICALL API(cancelReview)(JNIEnv *, jclass, jlong);
JNIEXPORT jlongArray JNICALL API(reviewSnapshot)(JNIEnv *, jclass, jlong, jlong);
JNIEXPORT jbyteArray JNICALL API(reviewWire)(JNIEnv *, jclass, jlong, jlong);

/* Bounded fake VM verifies regions, pending exceptions and local-reference
 * balancing. Real JVM/device qualification remains separately required. */
typedef enum { BYTES, NUMBERS, OBJECTS } array_kind;
typedef struct fake_array {
    jsize length;
    array_kind kind;
    union {
        uint8_t bytes[ZCL_V4_SOURCE_MAX + 1];
        jlong numbers[ZCL_REVIEW_PACKET_MAX];
        struct fake_array *objects[ZCL_TX_OUTPUT_MAX];
    } data;
} fake_array;
static fake_array *references[64];
static fake_array *captured[ZCL_TX_INPUT_MAX];
static size_t reference_count, borrowed;
static bool pending, fail_malloc, fail_set;
static bool element_with_exception;
/* New-array faults: 0=none, 1=NULL+exception, 2=array+exception, 3=NULL only. */
static unsigned fail_new, fail_length, fail_element, fail_region;
static void (*allocation_hook)(void);
static void *owned_inputs;
static size_t owned_size, last_allocation_size;
static size_t byte_reads;
static bool full_sources;
static bool preparing, fail_construction;
static unsigned request_clears, transaction_clears, parameter_clears, address_clears;
static uintptr_t parameter_identity;
static void (*construction_hook)(void);
static unsigned source_clears, draft_clears;
static void (*input_allocation_hook)(void);
static assessment_fixture fixture;
static source_assessment_fixture full_fixture;
static uint8_t draft[ZCL_TX_WIRE_MAX];
static size_t draft_length;
static uintptr_t snapshot_identity, output_identity;
static unsigned snapshot_reads, snapshot_clears, number_clears, wire_clears;

zcl_status zcl_jni_review_test_snapshot(zcl_review_owner *owner, uint64_t id, uint64_t now,
    zcl_review_snapshot *snapshot);
void zcl_jni_review_test_zero(void *buffer, size_t length);
zcl_status zcl_jni_review_test_full_open(zcl_review_owner *owner, const uint8_t *wire, size_t length,
    zcl_network network, const zcl_previous_transaction *sources, size_t count,
    uint64_t fee, uint64_t now, uint64_t *id);
zcl_status zcl_jni_review_test_construct(const zcl_draft_request *request, zcl_transparent_tx *transaction);

zcl_status zcl_jni_review_test_construct(const zcl_draft_request *request, zcl_transparent_tx *transaction)
{
    CHECK(preparing && borrowed == 0 && owned_inputs != NULL && request_clears == 0);
    size_t used = 0;
    for (size_t i = 0; i < request->input_count; ++i) {
        CHECK(request->inputs[i].previous.wire == (const uint8_t *)owned_inputs + used);
        CHECK(request->inputs[i].previous.length <= owned_size - used);
        used += request->inputs[i].previous.length;
    }
    CHECK(used == owned_size);
    memset(transaction, 0xa5, sizeof(*transaction));
    if (fail_construction) return ZCL_CRYPTO_FAILURE;
    const zcl_status status = zcl_transaction_draft_full_sources(request, transaction);
    if (construction_hook != NULL) {
        void (*hook)(void) = construction_hook;
        construction_hook = NULL;
        hook();
    }
    return status;
}

zcl_status zcl_jni_review_test_full_open(zcl_review_owner *owner, const uint8_t *wire, size_t length,
    zcl_network network, const zcl_previous_transaction *sources, size_t count,
    uint64_t fee, uint64_t now, uint64_t *id)
{
    CHECK(full_sources && borrowed == 0 && owned_inputs != NULL);
    CHECK(source_clears == 0 && draft_clears == 0 && count > 0 && count <= ZCL_TX_INPUT_MAX);
    CHECK(transaction_clears == 1);
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        CHECK(sources[i].wire == (const uint8_t *)owned_inputs + used);
        CHECK(sources[i].length <= owned_size - used);
        used += sources[i].length;
    }
    CHECK(used == owned_size);
    return zcl_review_open_full_sources(owner, wire, length, network, sources, count, fee, now, id);
}

zcl_status zcl_jni_review_test_snapshot(zcl_review_owner *owner, uint64_t id, uint64_t now,
    zcl_review_snapshot *snapshot)
{
    CHECK(snapshot_identity == 0 && snapshot_reads == snapshot_clears);
    snapshot_identity = (uintptr_t)snapshot;
    ++snapshot_reads;
    return zcl_review_snapshot_get(owner, id, now, snapshot);
}

static bool preparation_clear(void *buffer, size_t length)
{
    if (length == sizeof(zcl_draft_request)) { CHECK(request_clears++ == 0); return true; }
    if (length == sizeof(zcl_transparent_tx)) { CHECK(transaction_clears++ == 0); return true; }
    if ((uintptr_t)buffer == parameter_identity) {
        CHECK(length == ZCL_DRAFT_PARAMETER_MAX * sizeof(jlong) && parameter_clears++ == 0);
        parameter_identity = 0;
        return true;
    }
    if (length == 35) { ++address_clears; return true; }
    return false;
}

static bool input_scratch_clear(void *buffer, size_t length)
{
    if (preparing && preparation_clear(buffer, length)) return true;
    if (full_sources && length == sizeof(zcl_transparent_tx)) { CHECK(transaction_clears++ == 0); return true; }
    if (full_sources && length == sizeof(zcl_jni_full_sources)) { CHECK(source_clears++ == 0 && borrowed == 0); return true; }
    if (full_sources && length == ZCL_TX_WIRE_MAX) { CHECK(draft_clears++ == 0); return true; }
    return false;
}

void zcl_jni_review_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if (buffer == owned_inputs) {
        CHECK(length == owned_size);
        if (full_sources) CHECK(source_clears == 1 && draft_clears == 1 && borrowed == 0);
        if (preparing) CHECK(request_clears == 1);
        return;
    }
    if (input_scratch_clear(buffer, length)) return;
    if ((uintptr_t)buffer == snapshot_identity) {
        CHECK(length == sizeof(zcl_review_snapshot));
        snapshot_identity = 0;
        ++snapshot_clears;
        return;
    }
    CHECK(snapshot_identity == 0);
    CHECK(output_identity == 0 || output_identity == (uintptr_t)buffer);
    output_identity = 0;
    if (length == ZCL_REVIEW_PACKET_MAX * sizeof(jlong)) ++number_clears;
    else { CHECK(length == ZCL_TX_WIRE_MAX + 1); ++wire_clears; }
}

static jlongArray snapshot_read(JNIEnv *env, jclass type, jlong id, jlong now)
{
    const unsigned before = number_clears;
    const unsigned expected = env != NULL && !pending ? 1U : 0U;
    CHECK(output_identity == 0 && snapshot_identity == 0);
    jlongArray result = API(reviewSnapshot)(env, type, id, now);
    CHECK(number_clears == before + expected && snapshot_reads == snapshot_clears);
    CHECK(output_identity == 0 && snapshot_identity == 0);
    return result;
}

static jbyteArray wire_read(JNIEnv *env, jclass type, jlong id, jlong now)
{
    const unsigned before = wire_clears;
    const unsigned expected = env != NULL && !pending ? 1U : 0U;
    CHECK(output_identity == 0 && snapshot_identity == 0);
    jbyteArray result = API(reviewWire)(env, type, id, now);
    CHECK(wire_clears == before + expected && output_identity == 0);
    return result;
}

void *zcl_jni_review_test_malloc(size_t size)
{
    size_t expected_size = sizeof(zcl_jni_review_inputs);
    if (full_sources) {
        CHECK(borrowed > 0 && borrowed <= ZCL_TX_INPUT_MAX);
        expected_size = 0;
        for (size_t i = 0; i < borrowed; ++i) {
            CHECK(captured[i] != NULL && captured[i]->length > 0 && (size_t)captured[i]->length <= ZCL_V4_SOURCE_MAX);
            expected_size += (size_t)captured[i]->length;
        }
    }
    CHECK(size == expected_size && owned_inputs == NULL);
    if (fail_malloc) { fail_malloc = false; return NULL; }
    owned_inputs = malloc(size);
    owned_size = last_allocation_size = size;
    CHECK(owned_inputs != NULL);
    memset(owned_inputs, 0xa5, size);
    if (input_allocation_hook != NULL) {
        void (*hook)(void) = input_allocation_hook;
        input_allocation_hook = NULL;
        hook();
    }
    return owned_inputs;
}

void zcl_jni_review_test_free(void *pointer)
{
    CHECK(pointer != NULL && pointer == owned_inputs);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < owned_size; ++i) CHECK(bytes[i] == 0);
    if (full_sources) CHECK(source_clears == 1 && draft_clears == 1 && borrowed == 0);
    owned_inputs = NULL;
    owned_size = 0;
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

static fake_array *array_new(jsize length, array_kind kind)
{
    const size_t maximum = kind == BYTES ? ZCL_V4_SOURCE_MAX + 1 :
        (kind == NUMBERS ? ZCL_REVIEW_PACKET_MAX : ZCL_TX_OUTPUT_MAX);
    CHECK(length >= 0 && (size_t)length <= maximum);
    CHECK(reference_count < sizeof(references) / sizeof(references[0]));
    fake_array *result = calloc(1, sizeof(*result));
    CHECK(result != NULL);
    result->length = length;
    result->kind = kind;
    references[reference_count++] = result;
    return result;
}

static void release_references(void)
{
    CHECK(owned_inputs == NULL && borrowed == 0);
    CHECK(snapshot_identity == 0 && output_identity == 0 && snapshot_reads == snapshot_clears);
    snapshot_reads = snapshot_clears = number_clears = wire_clears = 0;
    source_clears = draft_clears = 0;
    request_clears = transaction_clears = parameter_clears = address_clears = 0;
    CHECK(construction_hook == NULL);
    CHECK(parameter_identity == 0);
    fail_construction = false;
    CHECK(allocation_hook == NULL);
    CHECK(input_allocation_hook == NULL);
    for (size_t i = 0; i < reference_count; ++i) { free(references[i]); references[i] = NULL; }
    reference_count = 0;
    pending = fail_malloc = fail_set = false;
    element_with_exception = false;
    fail_new = 0;
    fail_length = fail_element = fail_region = 0;
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
    if (fails(&fail_length)) return 0;
    return ((fake_array *)input)->length;
}

static jobject JNICALL get_element(JNIEnv *env, jobjectArray input, jsize index)
{
    (void)env;
    const fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == OBJECTS && !pending);
    CHECK(index >= 0 && index < array->length && (size_t)index < ZCL_TX_OUTPUT_MAX);
    if (fails(&fail_element) && !element_with_exception) return NULL;
    fake_array *element = array->data.objects[(size_t)index];
    if (element != NULL) { CHECK(borrowed < ZCL_TX_INPUT_MAX); captured[borrowed++] = element; }
    return (jobject)element;
}

static void JNICALL delete_reference(JNIEnv *env, jobject reference)
{
    (void)env;
    CHECK(reference != NULL && borrowed > 0);
    --borrowed; /* Allowed with a pending exception, as in the actual VM. */
    if (borrowed == 0) memset(captured, 0, sizeof(captured));
}

static fake_array *region(jarray input, jsize offset, jsize count, array_kind kind)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->kind == kind && !pending);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length && count <= array->length - offset);
    return array;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    byte_reads += (size_t)count;
    if (fails(&fail_region)) {
        if (count > 0) output[0] = 42; /* Partially written scratch must still clear. */
        return;
    }
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}

static void JNICALL get_numbers(JNIEnv *env, jlongArray input, jsize offset, jsize count, jlong *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, NUMBERS);
    CHECK(parameter_identity == 0);
    parameter_identity = (uintptr_t)output;
    if (fails(&fail_region)) { if (count > 0) output[0] = INT64_MAX; return; }
    memcpy(output, array->data.numbers + (size_t)offset, (size_t)count * sizeof(*output));
}

static jlongArray JNICALL new_numbers(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending);
    CHECK(snapshot_identity == 0 && snapshot_reads == snapshot_clears);
    if (allocation_hook != NULL) {
        void (*hook)(void) = allocation_hook;
        allocation_hook = NULL;
        hook();
    }
    const unsigned failure = fail_new;
    fail_new = 0;
    if (failure != 0) {
        pending = failure != 3;
        if (failure != 2) return NULL;
    }
    return (jlongArray)array_new(length, NUMBERS);
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending);
    if (allocation_hook != NULL) {
        void (*hook)(void) = allocation_hook;
        allocation_hook = NULL;
        hook();
    }
    const unsigned failure = fail_new;
    fail_new = 0;
    if (failure != 0) {
        pending = failure != 3;
        if (failure != 2) return NULL;
    }
    return (jbyteArray)array_new(length, BYTES);
}

static void JNICALL set_numbers(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, NUMBERS);
    CHECK(output_identity == 0);
    output_identity = (uintptr_t)data;
    memcpy(array->data.numbers + (size_t)offset, data, (size_t)count * sizeof(*data));
    if (fail_set) { fail_set = false; pending = true; }
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    CHECK(output_identity == 0);
    output_identity = (uintptr_t)data;
    memcpy(array->data.bytes + (size_t)offset, data, (size_t)count);
    if (fail_set) { fail_set = false; pending = true; }
}

#ifdef __ANDROID__
typedef struct JNINativeInterface review_jni_interface;
#else
typedef struct JNINativeInterface_ review_jni_interface;
#endif
static const review_jni_interface vm_table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetObjectArrayElement = get_element, .DeleteLocalRef = delete_reference,
    .GetByteArrayRegion = get_bytes, .NewLongArray = new_numbers,
    .SetLongArrayRegion = set_numbers, .NewByteArray = new_bytes,
    .SetByteArrayRegion = set_bytes, .GetLongArrayRegion = get_numbers
};
static JNIEnv vm = &vm_table;
static fake_array *java_draft, *java_previous;
static fake_array *java_parameters, *java_destinations;

static fake_array *bytes(const uint8_t *data, size_t length)
{
    CHECK(length <= ZCL_V4_SOURCE_MAX + 1);
    fake_array *array = array_new((jsize)length, BYTES);
    memcpy(array->data.bytes, data, length);
    return array;
}

static void java_selections(void)
{
    const size_t count = fixture.spending.input_count, outputs = fixture.spending.output_count;
    java_parameters = array_new((jsize)(3 + 2 * count + outputs), NUMBERS);
    jlong *values = java_parameters->data.numbers;
    values[0] = fixture.spending.lock_time; values[1] = fixture.spending.expiry_height; values[2] = 500;
    for (size_t i = 0; i < count; ++i) {
        values[3 + 2 * i] = fixture.spending.inputs[i].previous_index;
        values[4 + 2 * i] = fixture.spending.inputs[i].sequence;
    }
    java_destinations = array_new((jsize)outputs, OBJECTS);
    for (size_t i = 0; i < outputs; ++i) {
        zcl_address address;
        uint8_t text[35]; size_t length = 0;
        CHECK(zcl_address_from_script(fixture.spending.outputs[i].script, fixture.spending.outputs[i].script_len,
            ZCL_MAINNET, &address) == ZCL_OK);
        CHECK(zcl_address_encode(&address, text, sizeof(text), &length) == ZCL_OK);
        java_destinations->data.objects[i] = bytes(text, length);
        values[3 + 2 * count + i] = (jlong)fixture.spending.outputs[i].value;
    }
}

static void java_inputs(void)
{
    CHECK(zcl_transaction_serialize(&fixture.spending, draft, sizeof(draft), &draft_length) == ZCL_OK);
    java_draft = bytes(draft, draft_length);
    java_previous = array_new((jsize)fixture.spending.input_count, OBJECTS);
    for (size_t i = 0; i < fixture.spending.input_count; ++i)
        java_previous->data.objects[i] = bytes(fixture.sources[i].wire, fixture.sources[i].length);
    if (preparing) java_selections();
}

static void setup(void)
{
    CHECK(reference_count == 0 && assessment_fixture_init(&fixture));
    if (full_sources) {
        CHECK(source_assessment_init(&full_fixture, 7));
        fixture = full_fixture.base;
    }
    fixture.spending.lock_time = UINT32_MAX;
    fixture.spending.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    fixture.spending.inputs[0].sequence = UINT32_C(0x80000000);
    java_inputs();
}

static jlong open_review_fee(jlong now, jlong fee)
{
    last_allocation_size = byte_reads = 0;
    source_clears = draft_clears = 0;
    request_clears = transaction_clears = parameter_clears = address_clears = 0;
    const jlong result = preparing
        ? API(prepareFullSourceReview)(&vm, NULL, (jobjectArray)java_previous, (jobjectArray)java_destinations,
            (jlongArray)java_parameters, (jint)ZCL_MAINNET, now)
        : full_sources
        ? API(openFullSourceReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous, (jint)ZCL_MAINNET, fee, now)
        : API(openReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous, (jint)ZCL_MAINNET, fee, now);
    if (full_sources) CHECK(source_clears == draft_clears && borrowed == 0 && owned_inputs == NULL);
    if (result == -(jlong)ZCL_BUSY) CHECK(source_clears == 0 && draft_clears == 0 && request_clears == 0);
    return result;
}

static jlong open_review(jlong now) { return open_review_fee(now, 500); }

static void fee_admission(void)
{
    setup();
    for (size_t i = 0; i < fixture.spending.input_count; ++i)
        java_previous->data.objects[i]->length = (jsize)(full_sources ? ZCL_V4_SOURCE_MAX : ZCL_TX_WIRE_MAX);
    const jlong fees[] = {-1, (jlong)ZCL_MAX_MONEY + 1, INT64_MAX};
    for (size_t i = 0; i < sizeof(fees) / sizeof(fees[0]); ++i) {
        CHECK(open_review_fee(100, fees[i]) == -(jlong)ZCL_OUT_OF_RANGE);
        fprintf(stderr, "Rejected fee: allocated=%zu copied=%zu\n", last_allocation_size, byte_reads);
        CHECK(last_allocation_size == 0 && byte_reads == 0 && !pending);
    }
    release_references();
    setup();
    const jlong id = open_review_fee(100, (jlong)ZCL_MAX_MONEY);
    CHECK(id > 0 && API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    release_references();
}

static void hash_matches(const jlong *words, const uint8_t *expected, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        CHECK(words[i / 4] >= 0 && (uint64_t)words[i / 4] <= UINT32_MAX);
        const uint64_t byte = ((uint64_t)words[i / 4] >> (8 * (3 - i % 4))) & UINT64_C(255);
        CHECK(byte == expected[i]);
    }
}

static void destination_matches(const jlong *values, const zcl_tx_output *output)
{
    zcl_address address;
    CHECK(zcl_address_from_script(output->script, output->script_len, ZCL_MAINNET, &address) == ZCL_OK);
    CHECK(values[0] == (jlong)output->value && values[1] == (jlong)address.kind);
    hash_matches(&values[2], address.hash, sizeof(address.hash));
}

static void snapshot_matches(fake_array *array, jlong remaining)
{
    CHECK(array != NULL && array->kind == NUMBERS);
    const size_t count = fixture.spending.input_count, outputs = fixture.spending.output_count;
    CHECK(array->length == (jsize)(20 + 17 * count + 7 * outputs));
    const jlong *values = array->data.numbers;
    CHECK(values[0] == ZCL_OK && values[1] == remaining && values[2] == ZCL_MAINNET);
    CHECK(values[3] == fixture.spending.lock_time && values[4] == fixture.spending.expiry_height);
    CHECK(values[5] == (jlong)draft_length && values[6] == (jlong)count && values[7] == (jlong)outputs);
    CHECK(values[8] >= 0 && values[9] >= 0 && values[8] - values[9] == 500);
    CHECK(values[10] == 500 && values[11] == 500);
    uint8_t txid[32];
    CHECK(zcl_transaction_id(&fixture.spending, txid, sizeof(txid)) == ZCL_OK);
    hash_matches(&values[12], txid, sizeof(txid));
    for (size_t i = 0; i < count; ++i) {
        const jlong *row = &values[20 + i * 17];
        hash_matches(row, fixture.spending.inputs[i].previous_txid, 32);
        CHECK(row[8] == fixture.spending.inputs[i].previous_index && row[9] == fixture.spending.inputs[i].sequence);
        zcl_tx_output previous;
        const zcl_status status = full_sources
            ? zcl_v4_source_prevout(&fixture.spending.inputs[i], fixture.sources[i].wire, fixture.sources[i].length, &previous)
            : zcl_transaction_prevout(&fixture.spending.inputs[i], fixture.sources[i].wire, fixture.sources[i].length, &previous);
        CHECK(status == ZCL_OK);
        destination_matches(&row[10], &previous);
    }
    for (size_t i = 0; i < outputs; ++i)
        destination_matches(&values[20 + 17 * count + 7 * i], &fixture.spending.outputs[i]);
}

static void snapshot_error(jlong id, jlong now, zcl_status status)
{
    fake_array *array = (fake_array *)snapshot_read(&vm, NULL, id, now);
    CHECK(array != NULL && array->kind == NUMBERS && array->length == 1);
    CHECK(array->data.numbers[0] == (jlong)status);
}

static void golden_and_lifetime(void)
{
    setup();
    const jlong first = open_review(100);
    CHECK(first > 0 && open_review(100) == -(jlong)ZCL_BUSY);
    memset(java_draft->data.bytes, 0xff, (size_t)java_draft->length);
    memset(java_previous->data.objects[0]->data.bytes, 0xff,
        (size_t)java_previous->data.objects[0]->length);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, first, 101), 89999);
    fake_array *wire = (fake_array *)wire_read(&vm, NULL, first, 102);
    CHECK(wire != NULL && wire->kind == BYTES && wire->length == (jsize)(draft_length + 1));
    CHECK(wire->data.bytes[0] == ZCL_OK && memcmp(wire->data.bytes + 1, draft, draft_length) == 0);
    memset(wire->data.bytes, 0, (size_t)wire->length);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, first, 103), 89997);
    CHECK(API(cancelReview)(&vm, NULL, first) == ZCL_OK);
    snapshot_error(first, 103, ZCL_CANCELLED);
    release_references();
    setup();
    const jlong second = open_review(200);
    CHECK(second > first);
    snapshot_error(first, INT64_MAX, ZCL_CANCELLED);
    CHECK(API(cancelReview)(&vm, NULL, first) == ZCL_CANCELLED);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, second, 201), 89999);
    snapshot_error(second, 90200, ZCL_TIMED_OUT);
    CHECK(API(cancelReview)(&vm, NULL, second) == ZCL_CANCELLED);
    const jlong third = open_review(300);
    CHECK(third > second);
    snapshot_error(third, 299, ZCL_CANCELLED);
    release_references();
}

static void opening_failures(void)
{
    const unsigned counts[3][4] = {{4, 2, 3, 1}, {5, 2, 3, 1}, {8, 4, 5, 1}};
    for (unsigned kind = 0; kind < 4; ++kind) {
        const unsigned maximum = counts[preparing ? 2 : full_sources ? 1 : 0][kind];
        for (unsigned point = 1; point <= maximum; ++point) {
            setup();
            if (kind == 0) fail_length = point;
            else if (kind == 1) fail_element = point;
            else if (kind == 2) fail_region = point;
            else fail_malloc = true;
            const jlong expected = kind == 3 ? -(jlong)ZCL_RESOURCE_EXHAUSTED : -(jlong)ZCL_INVALID_ARGUMENT;
            CHECK(open_review(100) == expected && owned_inputs == NULL && borrowed == 0);
            CHECK(pending == (kind != 3));
            pending = false; /* Managed caller catches the simulated VM exception. */
            const jlong id = open_review(100);
            CHECK(id > 0 && API(cancelReview)(&vm, NULL, id) == ZCL_OK);
            release_references();
        }
    }
}

static void publication_failures(void)
{
    for (unsigned kind = 0; kind < 8; ++kind) {
        setup();
        const jlong id = open_review(100);
        CHECK(id > 0);
        fail_new = kind % 4;
        fail_set = !fail_new;
        if (kind < 4) CHECK(snapshot_read(&vm, NULL, id, 101) == NULL);
        else CHECK(wire_read(&vm, NULL, id, 101) == NULL);
        CHECK(pending == (kind % 4 != 3) && borrowed == 0 && owned_inputs == NULL);
        pending = false;
        snapshot_error(id, 102, ZCL_CANCELLED);
        release_references();
    }
}

static jlong publication_old, publication_replacement;
static void replace_during_publication(void)
{
    /* Deterministically interleave another owner's close/open after sampling,
     * at Java allocation. Holding the native mutex here would deadlock. */
    CHECK(API(cancelReview)(&vm, NULL, publication_old) == ZCL_OK);
    publication_replacement = open_review(100);
    CHECK(publication_replacement > publication_old);
}

static void publication_races(void)
{
    for (unsigned kind = 0; kind < 8; ++kind) {
        setup();
        publication_old = open_review(100);
        CHECK(publication_old > 0);
        allocation_hook = replace_during_publication;
        fail_new = kind / 2;
        fail_set = !fail_new;
        if (kind % 2 == 0) CHECK(snapshot_read(&vm, NULL, publication_old, 101) == NULL);
        else CHECK(wire_read(&vm, NULL, publication_old, 101) == NULL);
        CHECK(pending == (kind / 2 != 3) && allocation_hook == NULL);
        pending = false;
        snapshot_matches((fake_array *)snapshot_read(&vm, NULL, publication_replacement, 100), 90000);
        CHECK(API(cancelReview)(&vm, NULL, publication_replacement) == ZCL_OK);
        release_references();
    }
}

static void argument_failures(void)
{
    jlong (JNICALL *entry)(JNIEnv *, jclass, jbyteArray, jobjectArray, jint, jlong, jlong)
        = full_sources ? API(openFullSourceReview) : API(openReview);
    setup();
    CHECK(entry(NULL, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(entry(&vm, NULL, NULL, (jobjectArray)java_previous, 0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(entry(&vm, NULL, (jbyteArray)java_draft, NULL, 0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(entry(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        2, 500, 100) == -(jlong)ZCL_UNSUPPORTED);
    CHECK(entry(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        0, -1, 100) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(entry(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        0, INT64_MAX, 100) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(open_review(-1) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(open_review(INT64_MAX - 89999) == -(jlong)ZCL_OUT_OF_RANGE);
    pending = true;
    CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT);
    pending = false;
    const jsize counts[] = {-1, 0, 1, 3, 9, INT32_MAX};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        java_previous->length = counts[i];
        CHECK(open_review(100) < 0 && owned_inputs == NULL && borrowed == 0);
    }
    java_previous->length = 2;
    fake_array *saved = java_previous->data.objects[1];
    java_previous->data.objects[1] = NULL;
    CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT);
    java_previous->data.objects[1] = saved;
    const jsize original_length = saved->length;
    saved->length = (jsize)ZCL_TX_WIRE_MAX + 1;
    CHECK(open_review(100) == -(jlong)(full_sources ? ZCL_INVALID_ENCODING : ZCL_OUT_OF_RANGE));
    saved->length = original_length;
    java_draft->length = (jsize)ZCL_TX_WIRE_MAX + 1;
    CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE);
    java_draft->length = (jsize)draft_length;
    const jlong id = open_review(100);
    CHECK(id > 0);
    CHECK(snapshot_read(NULL, NULL, id, 100) == NULL);
    CHECK(wire_read(NULL, NULL, id, 100) == NULL);
    pending = true;
    CHECK(snapshot_read(&vm, NULL, id, 100) == NULL);
    CHECK(wire_read(&vm, NULL, id, 100) == NULL);
    pending = false;
    snapshot_error(id, -1, ZCL_OUT_OF_RANGE);
    snapshot_error(0, INT64_MAX, ZCL_CANCELLED);
    snapshot_error(-1, INT64_MAX, ZCL_CANCELLED);
    CHECK(API(cancelReview)(&vm, NULL, -1) == ZCL_CANCELLED);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, id, 100), 90000);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    const jlong last = open_review(INT64_MAX - 90000);
    CHECK(last > id);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, last, INT64_MAX - 1), 1);
    snapshot_error(last, INT64_MAX, ZCL_TIMED_OUT);
    release_references();
}

static void maximum_packet(void)
{
    CHECK(assessment_fixture_init(&fixture));
    fixture.previous[0].output_count = ZCL_TX_OUTPUT_MAX;
    const zcl_tx_output destination = fixture.previous[0].outputs[0];
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.previous[0].outputs[i] = destination;
        fixture.previous[0].outputs[i].value = i == 8 ? 10000 : 0;
    }
    CHECK(assessment_fixture_rebind(&fixture, 0));
    fixture.spending.input_count = ZCL_TX_INPUT_MAX;
    fixture.spending.output_count = ZCL_TX_OUTPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        fixture.spending.inputs[i] = fixture.spending.inputs[0];
        fixture.spending.inputs[i].previous_index = (uint32_t)(8 + i);
        fixture.spending.inputs[i].sequence = UINT32_MAX - (uint32_t)i;
        fixture.sources[i] = fixture.sources[0];
    }
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        fixture.spending.outputs[i] = destination;
        fixture.spending.outputs[i].value = i == 0 ? 9500 : 0;
    }
    java_inputs();
    const jlong id = open_review(100);
    CHECK(id > 0);
    fake_array *packet = (fake_array *)snapshot_read(&vm, NULL, id, 100);
    CHECK(packet != NULL && packet->length == (jsize)ZCL_REVIEW_PACKET_MAX);
    snapshot_matches(packet, 90000);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    release_references();
}

static void replace_captured_elements(void)
{
    CHECK(borrowed == 2);
    for (size_t i = 0; i < 2; ++i) java_previous->data.objects[i] = array_new(1, BYTES);
}

static void captured_sources(void)
{
    setup();
    input_allocation_hook = replace_captured_elements;
    const jlong id = open_review(100);
    CHECK(id > 0 && input_allocation_hook == NULL);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, id, 101), 89999);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    release_references();
}

static void source_bounds(void)
{
    setup();
    fake_array *first = java_previous->data.objects[0];
    const jsize length = first->length;
    first->length = 0;
    CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE && owned_inputs == NULL && !pending);
    first->length = (jsize)ZCL_V4_SOURCE_MAX + 1;
    CHECK(open_review(100) == -(jlong)ZCL_RESOURCE_EXHAUSTED && owned_inputs == NULL && !pending);
    first->length = length;
    java_previous->data.objects[1] = NULL;
    CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT && owned_inputs == NULL && !pending);
    release_references();
    setup();
    java_previous->length = (jsize)ZCL_TX_INPUT_MAX;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i)
        java_previous->data.objects[i] = array_new((jsize)ZCL_V4_SOURCE_MAX, BYTES);
    /* Draft/source count mismatch must refuse before copying any source. */
    CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT);
    fprintf(stderr, "Rejected source count: allocated=%zu copied=%zu\n", last_allocation_size, byte_reads);
    CHECK(last_allocation_size == 0 && byte_reads == (preparing ? 0 : draft_length));
    CHECK(source_clears == 1 && draft_clears == 1 && borrowed == 0 && owned_inputs == NULL);
    release_references();
}

static void exceptional_reference(void)
{
    const unsigned maximum = preparing ? 4 : 2;
    for (unsigned point = 1; point <= maximum; ++point) {
        setup();
        element_with_exception = true;
        fail_element = point;
        CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT);
        CHECK(pending && borrowed == 0 && owned_inputs == NULL);
        pending = false;
        release_references();
    }
}

static void draft_admission(void)
{
    for (unsigned malformed = 0; malformed < 3; ++malformed) {
        setup();
        for (size_t i = 0; i < fixture.spending.input_count; ++i)
            java_previous->data.objects[i]->length = (jsize)ZCL_V4_SOURCE_MAX;
        if (malformed == 0) java_draft->data.bytes[0] ^= 1;
        else java_draft->length = malformed == 1 ? 0 : (jsize)draft_length - 1;
        const zcl_status expected = malformed == 0 ? ZCL_UNSUPPORTED : ZCL_INVALID_ENCODING;
        CHECK(open_review(100) == -(jlong)expected);
        CHECK(last_allocation_size == 0 && byte_reads == (size_t)java_draft->length);
        CHECK(transaction_clears == 1 && !pending && borrowed == 0 && owned_inputs == NULL);
        release_references();
    }
}

static void destroy_java_sources(void)
{
    CHECK(borrowed == 0 && owned_inputs != NULL);
    for (size_t i = 0; i < fixture.spending.input_count; ++i)
        memset(java_previous->data.objects[i]->data.bytes, 0xff,
            (size_t)java_previous->data.objects[i]->length);
}

static void prepared_copy_lifetime(void)
{
    setup();
    construction_hook = destroy_java_sources;
    const jlong id = open_review(100);
    CHECK(id > 0 && construction_hook == NULL);
    CHECK(request_clears == 1 && transaction_clears == 1 && parameter_clears == 1 && address_clears == 2);
    snapshot_matches((fake_array *)snapshot_read(&vm, NULL, id, 100), 90000);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    release_references();
    setup();
    fail_construction = true;
    CHECK(open_review(100) == -(jlong)ZCL_CRYPTO_FAILURE);
    CHECK(transaction_clears == 1 && request_clears == 1 && owned_inputs == NULL);
    fail_construction = false;
    const jlong next = open_review(100);
    CHECK(next > id && API(cancelReview)(&vm, NULL, next) == ZCL_OK);
    release_references();
}

static void preparation_fields(void)
{
    for (size_t field = 0; field < 9; ++field) for (unsigned edge = 0; edge < 2; ++edge) {
        setup();
        java_parameters->data.numbers[field] = edge == 0 ? INT64_MIN : INT64_MAX;
        CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE && transaction_clears == 0);
        CHECK(last_allocation_size == 0 && byte_reads == 0);
        release_references();
    }
    for (jsize length = 1; length <= (jsize)ZCL_DRAFT_PARAMETER_MAX; ++length) {
        if (length == 9) continue;
        setup(); java_parameters->length = length;
        CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT && parameter_clears == 0);
        CHECK(last_allocation_size == 0 && byte_reads == 0);
        release_references();
    }
    setup(); java_parameters->data.numbers[2] = 499;
    CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE);
    release_references();
}

static void preparation_admission(void)
{
    for (unsigned malformed = 0; malformed < 2; ++malformed) {
        setup();
        java_previous->length = (jsize)ZCL_TX_INPUT_MAX;
        fake_array *source = java_previous->data.objects[0];
        source->length = (jsize)ZCL_V4_SOURCE_MAX;
        for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) java_previous->data.objects[i] = source;
        if (malformed != 0) {
            java_parameters->length = (jsize)(3 + 2 * ZCL_TX_INPUT_MAX + 2);
            java_parameters->data.numbers[0] = -1;
        }
        const jlong expected = malformed == 0 ? -(jlong)ZCL_INVALID_ARGUMENT : -(jlong)ZCL_OUT_OF_RANGE;
        CHECK(open_review(100) == expected);
        fprintf(stderr, "Rejected parameters: allocated=%zu copied=%zu\n", last_allocation_size, byte_reads);
        CHECK(last_allocation_size == 0 && byte_reads == 0);
        CHECK(!pending && owned_inputs == NULL && borrowed == 0 && request_clears == 1);
        release_references();
    }
}

static void preparation_arguments(void)
{
    setup();
    CHECK(API(prepareFullSourceReview)(NULL, NULL, (jobjectArray)java_previous,
        (jobjectArray)java_destinations, (jlongArray)java_parameters, 0, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    for (unsigned field = 0; field < 3; ++field)
        CHECK(API(prepareFullSourceReview)(&vm, NULL, field == 0 ? NULL : (jobjectArray)java_previous,
            field == 1 ? NULL : (jobjectArray)java_destinations,
            field == 2 ? NULL : (jlongArray)java_parameters, 0, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(prepareFullSourceReview)(&vm, NULL, (jobjectArray)java_previous,
        (jobjectArray)java_destinations, (jlongArray)java_parameters, 2, 100) == -(jlong)ZCL_UNSUPPORTED);
    CHECK(API(prepareFullSourceReview)(&vm, NULL, (jobjectArray)java_previous,
        (jobjectArray)java_destinations, (jlongArray)java_parameters, 1, 100) == -(jlong)ZCL_UNSUPPORTED);
    CHECK(open_review(-1) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(open_review(INT64_MAX - 89999) == -(jlong)ZCL_OUT_OF_RANGE);
    pending = true;
    CHECK(open_review(100) == -(jlong)ZCL_INVALID_ARGUMENT);
    pending = false;
    release_references();
}

static void profile_regressions(void)
{
    fee_admission();
    golden_and_lifetime(); opening_failures(); publication_failures();
    argument_failures(); maximum_packet(); publication_races();
}

static void regressions(void)
{
    profile_regressions();
    full_sources = true;
    profile_regressions(); captured_sources(); source_bounds(); exceptional_reference(); draft_admission();
    preparing = true;
    preparation_admission();
    golden_and_lifetime(); opening_failures(); publication_failures(); publication_races();
    maximum_packet(); captured_sources(); source_bounds(); exceptional_reference();
    prepared_copy_lifetime(); preparation_fields(); preparation_arguments();
    preparing = false;
    full_sources = false;
}

#ifndef ZCL_JNI_REVIEW_FUZZ
int main(void)
{
    regressions();
    puts("JNI unsigned review ownership, packet and cleanup checks passed");
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void fuzz_snapshot(jlong id, jlong now)
{
    const bool refusal = fail_new != 0 || fail_set;
    const bool exception = refusal && fail_new != 3;
    fake_array *packet = (fake_array *)snapshot_read(&vm, NULL, id, now);
    CHECK((packet == NULL) == refusal && pending == exception);
    if (packet == NULL) return;
    CHECK(!pending && packet->kind == NUMBERS && packet->length >= 1);
    if (packet->data.numbers[0] == ZCL_OK) {
        CHECK(now >= 100 && now < 90100);
        snapshot_matches(packet, 90100 - now);
    } else CHECK(packet->length == 1);
}

static void fuzz_wire(jlong id, jlong now)
{
    const bool refusal = fail_new != 0 || fail_set;
    const bool exception = refusal && fail_new != 3;
    fake_array *packet = (fake_array *)wire_read(&vm, NULL, id, now);
    CHECK((packet == NULL) == refusal && pending == exception);
    if (packet == NULL) return;
    CHECK(!pending && packet->kind == BYTES && packet->length >= 1);
    if (packet->data.bytes[0] == ZCL_OK) {
        CHECK(packet->length == (jsize)(draft_length + 1));
        CHECK(memcmp(packet->data.bytes + 1, draft, draft_length) == 0);
    } else CHECK(packet->length == 1);
}

static jlong fuzz_open(uint8_t mode, jlong current)
{
    java_previous->length = 2;
    java_draft->length = mode % 5 == 0 ? 0 : (jsize)draft_length;
    memcpy(java_draft->data.bytes, draft, draft_length);
    if (mode % 5 == 1) java_draft->data.bytes[0] ^= 1;
    if (preparing) {
        java_parameters->length = mode % 5 == 0 ? 0 : 9;
        java_parameters->data.numbers[2] = mode % 5 == 1 ? 499 : 500;
    }
    fail_malloc = mode % 5 == 2;
    fail_region = mode % 5 == 3 ? 2 : 0;
    const jlong next = open_review(100);
    if (next <= 0) return current;
    CHECK(next > current);
    return next;
}

static void clear_failures(void)
{
    CHECK(owned_inputs == NULL && borrowed == 0);
    pending = fail_malloc = fail_set = false;
    fail_new = 0;
    fail_length = fail_element = fail_region = 0;
}

static void fuzz_profile(uint8_t flags)
{
    preparing = (flags & 2U) != 0;
    full_sources = preparing || (flags & 1U) != 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 128) return 0;
    static bool tested;
    if (!tested) { regressions(); tested = true; }
    fuzz_profile(size == 0 ? 0 : data[0]);
    setup();
    jlong current = open_review(100), old = 0;
    CHECK(current > 0);
    for (size_t offset = 0; size - offset >= 3; offset += 3) {
        const uint8_t mode = data[offset];
        const jlong now = data[offset + 1] == 0 ? -1 : (jlong)data[offset + 1] * 500;
        const jlong selected = (mode & 8) == 0 ? current : old;
        switch (mode % 7) {
        case 0: fuzz_snapshot(selected, now); break;
        case 1: fuzz_wire(selected, now); break;
        case 2: (void)API(cancelReview)(&vm, NULL, selected); break;
        case 3: {
            const jlong next = fuzz_open(data[offset + 2], current);
            if (next != current) { old = current; current = next; }
            break;
        }
        case 4:
            fail_new = data[offset + 2] % 4; fail_set = !fail_new;
            fuzz_snapshot(selected, now); break;
        case 5:
            fail_new = data[offset + 2] % 4; fail_set = !fail_new;
            fuzz_wire(selected, now); break;
        default:
            java_previous->length = (jsize)(data[offset + 2] % 10);
            fail_length = 1 + data[offset + 1] % 4;
            (void)open_review(100); break;
        }
        clear_failures();
    }
    (void)API(cancelReview)(&vm, NULL, current);
    release_references();
    return 0;
}
#endif
