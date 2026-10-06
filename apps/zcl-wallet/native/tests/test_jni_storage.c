/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_secure_zero
#undef zcl_wallet_change_create_owned
#include "jni_support.h"
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include "change_custody_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "JNI storage fault at%d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jint JNICALL API(createFreshWalletStorage)(JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray);
JNIEXPORT jint JNICALL API(createWalletStorage)(JNIEnv *, jclass, jbyteArray, jbyteArray);
JNIEXPORT jint JNICALL API(promoteWalletStorage)(JNIEnv *, jclass, jbyteArray, jbyteArray);
JNIEXPORT jbyteArray JNICALL API(readWalletStorage)(JNIEnv *, jclass, jbyteArray);

typedef struct { const uint8_t *bytes; size_t capacity; jsize length; } fake_array;
static fake_array path_array, record_array, entropy_array;
static uint8_t mutable_record[140], entropy_bytes[33];
static const change_storage_data *expected;
static void *secret_pointer;
static size_t vm_calls, fail_call, core_calls, zero_calls;
static bool pending, fail_core, mutate_record;
static bool malformed_record_case;
static bool live_entropy_at_storage;
static bool live_entropy_after_core, fail_random;
static unsigned storage_calls, entropy_reads, record_admission_clears;
/* Native spans are observed while live; only numeric identities survive a VM
 * call. Bits: path=1, record=2, entropy=4, publication packet=8. */
static uintptr_t identities[4];
static unsigned cleared, fail_new;
static bool fail_set;
static uint8_t published[142];
static fake_array publication;

zcl_status __real_zcl_storage_create_with_change(const uint8_t *, size_t,
    const uint8_t *, size_t, const uint8_t *, size_t);
zcl_status __wrap_zcl_storage_create_with_change(const uint8_t *, size_t,
    const uint8_t *, size_t, const uint8_t *, size_t);
zcl_status __real_zcl_random_bytes(uint8_t *, size_t);
zcl_status __wrap_zcl_random_bytes(uint8_t *, size_t);

zcl_status __wrap_zcl_random_bytes(uint8_t *output, size_t length)
{
    if (!fail_random) return __real_zcl_random_bytes(output, length);
    memset(output, 0x42, length); /* Provider may fill before reporting failure. */
    return ZCL_CRYPTO_FAILURE;
}

zcl_status __wrap_zcl_storage_create_with_change(const uint8_t *path, size_t path_len,
    const uint8_t *record, size_t record_len, const uint8_t *state, size_t state_len)
{
    if (secret_pointer != NULL) {
        ++storage_calls;
        const uint8_t *secret = secret_pointer; /* JNI-owned span is still live. */
        for (size_t i = 0; i < 32; ++i)
            if (secret[i] != 0) live_entropy_at_storage = true;
    }
    return __real_zcl_storage_create_with_change(path, path_len, record, record_len, state, state_len);
}

static bool vm_fault(void)
{
    REQUIRE(!pending);
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
    REQUIRE(input != NULL);
    return vm_fault() ? 0 : ((fake_array *)input)->length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize start, jsize length, jbyte *output)
{
    (void)env;
    const fake_array *array = (const fake_array *)input;
    REQUIRE(array != NULL && start == 0 && length >= 0 && (size_t)length <= array->capacity);
    REQUIRE(length == array->length && output != NULL);
    const size_t slot = array == &path_array ? 0 : array == &record_array ? 1 : 2;
    REQUIRE(identities[slot] == 0);
    identities[slot] = (uintptr_t)output;
    if (array == &entropy_array) {
        secret_pointer = output;
        ++entropy_reads;
    }
    if (vm_fault()) {
        if (length > 0) output[0] = 42; /* Partial VM read before exception. */
        return;
    }
    memcpy(output, array->bytes, (size_t)length);
    if (array == &entropy_array && mutate_record) {
        mutable_record[expected->wallet_len - 1] ^= 1;
        mutate_record = false;
    }
}

void zcl_jni_storage_test_zero(void *pointer, size_t length)
{
    REQUIRE(pointer != NULL);
    if (length == sizeof(zcl_wallet_record)) {
        zcl_secure_zero(pointer, length);
        const uint8_t *bytes = pointer;
        for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
        ++record_admission_clears;
        return;
    }
    const size_t slot = length == 1024 ? 0 : length == 140 ? 1 : length == 32 ? 2 : 3;
    REQUIRE(slot != 3 || length == 142);
    const unsigned bit = 1U << slot;
    REQUIRE((cleared & bit) == 0);
    REQUIRE(identities[slot] == 0 || identities[slot] == (uintptr_t)pointer);
    if (slot == 2) {
        REQUIRE(zero_calls == 0);
        if (secret_pointer != NULL) REQUIRE(secret_pointer == pointer);
        secret_pointer = NULL;
        ++zero_calls;
    }
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    identities[slot] = 0;
    cleared |= bit;
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize length)
{
    (void)env;
    REQUIRE(!pending && cleared == 1 && length > 0 && length <= (jsize)sizeof(published));
    publication = (fake_array){published, sizeof(published), length};
    /* 1=NULL+exception; 2=array+exception; 3=NULL without exception. */
    if (fail_new != 0) {
        pending = fail_new != 3;
        if (fail_new != 2) return NULL;
    }
    return (jbyteArray)&publication;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray output, jsize start, jsize length,
    const jbyte *bytes)
{
    (void)env;
    REQUIRE(!pending && output == (jbyteArray)&publication && start == 0);
    REQUIRE(length == publication.length && length > 0 && (size_t)length <= sizeof(published));
    REQUIRE(identities[3] == 0 && bytes != NULL);
    identities[3] = (uintptr_t)bytes;
    /* Even a partial publication must not escape as a successful result. */
    memcpy(published, bytes, fail_set ? 1 : (size_t)length);
    if (fail_set) pending = true;
}

zcl_status zcl_jni_storage_test_create(const uint8_t *path, size_t path_len,
    const uint8_t *record, size_t record_len, uint8_t *entropy, size_t entropy_len, size_t capacity)
{
    REQUIRE(!pending && core_calls == 0);
    ++core_calls;
    REQUIRE(path != path_array.bytes && record != record_array.bytes && entropy != entropy_array.bytes);
    REQUIRE(record_len == expected->wallet_len);
    REQUIRE(malformed_record_case ? memcmp(record, expected->wallet, record_len) != 0 :
        memcmp(record, expected->wallet, record_len) == 0);
    REQUIRE(entropy_len == (size_t)entropy_array.length && entropy == secret_pointer);
    REQUIRE(capacity == 32);
    if (fail_core) return ZCL_CRYPTO_FAILURE;
    const zcl_status status = zcl_wallet_change_create_owned(path, path_len, record, record_len,
        entropy, entropy_len, capacity);
    for (size_t i = 0; i < capacity; ++i)
        if (entropy[i] != 0) live_entropy_after_core = true;
    return status;
}

#if defined(__ANDROID__)
typedef struct JNINativeInterface storage_jni_interface;
#else
typedef struct JNINativeInterface_ storage_jni_interface;
#endif
static const storage_jni_interface fake_table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length, .GetByteArrayRegion = get_bytes,
    .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes
};
static JNIEnv fake_env = &fake_table;

static void reset(const storage_fixture *fixture, const change_storage_data *data)
{
    REQUIRE(secret_pointer == NULL);
    for (size_t i = 0; i < 4; ++i) REQUIRE(identities[i] == 0);
    cleared = fail_new = 0;
    fail_set = false;
    memset(published, 0xa5, sizeof(published));
    memcpy(mutable_record, data->wallet, data->wallet_len);
    memset(entropy_bytes, 0, sizeof(entropy_bytes));
    path_array = (fake_array){(const uint8_t *)fixture->path, fixture_path_len(), (jsize)fixture_path_len()};
    record_array = (fake_array){mutable_record, sizeof(mutable_record), (jsize)data->wallet_len};
    entropy_array = (fake_array){entropy_bytes, sizeof(entropy_bytes), 16};
    expected = data;
    vm_calls = fail_call = core_calls = zero_calls = 0;
    pending = fail_core = mutate_record = malformed_record_case = false;
    live_entropy_at_storage = false;
    live_entropy_after_core = fail_random = false;
    storage_calls = entropy_reads = record_admission_clears = 0;
}

static int no_files(const storage_fixture *fixture)
{
    struct stat info = {0};
    CHECK(fstatat(fixture->directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(fstatat(fixture->directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(fstatat(fixture->directory, ".wallet.pending", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    return 0;
}

static jint create(JNIEnv *env, jbyteArray path, jbyteArray record, jbyteArray entropy)
{
    jint status = API(createFreshWalletStorage)(env, NULL, path, record, entropy);
    REQUIRE(zero_calls == 1 && secret_pointer == NULL && cleared == 7);
    return status;
}

static jint write_record(JNIEnv *env, bool promote, jbyteArray path, jbyteArray record)
{
    const jint status = promote ? API(promoteWalletStorage)(env, NULL, path, record) :
        API(createWalletStorage)(env, NULL, path, record);
    REQUIRE(cleared == 3 && zero_calls == 0);
    return status;
}

static fake_array *read_record(JNIEnv *env, jbyteArray path)
{
    const unsigned expected_clears = env != NULL && !pending ? 9U : 0U;
    fake_array *result = (fake_array *)API(readWalletStorage)(env, NULL, path);
    REQUIRE(cleared == expected_clears && zero_calls == 0);
    return result;
}

static int write_failures(const change_storage_data *data, bool promote)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    for (size_t at = 1; at <= 4; ++at) {
        reset(&fixture, data);
        fail_call = at;
        CHECK(write_record(&fake_env, promote, (jbyteArray)&path_array,
            (jbyteArray)&record_array) == ZCL_INVALID_ARGUMENT);
        CHECK(pending && vm_calls == at && no_files(&fixture) == 0);
    }
    for (size_t argument = 0; argument < 4; ++argument) {
        reset(&fixture, data);
        pending = argument == 3;
        CHECK(write_record(argument == 0 ? NULL : &fake_env, promote,
            argument == 1 ? NULL : (jbyteArray)&path_array,
            argument == 2 ? NULL : (jbyteArray)&record_array) == ZCL_INVALID_ARGUMENT);
        CHECK(pending == (argument == 3) && no_files(&fixture) == 0);
    }
    return fixture_close(&fixture);
}

static int read_matches(const storage_fixture *fixture, const change_storage_data *data, bool is_pending)
{
    reset(fixture, data);
    const fake_array *packet = read_record(&fake_env, (jbyteArray)&path_array);
    CHECK(packet == &publication && packet->length == (jsize)(data->wallet_len + 2));
    CHECK(published[0] == ZCL_OK && published[1] == (is_pending ? 1 : 0));
    CHECK(memcmp(published + 2, data->wallet, data->wallet_len) == 0 && !pending);
    /* Native erasure may not erase or overrun the separate VM result. */
    for (size_t i = data->wallet_len + 2; i < sizeof(published); ++i) CHECK(published[i] == 0xa5);
    return 0;
}

static int write_outcomes(const change_storage_data *data, bool promote)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    if (promote) CHECK(fixture_write(&fixture, ".wallet.pending", data->wallet, data->wallet_len) == 0);
    reset(&fixture, data);
    CHECK(write_record(&fake_env, promote, (jbyteArray)&path_array, (jbyteArray)&record_array) == ZCL_OK);
    CHECK(read_matches(&fixture, data, false) == 0);
    reset(&fixture, data);
    CHECK(write_record(&fake_env, false, (jbyteArray)&path_array,
        (jbyteArray)&record_array) == ZCL_ALREADY_EXISTS);
    CHECK(read_matches(&fixture, data, false) == 0);
    return fixture_close(&fixture);
}

static int read_failures(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, ".wallet.pending", data->wallet, data->wallet_len) == 0);
    CHECK(read_matches(&fixture, data, true) == 0);
    for (size_t at = 1; at <= 2; ++at) {
        reset(&fixture, data);
        fail_call = at;
        CHECK(read_record(&fake_env, (jbyteArray)&path_array) == NULL);
        CHECK(pending && vm_calls == at && published[0] == 0xa5);
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        reset(&fixture, data);
        fail_new = fault;
        fail_set = fault == 0;
        CHECK(read_record(&fake_env, (jbyteArray)&path_array) == NULL);
        CHECK(pending == (fault != 3));
        CHECK(published[0] == (fault == 0 ? ZCL_OK : 0xa5) && published[1] == 0xa5);
        CHECK(read_matches(&fixture, data, true) == 0);
    }
    return fixture_close(&fixture);
}

static int read_status_only(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    reset(&fixture, data);
    const fake_array *packet = read_record(&fake_env, NULL);
    CHECK(packet == &publication && packet->length == 1 && published[0] == ZCL_INVALID_ARGUMENT);
    CHECK(published[1] == 0xa5 && !pending);
    reset(&fixture, data);
    path_array.length = INT32_MAX;
    packet = read_record(&fake_env, (jbyteArray)&path_array);
    CHECK(packet == &publication && packet->length == 1 && published[0] == ZCL_OUT_OF_RANGE);
    CHECK(published[1] == 0xa5 && !pending && no_files(&fixture) == 0);
    return fixture_close(&fixture);
}

static int storage_refusals(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, ".wallet.pending", data->wallet, data->wallet_len) == 0);
    reset(&fixture, data);
    /* Structurally valid but not the exact pending ciphertext. */
    mutable_record[data->wallet_len - 1] ^= 1;
    CHECK(write_record(&fake_env, true, (jbyteArray)&path_array,
        (jbyteArray)&record_array) == ZCL_INVALID_ENCODING);
    CHECK(read_matches(&fixture, data, true) == 0);
    CHECK(fixture_close(&fixture) == 0);
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, "wallet.zcl", data->wallet, 1) == 0);
    reset(&fixture, data);
    const fake_array *packet = read_record(&fake_env, (jbyteArray)&path_array);
    CHECK(packet == &publication && packet->length == 1 && published[0] == ZCL_INVALID_ENCODING);
    CHECK(published[1] == 0xa5 && !pending);
    reset(&fixture, data);
    CHECK(write_record(&fake_env, false, (jbyteArray)&path_array,
        (jbyteArray)&record_array) == ZCL_ALREADY_EXISTS);
    return fixture_close(&fixture);
}

static int vm_failures(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    for (size_t at = 1; at <= 6; ++at) {
        reset(&fixture, data);
        fail_call = at;
        CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
            (jbyteArray)&entropy_array) == ZCL_INVALID_ARGUMENT);
        CHECK(pending && vm_calls == at && core_calls == 0 && no_files(&fixture) == 0);
    }
    reset(&fixture, data);
    pending = true;
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_INVALID_ARGUMENT);
    CHECK(pending && vm_calls == 0 && core_calls == 0 && no_files(&fixture) == 0);
    return fixture_close(&fixture);
}

static int null_arguments(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    for (size_t argument = 0; argument < 4; ++argument) {
        reset(&fixture, data);
        CHECK(create(argument == 0 ? NULL : &fake_env, argument == 1 ? NULL : (jbyteArray)&path_array,
            argument == 2 ? NULL : (jbyteArray)&record_array,
            argument == 3 ? NULL : (jbyteArray)&entropy_array) == ZCL_INVALID_ARGUMENT);
        CHECK(core_calls == 0 && !pending && no_files(&fixture) == 0);
    }
    return fixture_close(&fixture);
}

static int length_arguments(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    const jsize invalid[2] = {-1, INT32_MAX};
    for (size_t argument = 0; argument < 3; ++argument) {
        for (size_t value = 0; value < 2; ++value) {
            reset(&fixture, data);
            fake_array *selected = argument == 0 ? &path_array : argument == 1 ? &record_array : &entropy_array;
            selected->length = invalid[value];
            CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
                (jbyteArray)&entropy_array) == ZCL_OUT_OF_RANGE);
            CHECK(core_calls == 0 && !pending && no_files(&fixture) == 0);
        }
    }
    return fixture_close(&fixture);
}

static int core_outcomes(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    reset(&fixture, data);
    fail_core = true;
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_CRYPTO_FAILURE);
    CHECK(core_calls == 1 && vm_calls == 6 && no_files(&fixture) == 0);
    reset(&fixture, data);
    mutate_record = true;
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_OK);
    CHECK(core_calls == 1 && vm_calls == 6 && !mutate_record);
    CHECK(memcmp(mutable_record, data->wallet, data->wallet_len) != 0);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    uint8_t record[140] = {0};
    size_t length = 0;
    bool is_pending = false;
    CHECK(fixture_read(&fixture, record, sizeof(record), &length, &is_pending) == ZCL_OK);
    CHECK(!is_pending && length == data->wallet_len && memcmp(record, data->wallet, length) == 0);
    reset(&fixture, data);
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_ALREADY_EXISTS);
    return fixture_close(&fixture);
}

static int malformed_record_precedes_entropy(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    reset(&fixture, data);
    mutable_record[0] ^= 1;
    malformed_record_case = true;
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_UNSUPPORTED);
    CHECK(vm_calls == 4 && entropy_reads == 0 && core_calls == 0 &&
        record_admission_clears == 1);
    CHECK(no_files(&fixture) == 0);
    return fixture_close(&fixture);
}

static int full_entropy_data(change_storage_data *data, const uint8_t *entropy, size_t entropy_len)
{
    CHECK(data != NULL && entropy != NULL && entropy_len == 32);
    uint8_t header[80] = {0}, blind[32] = {1}, iv[12] = {0}, ciphertext[48] = {0};
    CHECK(zcl_wallet_header_create(entropy, entropy_len, ZCL_TESTNET, blind, sizeof(blind),
        header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, sizeof(header), iv, sizeof(iv), ciphertext, sizeof(ciphertext),
        data->wallet, sizeof(data->wallet), &data->wallet_len) == ZCL_OK);
    CHECK(zcl_change_state_encode(header, sizeof(header), entropy, entropy_len, blind, sizeof(blind),
        0, data->state[0], sizeof(data->state[0])) == ZCL_OK);
    return 0;
}

static int entropy_creation_attempt(const storage_fixture *fixture, const change_storage_data *data,
    const uint8_t entropy[32], unsigned fault, zcl_status wanted)
{
    reset(fixture, data);
    memcpy(entropy_bytes, entropy, 32);
    entropy_array.length = fault == 1 ? 16 : 32;
    if (fault == 2) entropy_bytes[0] ^= 1;
    fail_random = fault == 3;
    const jint status = create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array);
    CHECK(status == (jint)wanted && !live_entropy_after_core && !live_entropy_at_storage);
    CHECK(storage_calls == (fault == 0 ? 1u : 0u));
    CHECK(core_calls == (fault == 1 ? 0u : 1u) && vm_calls == 6 && zero_calls == 1 && !pending);
    return 0;
}

static int full_entropy_cleared(void)
{
    /* Public, inert fixture. Every byte is nonzero so a truncated native wipe
     * cannot pass merely because the unused secret-buffer suffix was zero. */
    uint8_t entropy[32] = {0};
    for (size_t i = 0; i < sizeof(entropy); ++i) entropy[i] = (uint8_t)(i + 1);
    change_storage_data data = {0};
    CHECK(full_entropy_data(&data, entropy, sizeof(entropy)) == 0);
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    int result = entropy_creation_attempt(&fixture, &data, entropy, 1, ZCL_OUT_OF_RANGE);
    if (result == 0) result = entropy_creation_attempt(&fixture, &data, entropy, 2, ZCL_INVALID_ENCODING);
    if (result == 0) result = entropy_creation_attempt(&fixture, &data, entropy, 3, ZCL_CRYPTO_FAILURE);
    if (result == 0) result = no_files(&fixture);
    if (result == 0) result = entropy_creation_attempt(&fixture, &data, entropy, 0, ZCL_OK);
    if (result == 0) result = entropy_creation_attempt(&fixture, &data, entropy, 0, ZCL_ALREADY_EXISTS);
    expected = NULL;
    CHECK(memcmp(entropy_bytes, entropy, sizeof(entropy)) == 0);
    if (result == 0) result = change_bytes(&fixture, data.state[0], 80, 0);
    if (result == 0) result = read_matches(&fixture, &data, false);
    expected = NULL;
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(entropy_bytes, sizeof(entropy_bytes));
    const int closed = fixture_close(&fixture);
    CHECK(closed == 0 && result == 0);
    return 0;
}

static int read_environment_refusal(void)
{
    const fake_array unused = {0}; /* This entry must never inspect its fields. */
    CHECK(read_record(NULL, NULL) == NULL);
    CHECK(read_record(NULL, (jbyteArray)&unused) == NULL);
    pending = true;
    vm_calls = 0;
    CHECK(read_record(&fake_env, (jbyteArray)&unused) == NULL);
    CHECK(pending && vm_calls == 0);
    pending = false;
    return 0;
}

static int owned_span_refusals(const change_storage_data *data)
{
    uint8_t entropy[32];
    memset(entropy, 0x42, sizeof(entropy));
    const size_t capacities[] = {0, 31, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i) {
        CHECK(zcl_wallet_change_create_owned(NULL, 0, data->wallet, data->wallet_len,
            entropy, 16, capacities[i]) == ZCL_INVALID_ARGUMENT);
        for (size_t j = 0; j < sizeof(entropy); ++j) CHECK(entropy[j] == 0x42);
    }
    CHECK(zcl_wallet_change_create_owned(NULL, 0, data->wallet, data->wallet_len,
        NULL, 16, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_create_owned(NULL, 0, data->wallet, data->wallet_len,
        entropy, SIZE_MAX, sizeof(entropy)) == ZCL_OUT_OF_RANGE);
    for (size_t i = 0; i < sizeof(entropy); ++i) CHECK(entropy[i] == 0);
    return 0;
}

int main(void)
{
    CHECK(read_environment_refusal() == 0);
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(owned_span_refusals(&data) == 0);
    CHECK(vm_failures(&data) == 0 && null_arguments(&data) == 0);
    CHECK(length_arguments(&data) == 0 && core_outcomes(&data) == 0);
    CHECK(malformed_record_precedes_entropy(&data) == 0);
    CHECK(full_entropy_cleared() == 0);
    CHECK(write_failures(&data, false) == 0 && write_failures(&data, true) == 0);
    CHECK(write_outcomes(&data, false) == 0 && write_outcomes(&data, true) == 0);
    CHECK(read_failures(&data) == 0 && read_status_only(&data) == 0);
    CHECK(storage_refusals(&data) == 0);
    puts("JNI storage: live path/record/entropy/packet retirement, VM faults, publication faults, paired creation, pending promotion and no overwrite passed");
    return 0;
}
