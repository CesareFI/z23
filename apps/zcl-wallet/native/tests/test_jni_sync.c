/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Only the test target redirects the JNI translation unit's allocator calls.
 * The harness itself uses normal allocation; libFuzzer/VM internals are untouched. */
#undef malloc
#undef free
#undef zcl_secure_zero
#undef zcl_sync_watch_snapshot
#undef zcl_sync_watch_request
#include "jni_support.h"
#include "sync_fixture.h"
#include "zcl_sync_watch.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI sync check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jlong JNICALL API(openSyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jlong JNICALL API(openHistorySyncOwner)(JNIEnv *, jclass, jbyteArray, jint, jbyteArray);
JNIEXPORT jint JNICALL API(closeSyncOwner)(JNIEnv *, jclass, jlong);
JNIEXPORT jlong JNICALL API(beginSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(failSyncAttempt)(JNIEnv *, jclass, jlong, jlong, jint);
JNIEXPORT jbyteArray JNICALL API(syncRequest)(JNIEnv *, jclass, jlong, jlong, jlong);
JNIEXPORT jint JNICALL API(syncReply)(JNIEnv *, jclass, jlong, jlong, jlong, jbyteArray);
JNIEXPORT jlongArray JNICALL API(syncSnapshot)(JNIEnv *, jclass, jlong, jlong);
JNIEXPORT jlongArray JNICALL API(syncHistorySnapshot)(JNIEnv *, jclass, jlong, jlong);

/* Bounded fake VM local references for sanitizer/failure injection. This tests
 * native cleanup only; real JVM -Xcheck:jni and Android tests remain separate.
 * A failed JNI allocation/region operation leaves a pending fake exception.
 * Release all local references after each test/fuzz invocation, as a VM would.
 */
typedef struct {
    jsize length;
    bool longs;
    union { uint8_t bytes[16385]; jlong numbers[156]; } data;
} fake_array;
static fake_array *references[32];
static size_t reference_count;
static bool pending_exception, fail_set, fail_get, fail_frame;
static unsigned fail_new;
static uint8_t *owned_frame;
static uintptr_t snapshot_identity, output_identity;
static unsigned snapshot_calls, snapshot_clears, output_clears;
static unsigned snapshot_fault;
static size_t output_capacity;
static void (*allocation_hook)(void);
static uintptr_t request_identity, packet_identity;
static unsigned request_calls, request_clears, packet_clears;
static bool opening;
static unsigned opening_reads, address_clears, source_clears;
static unsigned fail_open_get;
static uintptr_t address_identity, source_identity;
static unsigned frame_allocations, byte_regions;

zcl_status zcl_jni_sync_test_snapshot(zcl_sync_watch *watch, uint64_t now, zcl_sync_snapshot *snapshot);
void zcl_jni_sync_test_zero(void *buffer, size_t length);
zcl_status zcl_jni_sync_test_request(zcl_sync_watch *watch, uint64_t token, uint64_t now,
    uint8_t *output, size_t capacity, size_t *length);

zcl_status zcl_jni_sync_test_request(zcl_sync_watch *watch, uint64_t token, uint64_t now,
    uint8_t *output, size_t capacity, size_t *length)
{
    CHECK(request_identity == 0 && packet_identity == 0);
    CHECK(request_calls == request_clears && request_calls == packet_clears);
    CHECK(capacity == ZCL_ELECTRUM_REQUEST_MAX);
    request_identity = (uintptr_t)watch;
    packet_identity = (uintptr_t)output - 1;
    ++request_calls;
    return zcl_sync_watch_request(watch, token, now, output, capacity, length);
}

static bool retire_request_copy(void *buffer, size_t length)
{
    if ((uintptr_t)buffer == request_identity) {
        CHECK(length == sizeof(zcl_sync_watch));
        request_identity = 0;
        ++request_clears;
        return true;
    }
    if ((uintptr_t)buffer == packet_identity) {
        CHECK(length == ZCL_ELECTRUM_REQUEST_MAX + 1);
        packet_identity = 0;
        ++packet_clears;
        return true;
    }
    return false;
}

static bool retire_open_copy(void *buffer, size_t length)
{
    if (!opening) return false;
    if (length == 35) {
        CHECK(address_identity == 0 || address_identity == (uintptr_t)buffer);
        address_identity = 0;
        ++address_clears;
    } else {
        CHECK(length == 32);
        CHECK(source_identity == 0 || source_identity == (uintptr_t)buffer);
        source_identity = 0;
        ++source_clears;
    }
    return true;
}

static zcl_status corrupt_snapshot(zcl_sync_snapshot *snapshot, zcl_status status)
{
    switch (snapshot_fault) {
    case 1: memset(snapshot, 0x5a, sizeof(*snapshot)); return ZCL_INVALID_ENCODING;
    case 2: snapshot->age_ms = UINT64_MAX; break;
    case 3: snapshot->report.balance.confirmed = UINT64_MAX; break;
    case 4: snapshot->report.balance.total = UINT64_MAX; break;
    case 5: snapshot->next_change_ms = UINT64_MAX; break;
    case 6: snapshot->report.history.count = ZCL_ELECTRUM_HISTORY_MAX + 1; break;
    case 7: snapshot->report.has_history = false; snapshot->report.history.count = 1; break;
    default: break;
    }
    return status;
}

zcl_status zcl_jni_sync_test_snapshot(zcl_sync_watch *watch, uint64_t now, zcl_sync_snapshot *snapshot)
{
    CHECK(snapshot_identity == 0 && snapshot_calls == snapshot_clears);
    snapshot_identity = (uintptr_t)snapshot;
    ++snapshot_calls;
    const zcl_status status = zcl_sync_watch_snapshot(watch, now, snapshot);
    return corrupt_snapshot(snapshot, status);
}

void zcl_jni_sync_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    const uint8_t *data = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(data[i] == 0);
    if (retire_open_copy(buffer, length)) return;
    if (retire_request_copy(buffer, length)) return;
    if (buffer == owned_frame) {
        CHECK(length == ZCL_ELECTRUM_FRAME_MAX);
    } else if ((uintptr_t)buffer == snapshot_identity) {
        CHECK(length == sizeof(zcl_sync_snapshot));
        snapshot_identity = 0;
        ++snapshot_clears;
    } else {
        CHECK(length == output_capacity && output_capacity != 0);
        CHECK(snapshot_identity == 0 && snapshot_calls == snapshot_clears);
        CHECK(output_identity == 0 || output_identity == (uintptr_t)buffer);
        output_identity = 0;
        ++output_clears;
    }
}

void *zcl_jni_test_malloc(size_t size)
{
    if (size != ZCL_ELECTRUM_FRAME_MAX) return malloc(size);
    CHECK(owned_frame == NULL);
    ++frame_allocations;
    if (fail_frame) { fail_frame = false; return NULL; }
    owned_frame = malloc(size);
    return owned_frame;
}
void zcl_jni_test_free(void *pointer)
{
    if (pointer != NULL && pointer == owned_frame) {
        for (size_t i = 0; i < ZCL_ELECTRUM_FRAME_MAX; ++i) CHECK(owned_frame[i] == 0);
        owned_frame = NULL;
    }
    free(pointer);
}

static fake_array *array_new(jsize length, bool longs)
{
    CHECK(length >= 0 && length <= (longs ? 156 : 16385));
    CHECK(reference_count < sizeof(references) / sizeof(references[0]));
    fake_array *array = calloc(1, sizeof(*array));
    CHECK(array != NULL);
    array->length = length;
    array->longs = longs;
    references[reference_count++] = array;
    return array;
}

static jbyteArray bytes(const uint8_t *data, size_t length)
{
    CHECK(length <= 16385);
    fake_array *array = array_new((jsize)length, false);
    if (length != 0) memcpy(array->data.bytes, data, length);
    return (jbyteArray)array;
}

static void release_references(void)
{
    CHECK(owned_frame == NULL);
    CHECK(!opening && address_identity == 0 && source_identity == 0);
    address_clears = source_clears = opening_reads = 0;
    fail_open_get = 0;
    CHECK(request_identity == 0 && packet_identity == 0);
    CHECK(request_calls == request_clears && request_calls == packet_clears);
    request_calls = request_clears = packet_clears = 0;
    CHECK(snapshot_identity == 0 && output_identity == 0 && output_capacity == 0);
    CHECK(snapshot_calls == snapshot_clears && allocation_hook == NULL);
    snapshot_calls = snapshot_clears = output_clears = 0;
    snapshot_fault = 0;
    for (size_t i = 0; i < reference_count; ++i) { free(references[i]); references[i] = NULL; }
    reference_count = 0;
    pending_exception = false;
    fail_new = false; fail_set = false; fail_get = false; fail_frame = false;
    frame_allocations = byte_regions = 0;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending_exception ? JNI_TRUE : JNI_FALSE;
}
static jsize JNICALL array_length(JNIEnv *env, jarray input)
{
    (void)env;
    CHECK(input != NULL && !pending_exception);
    return ((fake_array *)input)->length;
}
static fake_array *region(jarray input, jsize offset, jsize count, bool longs)
{
    fake_array *array = (fake_array *)input;
    CHECK(array != NULL && array->longs == longs && !pending_exception);
    CHECK(offset >= 0 && count >= 0 && offset <= array->length && count <= array->length - offset);
    return array;
}
static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, jbyte *output)
{
    (void)env;
    fake_array *array = region(input, offset, count, false);
    ++byte_regions;
    if (opening) {
        if (opening_reads++ == 0) address_identity = (uintptr_t)output;
        else source_identity = (uintptr_t)output;
        if (opening_reads == fail_open_get) fail_get = true;
    }
    if (fail_get) {
        if (count > 0) output[0] = 42;
        fail_get = false;
        pending_exception = true;
        return;
    }
    memcpy(output, array->data.bytes + (size_t)offset, (size_t)count);
}
static void JNICALL set_bytes(JNIEnv *env, jbyteArray input, jsize offset, jsize count, const jbyte *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, false);
    memcpy(array->data.bytes + (size_t)offset, data, (size_t)count);
    if (fail_set) { fail_set = false; pending_exception = true; }
}
static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending_exception);
    const unsigned fault = fail_new;
    fail_new = 0;
    if (fault != 0) {
        pending_exception = fault != 3;
        if (fault != 2) return NULL;
    }
    return (jbyteArray)array_new(length, false);
}
static jlongArray JNICALL new_longs(JNIEnv *env, jsize length)
{
    (void)env;
    CHECK(!pending_exception);
    CHECK(snapshot_identity == 0 && snapshot_calls == snapshot_clears);
    if (allocation_hook != NULL) {
        void (*hook)(void) = allocation_hook;
        allocation_hook = NULL;
        hook();
    }
    const unsigned fault = fail_new;
    fail_new = 0;
    if (fault != 0) {
        pending_exception = fault != 3;
        if (fault != 2) return NULL;
    }
    return (jlongArray)array_new(length, true);
}
static void JNICALL set_longs(JNIEnv *env, jlongArray input, jsize offset, jsize count, const jlong *data)
{
    (void)env;
    fake_array *array = region(input, offset, count, true);
    CHECK(output_identity == 0);
    output_identity = (uintptr_t)data;
    memcpy(array->data.numbers + (size_t)offset, data, (size_t)count * sizeof(*data));
    if (fail_set) { fail_set = false; pending_exception = true; }
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .SetByteArrayRegion = set_bytes,
    .NewByteArray = new_bytes, .NewLongArray = new_longs, .SetLongArrayRegion = set_longs
};
static JNIEnv environment = &table;

static jlong checked_open(JNIEnv *env, jbyteArray address, jint chain, jbyteArray source, bool history)
{
    CHECK(!opening && address_identity == 0 && source_identity == 0);
    const unsigned before_address = address_clears, before_source = source_clears;
    opening = true;
    opening_reads = 0;
    const jlong id = history ? API(openHistorySyncOwner)(env, NULL, address, chain, source)
        : API(openSyncOwner)(env, NULL, address, chain, source);
    CHECK(address_clears == before_address + 1 && source_clears == before_source + 1);
    CHECK(address_identity == 0 && source_identity == 0);
    opening = false;
    return id;
}

static jbyteArray checked_request(JNIEnv *env, jclass type, jlong id, jlong token, jlong now)
{
    CHECK(request_identity == 0 && packet_identity == 0);
    jbyteArray result = API(syncRequest)(env, type, id, token, now);
    CHECK(request_identity == 0 && packet_identity == 0);
    CHECK(request_calls == request_clears && request_calls == packet_clears);
    return result;
}

static jlongArray read_snapshot(JNIEnv *env, jclass type, jlong id, jlong now, bool history)
{
    CHECK(output_capacity == 0 && output_identity == 0);
    const unsigned before = output_clears;
    const unsigned expected = env != NULL && !pending_exception ? 1U : 0U;
    output_capacity = (history ? 156U : 10U) * sizeof(jlong);
    jlongArray result = history ? API(syncHistorySnapshot)(env, type, id, now)
        : API(syncSnapshot)(env, type, id, now);
    CHECK(output_clears == before + expected && output_identity == 0);
    CHECK(snapshot_identity == 0 && snapshot_calls == snapshot_clears);
    output_capacity = 0;
    return result;
}

static jlongArray checked_snapshot(JNIEnv *env, jclass type, jlong id, jlong now)
{
    return read_snapshot(env, type, id, now, false);
}

static jlongArray checked_history(JNIEnv *env, jclass type, jlong id, jlong now)
{
    return read_snapshot(env, type, id, now, true);
}

static jlong open_owner_mode(bool history)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    const uint8_t source[32] = {1};
    const jbyteArray address = bytes(fixture.candidate.address, 35);
    const jbyteArray source_input = bytes(source, sizeof(source));
    const jlong id = checked_open(&environment, address, (jint)ZCL_MAINNET, source_input, history);
    CHECK(id > 0 && !pending_exception);
    return id;
}

static jlong open_owner(void)
{
    return open_owner_mode(false);
}

static const jlong *snapshot(jlong id, jlong now)
{
    fake_array *result = (fake_array *)checked_snapshot(&environment, NULL, id, now);
    CHECK(result != NULL && result->longs && result->length == 10 && !pending_exception);
    CHECK(result->data.numbers[9] >= 0 && result->data.numbers[9] <= (jlong)ZCL_SYNC_FRESH_MS);
    return result->data.numbers;
}

static const jlong *history_snapshot(jlong id, jlong now)
{
    fake_array *result = (fake_array *)checked_history(&environment, NULL, id, now);
    CHECK(result != NULL && result->longs && result->length >= 12 && result->length <= 156 && !pending_exception);
    const jlong *values = result->data.numbers;
    CHECK(values[10] >= 0 && values[10] <= 1 && values[11] >= 0 && values[11] <= 16);
    CHECK(result->length == 12 + values[11] * 9);
    if (values[10] == 0) CHECK(values[11] == 0);
    for (size_t i = 0; i < (size_t)values[11]; ++i) {
        for (size_t word = 0; word < 8; ++word)
            CHECK(values[12 + i * 9 + word] >= 0 && values[12 + i * 9 + word] <= UINT32_MAX);
        CHECK(values[20 + i * 9] >= -1 && values[20 + i * 9] <= INT32_MAX);
    }
    return values;
}

static void refuse_pending_snapshots(jlong id, jlong now)
{
    CHECK(!pending_exception);
    const size_t before = reference_count;
    pending_exception = true;
    CHECK(checked_snapshot(&environment, NULL, id, now) == NULL);
    CHECK(checked_history(&environment, NULL, id, now) == NULL);
    CHECK(pending_exception && reference_count == before);
    pending_exception = false;
}

#ifndef ZCL_JNI_FUZZ
static jlong opening_fault(unsigned mode, bool history)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    uint8_t source[32] = {1};
    jbyteArray address = bytes(fixture.candidate.address, 35);
    jbyteArray source_input = bytes(source, sizeof(source));
    JNIEnv *env = &environment;
    jint chain = (jint)ZCL_MAINNET;
    switch (mode) {
    case 0: env = NULL; break;
    case 1: pending_exception = true; break;
    case 2: address = NULL; break;
    case 3: source_input = NULL; break;
    case 4: chain = 2; break;
    case 5: ((fake_array *)address)->length = 36; break;
    case 6: ((fake_array *)source_input)->length = 33; break;
    case 7: ((fake_array *)address)->data.bytes[0] = 0xff; break;
    default: fail_open_get = mode - 7; break;
    }
    return checked_open(env, address, chain, source_input, history);
}

static void opening_refusals(void)
{
    for (unsigned mode = 0; mode < 10; ++mode) for (unsigned history = 0; history < 2; ++history) {
        CHECK(opening_fault(mode, history != 0) < 0);
        CHECK(pending_exception == (mode == 1 || mode >= 8));
        pending_exception = false;
        fail_open_get = 0;
        const jlong id = open_owner_mode(history != 0);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static void snapshot_projection_refusals(void)
{
    for (unsigned mode = 1; mode <= 7; ++mode) {
        const jlong id = open_owner_mode(true);
        snapshot_fault = mode;
        fake_array *result = (fake_array *)checked_history(&environment, NULL, id, 0);
        const jlong expected = mode == 1 || mode == 7 ? ZCL_INVALID_ENCODING : ZCL_OUT_OF_RANGE;
        CHECK(result != NULL && result->length == 12 && result->data.numbers[0] == expected);
        if (mode <= 5) {
            result = (fake_array *)checked_snapshot(&environment, NULL, id, 0);
            CHECK(result != NULL && result->length == 10 && result->data.numbers[0] == expected);
        }
        snapshot_fault = 0;
        CHECK(snapshot(id, 0)[0] == ZCL_OK);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static jlong publication_old, publication_replacement;

static void replace_during_publication(void)
{
    CHECK(API(closeSyncOwner)(&environment, NULL, publication_old) == ZCL_OK);
    publication_replacement = open_owner_mode(true);
    CHECK(publication_replacement > publication_old);
}

static void snapshot_publication_races(void)
{
    for (unsigned mode = 0; mode < 8; ++mode) {
        publication_old = open_owner_mode(true);
        CHECK(API(beginSyncAttempt)(&environment, NULL, publication_old, 100, 10, 1) > 0);
        allocation_hook = replace_during_publication;
        fail_new = mode / 2;
        fail_set = fail_new == 0;
        CHECK(read_snapshot(&environment, NULL, publication_old, 101, mode % 2 != 0) == NULL);
        CHECK(pending_exception == (mode / 2 != 3) && allocation_hook == NULL);
        pending_exception = false;
        const jlong *state = history_snapshot(publication_replacement, 100);
        CHECK(state[0] == ZCL_OK && state[2] == 0 && state[3] == ZCL_OK && state[10] == 0);
        CHECK(snapshot(publication_old, INT64_MAX)[0] == ZCL_CANCELLED);
        CHECK(API(closeSyncOwner)(&environment, NULL, publication_replacement) == ZCL_OK);
        release_references();
    }
}

static void request_allocation_and_region_failure(void)
{
    const jlong id = open_owner();
    for (unsigned mode = 0; mode < 4; ++mode) {
        const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
        CHECK(token > 0);
        fail_new = mode;
        fail_set = mode == 0;
        CHECK(checked_request(&environment, NULL, id, token, 0) == NULL);
        CHECK(pending_exception == (mode != 3));
        pending_exception = false; /* Simulated VM catches the allocation/region exception. */
        const jlong *state = snapshot(id, 0);
        CHECK(state[0] == ZCL_OK && state[1] == 0 && state[2] == 0 && state[3] == ZCL_RESOURCE_EXHAUSTED);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
}

static void stale_request_failure_preserves_attempt(void)
{
    for (unsigned mode = 0; mode < 4; ++mode) {
        const jlong id = open_owner();
        const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 100, 10, 1);
        CHECK(token > 0);
        fail_new = mode;
        fail_set = mode == 0;
        CHECK(checked_request(&environment, NULL, id, token + 1, INT64_MAX) == NULL);
        CHECK(pending_exception == (mode != 3));
        pending_exception = false;
        const jlong *state = snapshot(id, 100);
        CHECK(state[0] == ZCL_OK && state[2] == 1 && state[3] == ZCL_OK && state[9] == 10);
        CHECK(checked_request(&environment, NULL, id, token, 100) != NULL);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static void frame_allocation_and_region_failure(void)
{
    const jlong id = open_owner();
    const jbyteArray frame = bytes((const uint8_t *)"{}", 2);
    for (unsigned mode = 0; mode < 2; ++mode) {
        const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
        CHECK(token > 0 && checked_request(&environment, NULL, id, token, 0) != NULL);
        if (mode == 0) fail_frame = true; else fail_get = true;
        const jint expected = mode == 0 ? (jint)ZCL_RESOURCE_EXHAUSTED : (jint)ZCL_INVALID_ARGUMENT;
        CHECK(API(syncReply)(&environment, NULL, id, token, 0, frame) == expected);
        CHECK(pending_exception == (mode == 1));
        pending_exception = false;
        CHECK(snapshot(id, 0)[3] == expected && owned_frame == NULL);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
}

static void retired_reply_batch(jlong id, jlong token, jbyteArray frame)
{
    jlong before[10];
    memcpy(before, snapshot(id, 100), sizeof(before));
    const unsigned allocations = frame_allocations, reads = byte_regions;
    for (unsigned i = 0; i < 64; ++i)
        CHECK(API(syncReply)(&environment, NULL, id, token, INT64_MAX, frame) == ZCL_CANCELLED);
    CHECK(!pending_exception && owned_frame == NULL);
    CHECK(memcmp(before, snapshot(id, 100), sizeof(before)) == 0);
    if (frame_allocations != allocations || byte_regions != reads)
        fprintf(stderr, "Retired reply batch: allocations=%u byte_regions=%u (expected zero)\n",
            frame_allocations - allocations, byte_regions - reads);
    CHECK(frame_allocations == allocations && byte_regions == reads);
}

static void retired_replies_need_no_frame_allocation_or_copy(void)
{
    for (unsigned history = 0; history < 2; ++history) {
        const jlong id = open_owner_mode(history != 0);
        const jbyteArray frame = (jbyteArray)array_new((jsize)ZCL_ELECTRUM_FRAME_MAX, false);
        retired_reply_batch(id, 1, frame); /* No attempt has been issued. */
        const jlong old = API(beginSyncAttempt)(&environment, NULL, id, 100, 100, 1);
        CHECK(old > 0);
        CHECK(API(failSyncAttempt)(&environment, NULL, id, old, ZCL_CANCELLED) == ZCL_CANCELLED);
        retired_reply_batch(id, old, frame);
        const jlong current = API(beginSyncAttempt)(&environment, NULL, id, 100, 100, 1);
        CHECK(current > old && checked_request(&environment, NULL, id, current, 100) != NULL);
        retired_reply_batch(id, old, frame); /* A future timestamp cannot expire the replacement. */
        char reply[512];
        const size_t length = sync_fixture_reply(ZCL_MAINNET, ZCL_SYNC_VERSION, 1, reply, sizeof(reply));
        const unsigned allocations = frame_allocations, reads = byte_regions;
        CHECK(API(syncReply)(&environment, NULL, id, current, 101,
            bytes((const uint8_t *)reply, length)) == ZCL_OK);
        CHECK(frame_allocations == allocations + 1 && byte_regions == reads + 1);
        CHECK(checked_request(&environment, NULL, id, current, 101) != NULL);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static void pending_snapshot_reads(void)
{
    for (unsigned mode = 0; mode < 2; ++mode) {
        const jlong id = open_owner_mode(mode != 0);
        CHECK(API(beginSyncAttempt)(&environment, NULL, id, 100, 10, 1) > 0);
        refuse_pending_snapshots(id, 110);
        /* Refused VM entries must not advance/expire the native owner clock. */
        const jlong *state = snapshot(id, 109);
        CHECK(state[0] == ZCL_OK && state[2] == 1 && state[3] == ZCL_OK && state[9] == 1);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static void pending_begin_preserves_owner(void)
{
    for (unsigned mode = 0; mode < 2; ++mode) {
        const jlong id = open_owner();
        const size_t before = reference_count;
        if (mode != 0) pending_exception = true;
        JNIEnv *env = mode == 0 ? NULL : &environment;
        CHECK(API(beginSyncAttempt)(env, NULL, id, 100, 10, 1)
            == -(jlong)ZCL_INVALID_ARGUMENT);
        CHECK(reference_count == before && pending_exception == (mode != 0));
        pending_exception = false;
        const jlong *state = snapshot(id, 100);
        CHECK(state[0] == ZCL_OK && state[2] == 0 && state[3] == ZCL_OK && state[9] == 0);
        CHECK(API(beginSyncAttempt)(&environment, NULL, id, 100, 10, 1) == 1);
        CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
        release_references();
    }
}

static void pending_request_reply_preserve_attempt(void)
{
    const jlong id = open_owner();
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 100, 10, 1);
    CHECK(token > 0);
    const size_t before = reference_count;

    CHECK(checked_request(NULL, NULL, id, token, 101) == NULL);
    CHECK(reference_count == before);
    pending_exception = true;
    CHECK(checked_request(&environment, NULL, id, token, 101) == NULL);
    CHECK(pending_exception && reference_count == before && owned_frame == NULL);
    pending_exception = false;
    const jlong *state = snapshot(id, 100);
    CHECK(state[0] == ZCL_OK && state[2] == 1 && state[3] == ZCL_OK && state[9] == 10);

    CHECK(checked_request(&environment, NULL, id, token, 101) != NULL);
    char reply[512] = {0};
    const size_t reply_len = sync_fixture_reply(ZCL_MAINNET, ZCL_SYNC_VERSION, 1,
        reply, sizeof(reply));
    const jbyteArray frame = bytes((const uint8_t *)reply, reply_len);
    pending_exception = true;
    CHECK(API(syncReply)(&environment, NULL, id, token, 102, frame) == ZCL_INVALID_ARGUMENT);
    CHECK(pending_exception && owned_frame == NULL);
    pending_exception = false;
    state = snapshot(id, 101);
    CHECK(state[0] == ZCL_OK && state[2] == 1 && state[3] == ZCL_OK && state[9] == 9);
    CHECK(API(syncReply)(NULL, NULL, id, token, 102, frame) == ZCL_INVALID_ARGUMENT);
    state = snapshot(id, 101);
    CHECK(state[0] == ZCL_OK && state[2] == 1 && state[3] == ZCL_OK && state[9] == 9);

    CHECK(API(syncReply)(&environment, NULL, id, token, 102, frame) == ZCL_OK);
    CHECK(checked_request(&environment, NULL, id, token, 102) != NULL);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
}

static void snapshot_failure_preserves_timeout(void)
{
    const jlong id = open_owner();
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 1, 1) > 0);
    CHECK(checked_snapshot(NULL, NULL, id, 1) == NULL);
    CHECK(snapshot(id, 0)[9] == 1); /* NULL env did not advance/expire the owner. */
    fail_new = true;
    CHECK(checked_snapshot(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    const jlong *state = snapshot(id, 1);
    CHECK(state[0] == ZCL_OK && state[2] == 0 && state[3] == ZCL_TIMED_OUT && state[9] == 0);
    fail_set = true;
    CHECK(checked_snapshot(&environment, NULL, id, 1) == NULL && pending_exception);
    pending_exception = false;
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(snapshot(id, 1)[0] == ZCL_CANCELLED);
    release_references();
}

static void history_snapshot_failures(void)
{
    const jlong id = open_owner_mode(true);
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 100, UINT32_MAX - 5) == -(jlong)ZCL_OUT_OF_RANGE);
    CHECK(API(beginSyncAttempt)(&environment, NULL, id, 0, 1, UINT32_MAX - 6) > 0);
    CHECK(checked_history(NULL, NULL, id, 1) == NULL);
    CHECK(history_snapshot(id, 0)[9] == 1);
    for (unsigned mode = 0; mode < 2; ++mode) {
        if (mode == 0) fail_new = true; else fail_set = true;
        CHECK(checked_history(&environment, NULL, id, 1) == NULL && pending_exception);
        pending_exception = false;
        const jlong *state = history_snapshot(id, 1);
        CHECK(state[0] == ZCL_OK && state[3] == ZCL_TIMED_OUT && state[10] == 0);
    }
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(history_snapshot(id, 1)[0] == ZCL_CANCELLED);
    release_references();
}

static size_t maximum_history(char *frame, size_t capacity)
{
    static const char prefix[] = "{\"id\":6,\"result\":[";
    CHECK(capacity > sizeof(prefix));
    size_t used = sizeof(prefix) - 1;
    memcpy(frame, prefix, used);
    for (unsigned i = 0; i < 16; ++i) {
        const int count = snprintf(frame + used, capacity - used,
            "%s{\"tx_hash\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff%08x\",\"height\":%d}",
            i == 0 ? "" : ",", i, i % 2 == 0 ? 0 : -1);
        CHECK(count > 0 && (size_t)count < capacity - used);
        used += (size_t)count;
    }
    CHECK(capacity - used >= 2);
    memcpy(frame + used, "]}", 2);
    return used + 2;
}

static void history_packet_and_owner_replacement(void)
{
    const jlong id = open_owner_mode(true);
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
    CHECK(token > 0);
    release_references();
    for (unsigned step = 1; step <= 7; ++step) {
        CHECK(checked_request(&environment, NULL, id, token, step) != NULL);
        static char frame[4096];
        const unsigned phase = step == 6 ? ZCL_SYNC_HISTORY : (step == 7 ? ZCL_SYNC_TIP_AFTER : step);
        const size_t length = step == 6 ? maximum_history(frame, sizeof(frame))
            : sync_fixture_reply(ZCL_MAINNET, phase, step, frame, sizeof(frame));
        CHECK(API(syncReply)(&environment, NULL, id, token, step, bytes((const uint8_t *)frame, length)) == ZCL_OK);
        const jlong *state = history_snapshot(id, step);
        CHECK(state[10] == (step == 7 ? 1 : 0));
        release_references();
    }
    const jlong *state = history_snapshot(id, 7);
    CHECK(state[0] == ZCL_OK && state[1] == ZCL_BALANCE_UNVERIFIED && state[7] == 993 && state[11] == 16);
    for (size_t i = 0; i < 16; ++i) {
        for (size_t word = 0; word < 7; ++word) CHECK(state[12 + i * 9 + word] == UINT32_MAX);
        CHECK(state[19 + i * 9] == (jlong)i && state[20 + i * 9] == (i % 2 == 0 ? 0 : -1));
    }
    CHECK(history_snapshot(id, 60007)[1] == ZCL_BALANCE_STALE);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    release_references();
    const jlong replacement = open_owner();
    CHECK(replacement > id && history_snapshot(replacement, 60007)[10] == 0);
    CHECK(API(failSyncAttempt)(&environment, NULL, id, token, ZCL_IO_FAILURE) == ZCL_CANCELLED);
    CHECK(history_snapshot(id, 60007)[0] == ZCL_CANCELLED);
    CHECK(API(closeSyncOwner)(&environment, NULL, replacement) == ZCL_OK);
    release_references();
}

int main(void)
{
    opening_refusals();
    stale_request_failure_preserves_attempt();
    snapshot_projection_refusals();
    snapshot_publication_races();
    request_allocation_and_region_failure(); frame_allocation_and_region_failure();
    retired_replies_need_no_frame_allocation_or_copy();
    pending_snapshot_reads(); pending_begin_preserves_owner();
    pending_request_reply_preserve_attempt();
    snapshot_failure_preserves_timeout();
    history_snapshot_failures(); history_packet_and_owner_replacement();
    puts("JNI sync: allocation/region exceptions, full frame clearing, slot cleanup and retained timeout passed");
    return 0;
}
#else
static jbyteArray fuzz_request(jlong id, jlong token, jlong now, uint8_t mode)
{
    fail_new = (mode & 0x80) != 0 ? 1U + (mode >> 1) % 3 : 0;
    fail_set = (mode & 0x40) != 0;
    const bool refusal = fail_new != 0 || fail_set;
    const bool exception = refusal && fail_new != 3;
    jbyteArray result = checked_request(&environment, NULL, id, token, now);
    CHECK((result == NULL) == refusal && pending_exception == exception);
    return result;
}

static void fuzz_snapshot_failure(jlong id, jlong now, uint8_t mode)
{
    fail_new = (mode >> 1) % 4;
    fail_set = fail_new == 0;
    const bool exception = fail_new != 3;
    CHECK(read_snapshot(&environment, NULL, id, now, (mode & 1) != 0) == NULL);
    CHECK(pending_exception == exception);
    pending_exception = false;
}

static void advance_fuzz_owner(jlong id, jlong token, bool history, unsigned step)
{
    for (unsigned n = 1; n < step; ++n) {
        CHECK(checked_request(&environment, NULL, id, token, (jlong)n) != NULL);
        static char frame[4096]; /* Single-threaded public fuzz fixture only. */
        const unsigned phase = history && n == 6 ? ZCL_SYNC_HISTORY : (history && n == 7 ? ZCL_SYNC_TIP_AFTER : n);
        const size_t count = sync_fixture_reply(ZCL_MAINNET, phase, n, frame, sizeof(frame));
        CHECK(API(syncReply)(&environment, NULL, id, token, (jlong)n,
            bytes((const uint8_t *)frame, count)) == ZCL_OK);
    }
}

static void inspect_fuzz_owner(jlong id, bool history, unsigned step)
{
    const jlong *state = snapshot(id, (jlong)step);
    CHECK(state[0] == ZCL_OK && state[1] >= 0 && state[1] <= 2);
    const jlong *complete = history_snapshot(id, (jlong)step);
    CHECK(complete[0] == ZCL_OK && complete[1] == state[1]);
    if (complete[10] != 0) CHECK(history && step == 7 && complete[11] == 2);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
    if (length == 0 || length > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    const bool history = (data[0] & 8) != 0;
    const jlong id = history ? open_owner_mode(true) : open_owner();
    const jlong token = API(beginSyncAttempt)(&environment, NULL, id, 0, 100, 1);
    CHECK(token > 0);
    const unsigned step = (unsigned)data[0] % (history ? 7u : 6u) + 1;
    advance_fuzz_owner(id, token, history, step);
    jbyteArray request = fuzz_request(id, token, (jlong)step, data[0]);
    pending_exception = false; fail_new = false; fail_set = false;
    if (request != NULL) {
        const jbyteArray input = bytes(data + 1, length - 1);
        fail_get = (data[0] & 0x20) != 0;
        fail_frame = (data[0] & 0x10) != 0;
        const jint status = API(syncReply)(&environment, NULL, id, token, (jlong)step, input);
        CHECK(status >= ZCL_OK && status <= ZCL_TLS_FAILURE);
        pending_exception = false; fail_get = false; fail_frame = false;
    }
    refuse_pending_snapshots(id, INT64_MAX);
    inspect_fuzz_owner(id, history, step);
    fuzz_snapshot_failure(id, (jlong)step, data[0]);
    CHECK(API(closeSyncOwner)(&environment, NULL, id) == ZCL_OK);
    CHECK(API(failSyncAttempt)(&environment, NULL, id, token, (jint)ZCL_CANCELLED) == ZCL_CANCELLED);
    release_references();
    return 0;
}
#endif
