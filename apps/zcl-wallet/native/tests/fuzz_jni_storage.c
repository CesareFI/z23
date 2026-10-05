/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_secure_zero
#include "jni_support.h"
#include "change_storage_fixture.h"
#include "zcl_keys.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

JNIEXPORT jint JNICALL Java_org_zclassic_wallet_core_NativeCore_createFreshWalletStorage(
    JNIEnv *, jclass, jbyteArray, jbyteArray, jbyteArray);
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);
typedef struct { const uint8_t *bytes; size_t capacity; jsize length; } fake_array;
static storage_fixture fixture;
static change_storage_data data;
static bool initialized, pending;
static size_t vm_calls, fail_call;
static uintptr_t identities[3];
static unsigned cleared;
static void require(bool condition) { if (!condition) abort(); }
static void cleanup(void) { require(fixture_close(&fixture) == 0); }

static void reset_retirement(void)
{
    for (size_t i = 0; i < 3; ++i) require(identities[i] == 0);
    cleared = 0;
}

void zcl_jni_storage_fuzz_zero(void *pointer, size_t length);
void zcl_jni_storage_fuzz_zero(void *pointer, size_t length)
{
    require(pointer != NULL);
    if (length == sizeof(zcl_wallet_record)) {
        zcl_secure_zero(pointer, length);
        const uint8_t *bytes = pointer;
        for (size_t i = 0; i < length; ++i) require(bytes[i] == 0);
        return;
    }
    const size_t slot = length == 1024 ? 0 : length == 140 ? 1 : 2;
    require(slot != 2 || length == 32);
    const unsigned bit = 1U << slot;
    require((cleared & bit) == 0);
    require(identities[slot] == 0 || identities[slot] == (uintptr_t)pointer);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) require(bytes[i] == 0);
    identities[slot] = 0;
    cleared |= bit;
}

static bool vm_fault(void)
{
    require(!pending);
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
    require(input != NULL);
    return vm_fault() ? 0 : ((fake_array *)input)->length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray input, jsize start, jsize length, jbyte *output)
{
    (void)env;
    const fake_array *array = (const fake_array *)input;
    require(array != NULL && output != NULL && start == 0 && length >= 0);
    require(length == array->length && (size_t)length <= array->capacity);
    const size_t slot = array->capacity == 1025 ? 0 : array->capacity == 140 ? 1 : 2;
    require(identities[slot] == 0);
    identities[slot] = (uintptr_t)output;
    if (vm_fault()) {
        if (length > 0) output[0] = 42;
        return;
    }
    memcpy(output, array->bytes, (size_t)length);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length, .GetByteArrayRegion = get_bytes
};
static JNIEnv env = &table;

static void initialize(void)
{
    if (initialized) return;
    require(change_data_init(&data) == 0 && fixture_open(&fixture) == 0);
    require(atexit(cleanup) == 0);
    initialized = true;
}

static void clear_files(void)
{
    const char *names[3] = {"wallet.zcl", ".wallet.pending", ".change.index"};
    for (size_t i = 0; i < 3; ++i) {
        int removed = unlinkat(fixture.directory, names[i], 0);
        require(removed == 0 || errno == ENOENT);
    }
}

static void verify(jint status, const uint8_t *record, size_t record_len, const uint8_t *entropy,
    size_t entropy_len)
{
    if (status == ZCL_IO_FAILURE || status == ZCL_IO_UNCERTAIN) return;
    struct stat info = {0};
    if (status != ZCL_OK) {
        require(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
        require(fstatat(fixture.directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
        require(fstatat(fixture.directory, ".wallet.pending", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
        return;
    }
    require(!pending && record_len <= 140 && entropy_len <= 32);
    uint8_t stored[140] = {0}, blind[32] = {1};
    size_t size = 0;
    bool is_pending = false;
    require(fixture_read(&fixture, stored, sizeof(stored), &size, &is_pending) == ZCL_OK);
    require(!is_pending && size == record_len && memcmp(record, stored, size) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    require(zcl_storage_change_observe((const uint8_t *)fixture.path, fixture_path_len(),
        record, record_len, &snapshot) == ZCL_OK && snapshot.file_bytes == 80);
    uint32_t index = UINT32_MAX;
    require(zcl_change_state_decode(record, 80, entropy, entropy_len, blind, 32,
        snapshot.tail, snapshot.tail_len, &index) == ZCL_OK && index == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length)
{
    if (length != 8) return 0;
    reset_retirement();
    initialize();
    clear_files();
    uint8_t path[1025] = {0}, record[140] = {0}, entropy[33] = {0};
    memcpy(path, fixture.path, fixture_path_len());
    memcpy(record, data.wallet, data.wallet_len);
    if ((bytes[0] & 1U) != 0) record[(size_t)bytes[1] % sizeof(record)] ^= bytes[2];
    if ((bytes[0] & 2U) != 0) entropy[(size_t)bytes[7] % sizeof(entropy)] ^= bytes[2];
    fake_array inputs[3] = {
        {path, sizeof(path), (jsize)fixture_path_len()},
        {record, sizeof(record), (jsize)data.wallet_len}, {entropy, sizeof(entropy), 16}
    };
    /* Truncating an absolute path can name a DIFFERENT valid directory. Keep
     * the sole valid locator fixed to this owned fixture; vary only lengths
     * that cannot authorize filesystem access. Do not fuzz ambient paths. */
    if ((bytes[0] & 4U) != 0)
        inputs[0].length = bytes[3] % 3U == 0 ? 0 : bytes[3] % 3U == 1 ? -1 : 1025;
    if ((bytes[0] & 8U) != 0) inputs[1].length = (jsize)bytes[4];
    if ((bytes[0] & 16U) != 0) inputs[2].length = bytes[5] == 255 ? -1 : (jsize)bytes[5];
    fail_call = bytes[6] % 7U;
    vm_calls = 0;
    pending = (bytes[0] & 32U) != 0;
    require(inputs[0].length == (jsize)fixture_path_len() || inputs[0].length <= 0 || inputs[0].length > 1024);
    jint status = Java_org_zclassic_wallet_core_NativeCore_createFreshWalletStorage(&env, NULL,
        (bytes[0] & 64U) != 0 ? NULL : (jbyteArray)&inputs[0], (jbyteArray)&inputs[1],
        (bytes[0] & 128U) != 0 ? NULL : (jbyteArray)&inputs[2]);
    require(vm_calls <= 6);
    require(cleared == 7);
    verify(status, record, (size_t)inputs[1].length, entropy,
        inputs[2].length >= 0 ? (size_t)inputs[2].length : SIZE_MAX);
    return 0;
}
