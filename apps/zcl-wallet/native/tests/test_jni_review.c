/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef malloc
#undef free
#include "jni_review_internal.h"
#include "assessment_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI review check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openReview)(JNIEnv *, jclass, jbyteArray, jobjectArray, jint, jlong, jlong);
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
        uint8_t bytes[ZCL_TX_WIRE_MAX + 1];
        jlong numbers[ZCL_REVIEW_PACKET_MAX];
        struct fake_array *objects[ZCL_TX_INPUT_MAX];
    } data;
} fake_array;
static fake_array *references[64];
static size_t reference_count, borrowed;
static bool pending, fail_malloc, fail_new, fail_set;
static unsigned fail_length, fail_element, fail_region;
static void (*allocation_hook)(void);
static void *owned_inputs;
static assessment_fixture fixture;
static uint8_t draft[ZCL_TX_WIRE_MAX];
static size_t draft_length;

void *zcl_jni_review_test_malloc(size_t size)
{
    CHECK(size == sizeof(zcl_jni_review_inputs) && owned_inputs == NULL);
    if (fail_malloc) { fail_malloc = false; return NULL; }
    owned_inputs = malloc(size);
    CHECK(owned_inputs != NULL);
    memset(owned_inputs, 0xa5, size);
    return owned_inputs;
}

void zcl_jni_review_test_free(void *pointer)
{
    CHECK(pointer != NULL && pointer == owned_inputs);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < sizeof(zcl_jni_review_inputs); ++i) CHECK(bytes[i] == 0);
    owned_inputs = NULL;
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
    const size_t maximum = kind == BYTES ? ZCL_TX_WIRE_MAX + 1 :
        (kind == NUMBERS ? ZCL_REVIEW_PACKET_MAX : ZCL_TX_INPUT_MAX);
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
    CHECK(allocation_hook == NULL);
    for (size_t i = 0; i < reference_count; ++i) { free(references[i]); references[i] = NULL; }
    reference_count = 0;
    pending = fail_malloc = fail_new = fail_set = false;
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
    CHECK(index >= 0 && index < array->length && (size_t)index < ZCL_TX_INPUT_MAX);
    if (fails(&fail_element)) return NULL;
    fake_array *element = array->data.objects[(size_t)index];
    if (element != NULL) ++borrowed;
    return (jobject)element;
}

static void JNICALL delete_reference(JNIEnv *env, jobject reference)
{
    (void)env;
    CHECK(reference != NULL && borrowed > 0);
    --borrowed; /* Allowed with a pending exception, as in the actual VM. */
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
    if (fails(&fail_region)) {
        if (count > 0) output[0] = 42; /* Partially written scratch must still clear. */
        return;
    }
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}

static jlongArray JNICALL new_numbers(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending);
    if (allocation_hook != NULL) {
        void (*hook)(void) = allocation_hook;
        allocation_hook = NULL;
        hook();
    }
    if (fail_new) { fail_new = false; pending = true; return NULL; }
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
    if (fail_new) { fail_new = false; pending = true; return NULL; }
    return (jbyteArray)array_new(length, BYTES);
}

static void JNICALL set_numbers(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, NUMBERS);
    memcpy(array->data.numbers + (size_t)offset, data, (size_t)count * sizeof(*data));
    if (fail_set) { fail_set = false; pending = true; }
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, BYTES);
    memcpy(array->data.bytes + (size_t)offset, data, (size_t)count);
    if (fail_set) { fail_set = false; pending = true; }
}

static const struct JNINativeInterface_ vm_table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetObjectArrayElement = get_element, .DeleteLocalRef = delete_reference,
    .GetByteArrayRegion = get_bytes, .NewLongArray = new_numbers,
    .SetLongArrayRegion = set_numbers, .NewByteArray = new_bytes,
    .SetByteArrayRegion = set_bytes
};
static JNIEnv vm = &vm_table;
static fake_array *java_draft, *java_previous;

static fake_array *bytes(const uint8_t *data, size_t length)
{
    CHECK(length <= ZCL_TX_WIRE_MAX + 1);
    fake_array *array = array_new((jsize)length, BYTES);
    memcpy(array->data.bytes, data, length);
    return array;
}

static void java_inputs(void)
{
    CHECK(zcl_transaction_serialize(&fixture.spending, draft, sizeof(draft), &draft_length) == ZCL_OK);
    java_draft = bytes(draft, draft_length);
    java_previous = array_new((jsize)fixture.spending.input_count, OBJECTS);
    for (size_t i = 0; i < fixture.spending.input_count; ++i)
        java_previous->data.objects[i] = bytes(fixture.sources[i].wire, fixture.sources[i].length);
}

static void setup(void)
{
    CHECK(reference_count == 0 && assessment_fixture_init(&fixture));
    fixture.spending.lock_time = UINT32_MAX;
    fixture.spending.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    fixture.spending.inputs[0].sequence = UINT32_C(0x80000000);
    java_inputs();
}

static jlong open_review(jlong now)
{
    return API(openReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        (jint)ZCL_MAINNET, 500, now);
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
        CHECK(zcl_transaction_prevout(&fixture.spending.inputs[i], fixture.sources[i].wire,
            fixture.sources[i].length, &previous) == ZCL_OK);
        destination_matches(&row[10], &previous);
    }
    for (size_t i = 0; i < outputs; ++i)
        destination_matches(&values[20 + 17 * count + 7 * i], &fixture.spending.outputs[i]);
}

static void snapshot_error(jlong id, jlong now, zcl_status status)
{
    fake_array *array = (fake_array *)API(reviewSnapshot)(&vm, NULL, id, now);
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
    snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, first, 101), 89999);
    fake_array *wire = (fake_array *)API(reviewWire)(&vm, NULL, first, 102);
    CHECK(wire != NULL && wire->kind == BYTES && wire->length == (jsize)(draft_length + 1));
    CHECK(wire->data.bytes[0] == ZCL_OK && memcmp(wire->data.bytes + 1, draft, draft_length) == 0);
    memset(wire->data.bytes, 0, (size_t)wire->length);
    snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, first, 103), 89997);
    CHECK(API(cancelReview)(&vm, NULL, first) == ZCL_OK);
    snapshot_error(first, 103, ZCL_CANCELLED);
    release_references();
    setup();
    const jlong second = open_review(200);
    CHECK(second > first);
    snapshot_error(first, INT64_MAX, ZCL_CANCELLED);
    CHECK(API(cancelReview)(&vm, NULL, first) == ZCL_CANCELLED);
    snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, second, 201), 89999);
    snapshot_error(second, 90200, ZCL_TIMED_OUT);
    CHECK(API(cancelReview)(&vm, NULL, second) == ZCL_CANCELLED);
    const jlong third = open_review(300);
    CHECK(third > second);
    snapshot_error(third, 299, ZCL_CANCELLED);
    release_references();
}

static void opening_failures(void)
{
    for (unsigned kind = 0; kind < 4; ++kind) {
        const unsigned maximum = kind == 0 ? 4 : (kind == 1 ? 2 : (kind == 2 ? 3 : 1));
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
    for (unsigned kind = 0; kind < 4; ++kind) {
        setup();
        const jlong id = open_review(100);
        CHECK(id > 0);
        fail_new = kind % 2 == 0;
        fail_set = !fail_new;
        if (kind < 2) CHECK(API(reviewSnapshot)(&vm, NULL, id, 101) == NULL);
        else CHECK(API(reviewWire)(&vm, NULL, id, 101) == NULL);
        CHECK(pending && borrowed == 0 && owned_inputs == NULL);
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
    for (unsigned kind = 0; kind < 2; ++kind) {
        setup();
        publication_old = open_review(100);
        CHECK(publication_old > 0);
        allocation_hook = replace_during_publication;
        fail_new = true;
        if (kind == 0) CHECK(API(reviewSnapshot)(&vm, NULL, publication_old, 101) == NULL);
        else CHECK(API(reviewWire)(&vm, NULL, publication_old, 101) == NULL);
        CHECK(pending && allocation_hook == NULL);
        pending = false;
        snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, publication_replacement, 100), 90000);
        CHECK(API(cancelReview)(&vm, NULL, publication_replacement) == ZCL_OK);
        release_references();
    }
}

static void argument_failures(void)
{
    setup();
    CHECK(API(openReview)(NULL, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(openReview)(&vm, NULL, NULL, (jobjectArray)java_previous, 0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(openReview)(&vm, NULL, (jbyteArray)java_draft, NULL, 0, 500, 100) == -(jlong)ZCL_INVALID_ARGUMENT);
    CHECK(API(openReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        2, 500, 100) == -(jlong)ZCL_UNSUPPORTED);
    CHECK(API(openReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
        0, -1, 100) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(openReview)(&vm, NULL, (jbyteArray)java_draft, (jobjectArray)java_previous,
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
    CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE);
    saved->length = original_length;
    java_draft->length = (jsize)ZCL_TX_WIRE_MAX + 1;
    CHECK(open_review(100) == -(jlong)ZCL_OUT_OF_RANGE);
    java_draft->length = (jsize)draft_length;
    const jlong id = open_review(100);
    CHECK(id > 0);
    CHECK(API(reviewSnapshot)(NULL, NULL, id, 100) == NULL);
    CHECK(API(reviewWire)(NULL, NULL, id, 100) == NULL);
    snapshot_error(id, -1, ZCL_OUT_OF_RANGE);
    snapshot_error(0, INT64_MAX, ZCL_CANCELLED);
    snapshot_error(-1, INT64_MAX, ZCL_CANCELLED);
    CHECK(API(cancelReview)(&vm, NULL, -1) == ZCL_CANCELLED);
    snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, id, 100), 90000);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    const jlong last = open_review(INT64_MAX - 90000);
    CHECK(last > id);
    snapshot_matches((fake_array *)API(reviewSnapshot)(&vm, NULL, last, INT64_MAX - 1), 1);
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
    fake_array *packet = (fake_array *)API(reviewSnapshot)(&vm, NULL, id, 100);
    CHECK(packet != NULL && packet->length == (jsize)ZCL_REVIEW_PACKET_MAX);
    snapshot_matches(packet, 90000);
    CHECK(API(cancelReview)(&vm, NULL, id) == ZCL_OK);
    release_references();
}

static void regressions(void)
{
    golden_and_lifetime(); opening_failures(); publication_failures();
    argument_failures(); maximum_packet(); publication_races();
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
    fake_array *packet = (fake_array *)API(reviewSnapshot)(&vm, NULL, id, now);
    if (packet == NULL) { CHECK(pending); return; }
    CHECK(!pending && packet->kind == NUMBERS && packet->length >= 1);
    if (packet->data.numbers[0] == ZCL_OK) {
        CHECK(now >= 100 && now < 90100);
        snapshot_matches(packet, 90100 - now);
    } else CHECK(packet->length == 1);
}

static void fuzz_wire(jlong id, jlong now)
{
    fake_array *packet = (fake_array *)API(reviewWire)(&vm, NULL, id, now);
    if (packet == NULL) { CHECK(pending); return; }
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
    pending = fail_malloc = fail_new = fail_set = false;
    fail_length = fail_element = fail_region = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 128) return 0;
    static bool tested;
    if (!tested) { regressions(); tested = true; }
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
            fail_new = (mode & 16) != 0; fail_set = !fail_new;
            fuzz_snapshot(selected, now); break;
        case 5:
            fail_new = (mode & 16) != 0; fail_set = !fail_new;
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
