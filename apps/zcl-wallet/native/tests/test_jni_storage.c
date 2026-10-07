/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_secure_zero
#undef zcl_wallet_change_create
#include "jni_support.h"
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "JNI storage fault at%d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jint JNICALL API(createFreshWalletStorage)(JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray);

typedef struct { const uint8_t *bytes; size_t capacity; jsize length; } fake_array;
static fake_array path_array, record_array, entropy_array;
static uint8_t mutable_record[140], entropy_bytes[33];
static const change_storage_data *expected;
static void *secret_pointer;
static size_t vm_calls, fail_call, core_calls, zero_calls;
static bool pending, fail_core, mutate_record;

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
    if (array == &entropy_array) secret_pointer = output;
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
    REQUIRE(pointer != NULL && length == 32 && zero_calls == 0);
    if (secret_pointer != NULL) REQUIRE(secret_pointer == pointer);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    secret_pointer = NULL; /* Retire while this invocation's span is live. */
    ++zero_calls;
}

zcl_status zcl_jni_storage_test_create(const uint8_t *path, size_t path_len,
    const uint8_t *record, size_t record_len, const uint8_t *entropy, size_t entropy_len)
{
    REQUIRE(!pending && core_calls == 0);
    ++core_calls;
    REQUIRE(path != path_array.bytes && record != record_array.bytes && entropy != entropy_array.bytes);
    REQUIRE(record_len == expected->wallet_len && memcmp(record, expected->wallet, record_len) == 0);
    REQUIRE(entropy_len == (size_t)entropy_array.length && entropy == secret_pointer);
    if (fail_core) return ZCL_CRYPTO_FAILURE;
    return zcl_wallet_change_create(path, path_len, record, record_len, entropy, entropy_len);
}

static const struct JNINativeInterface_ fake_table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length, .GetByteArrayRegion = get_bytes
};
static JNIEnv fake_env = &fake_table;

static void reset(const storage_fixture *fixture, const change_storage_data *data)
{
    REQUIRE(secret_pointer == NULL);
    memcpy(mutable_record, data->wallet, data->wallet_len);
    memset(entropy_bytes, 0, sizeof(entropy_bytes));
    path_array = (fake_array){(const uint8_t *)fixture->path, fixture_path_len(), (jsize)fixture_path_len()};
    record_array = (fake_array){mutable_record, sizeof(mutable_record), (jsize)data->wallet_len};
    entropy_array = (fake_array){entropy_bytes, sizeof(entropy_bytes), 16};
    expected = data;
    vm_calls = fail_call = core_calls = zero_calls = 0;
    pending = fail_core = mutate_record = false;
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
    REQUIRE(zero_calls == 1 && secret_pointer == NULL);
    return status;
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

static int full_entropy_data(change_storage_data *data, const uint8_t entropy[32])
{
    uint8_t header[80] = {0}, blind[32] = {1}, iv[12] = {0}, ciphertext[48] = {0};
    CHECK(zcl_wallet_header_create(entropy, 32, ZCL_TESTNET, blind, sizeof(blind),
        header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, sizeof(header), iv, sizeof(iv), ciphertext, sizeof(ciphertext),
        data->wallet, sizeof(data->wallet), &data->wallet_len) == ZCL_OK);
    CHECK(zcl_change_state_encode(header, sizeof(header), entropy, 32, blind, sizeof(blind),
        0, data->state[0], sizeof(data->state[0])) == ZCL_OK);
    return 0;
}

static int full_entropy_cleared(void)
{
    /* Public, inert fixture. Every byte is nonzero so a truncated native wipe
     * cannot pass merely because the unused secret-buffer suffix was zero. */
    uint8_t entropy[32] = {0};
    for (size_t i = 0; i < sizeof(entropy); ++i) entropy[i] = (uint8_t)(i + 1);
    change_storage_data data = {0};
    CHECK(full_entropy_data(&data, entropy) == 0);
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    reset(&fixture, &data);
    memcpy(entropy_bytes, entropy, sizeof(entropy));
    entropy_array.length = (jsize)sizeof(entropy);
    CHECK(create(&fake_env, (jbyteArray)&path_array, (jbyteArray)&record_array,
        (jbyteArray)&entropy_array) == ZCL_OK);
    CHECK(core_calls == 1 && vm_calls == 6 && zero_calls == 1);
    CHECK(memcmp(entropy_bytes, entropy, sizeof(entropy)) == 0);
    CHECK(change_bytes(&fixture, data.state[0], 80, 0) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(vm_failures(&data) == 0 && null_arguments(&data) == 0);
    CHECK(length_arguments(&data) == 0 && core_outcomes(&data) == 0);
    CHECK(full_entropy_cleared() == 0);
    puts("JNI fresh storage: six VM faults, pending exception refusal, bounded private copies, full-width nonzero entropy clearing, paired creation and no overwrite passed");
    return 0;
}
