/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_random_bytes
#undef zcl_secure_zero
#undef zcl_change_state_encode
#undef zcl_change_state_decode
#include "change_storage_fixture.h"
#include "change_custody_retirement.h"
#include "../src/change_custody_internal.h"
#include "storage_faults.h"
#include "zcl_change_reservation.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "recovery fault at%d\n", __LINE__); abort(); } } while (0)
typedef struct { void *pointer; bool cleared; } blind_span;
static blind_span spans[4];
static size_t random_calls, crypto_calls, fail_random, fail_crypto;
static uint8_t *mutate_cipher;
static const storage_fixture *competing_fixture;
static const change_storage_data *competing_data;

zcl_status zcl_recovery_test_random(uint8_t *output, size_t length)
{
    REQUIRE(output != NULL && length == 32 && random_calls < 4);
    spans[random_calls] = (blind_span){output, false};
    ++random_calls;
    if (mutate_cipher != NULL) {
        *mutate_cipher ^= 1;
        mutate_cipher = NULL;
    }
    if (competing_fixture != NULL && random_calls == 4) {
        zcl_change_storage_snapshot old = {0};
        REQUIRE(change_observe(competing_fixture, competing_data, &old) == ZCL_OK);
        REQUIRE(zcl_storage_change_repair((const uint8_t *)competing_fixture->path, fixture_path_len(),
            competing_data->wallet, competing_data->wallet_len, &old, competing_data->state[2], 80) == ZCL_OK);
        competing_fixture = NULL;
        competing_data = NULL;
    }
    memset(output, 0x42, random_calls == fail_random ? length / 2 : length);
    return random_calls == fail_random ? ZCL_IO_FAILURE : ZCL_OK;
}

void zcl_recovery_test_zero(void *pointer, size_t length)
{
    if (change_custody_retirement_zero(pointer, length)) return;
    REQUIRE(pointer != NULL && length == 32);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    bool found = false;
    for (size_t i = 0; i < random_calls; ++i) {
        if (spans[i].pointer != pointer) continue;
        REQUIRE(!spans[i].cleared);
        spans[i].pointer = NULL; /* Retire while the automatic span is live. */
        spans[i].cleared = true;
        found = true;
    }
    REQUIRE(found);
}

zcl_status zcl_recovery_test_encode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint32_t index, uint8_t *record, size_t capacity)
{
    ++crypto_calls;
    if (crypto_calls == fail_crypto) {
        REQUIRE(record != NULL && capacity >= 80);
        memset(record, 0x42, 16);
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_change_state_encode(header, header_len, entropy, entropy_len, blinding, blinding_len,
        index, record, capacity);
}

zcl_status zcl_recovery_test_decode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    const uint8_t *record, size_t record_len, uint32_t *index)
{
    ++crypto_calls;
    if (crypto_calls == fail_crypto) {
        REQUIRE(index != NULL);
        *index = UINT32_MAX;
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_change_state_decode(header, header_len, entropy, entropy_len, blinding, blinding_len,
        record, record_len, index);
}

static void checked_clear(void)
{
    change_custody_retirement_check();
    for (size_t i = 0; i < random_calls; ++i) REQUIRE(spans[i].cleared && spans[i].pointer == NULL);
    memset(spans, 0, sizeof(spans));
    random_calls = crypto_calls = fail_random = fail_crypto = 0;
    REQUIRE(mutate_cipher == NULL && competing_fixture == NULL && competing_data == NULL);
    storage_faults_reset();
}

static zcl_status recover(const storage_fixture *fixture, const change_storage_data *data)
{
    const uint8_t entropy[16] = {0};
    return zcl_wallet_change_recover((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, sizeof(entropy));
}

static int initial_partial(storage_fixture *fixture, const change_storage_data *data)
{
    CHECK(fixture_open(fixture) == 0);
    CHECK(fixture_create(fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t bytes[120] = {0};
    memcpy(bytes, data->state[0], 80);
    memcpy(bytes + 80, data->state[1], 40);
    return fixture_write(fixture, ".change.index", bytes, sizeof(bytes));
}

static int descriptors(size_t *count)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    size_t found = 0;
    errno = 0;
    while (found < 256 && readdir(directory) != NULL) ++found;
    int error = errno;
    CHECK(closedir(directory) == 0 && error == 0 && found < 256);
    *count = found;
    return 0;
}

static int after_failure(const storage_fixture *fixture, const change_storage_data *data, uint32_t size)
{
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == size);
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(fixture, data->state[1], 40, 80) == 0);
    uint8_t saved[240] = {0};
    CHECK(size <= sizeof(saved));
    int fd = openat(fixture->directory, ".change.index", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    ssize_t got = read(fd, saved, size);
    int closed = close(fd);
    CHECK(got >= 0 && (size_t)got == size && closed == 0);
    zcl_status status = recover(fixture, data);
    checked_clear();
    /* A partial replacement has an invalid predecessor. Preserve it for
     * independent recovery review; never weaken this rule to force a retry. */
    CHECK(status == (size == 200 ? ZCL_INVALID_ENCODING : size == 240 ? ZCL_ALREADY_EXISTS : ZCL_OK));
    CHECK(change_bytes(fixture, saved, size, 0) == 0);
    CHECK(change_observe(fixture, data, &snapshot) == ZCL_OK);
    CHECK(snapshot.file_bytes == (size == 200 ? 200U : 240U));
    if (size != 200) CHECK(memcmp(snapshot.tail, data->state[2], 80) == 0);
    return 0;
}

static int failed(const change_storage_data *data, size_t rng_failure, size_t crypto_failure,
    io_fault *fault, io_mode mode, size_t at, uint32_t bytes, size_t rng_calls)
{
    storage_fixture fixture;
    CHECK(initial_partial(&fixture, data) == 0);
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    checked_clear();
    fail_random = rng_failure;
    fail_crypto = crypto_failure;
    if (fault != NULL) *fault = (io_fault){mode, at, 0};
    CHECK(recover(&fixture, data) != ZCL_OK);
    CHECK(random_calls == rng_calls);
    if (fault != NULL) CHECK(fault->calls <= 256);
    checked_clear();
    CHECK(descriptors(&after) == 0 && before == after);
    CHECK(after_failure(&fixture, data, bytes) == 0);
    return fixture_close(&fixture);
}

static int private_binding(const change_storage_data *data, bool competitor)
{
    storage_fixture fixture;
    CHECK(initial_partial(&fixture, data) == 0);
    change_storage_data mutable = *data;
    checked_clear();
    if (competitor) {
        competing_fixture = &fixture;
        competing_data = data;
    } else mutate_cipher = &mutable.wallet[mutable.wallet_len - 1];
    CHECK(recover(&fixture, &mutable) == (competitor ? ZCL_BUSY : ZCL_OK));
    CHECK(random_calls == 4 && crypto_calls == 4);
    checked_clear();
    CHECK(after_failure(&fixture, data, 240) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    CHECK(zcl_wallet_change_recover(NULL, 0, NULL, 0, NULL, 0) == ZCL_INVALID_ARGUMENT);
    checked_clear();
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    for (size_t at = 1; at <= 4; ++at) {
        CHECK(failed(&data, at, 0, NULL, IO_NORMAL, 0, 120, at) == 0);
        CHECK(failed(&data, 0, at, NULL, IO_NORMAL, 0, 120, at) == 0);
    }
    for (size_t at = 1; at <= 10; ++at)
        CHECK(failed(&data, 0, 0, &storage_close_fault, IO_INTERRUPT, at,
            at >= 8 ? 240 : 120, at >= 6 ? 4 : 0) == 0);
    for (size_t at = 1; at <= 4; ++at)
        CHECK(failed(&data, 0, 0, &storage_sync_fault, IO_ERROR, at,
            at >= 3 ? 240 : 120, at >= 2 ? 4 : 0) == 0);
    for (size_t at = 1; at <= 3; ++at)
        CHECK(failed(&data, 0, 0, &storage_pread_fault, IO_ERROR, at, 120, at == 3 ? 4 : 0) == 0);
    CHECK(failed(&data, 0, 0, &storage_write_fault, IO_PARTIAL_ERROR, 1, 140, 4) == 0);
    CHECK(failed(&data, 0, 0, &storage_write_fault, IO_ERROR, 2, 160, 4) == 0);
    CHECK(failed(&data, 0, 0, &storage_write_fault, IO_PARTIAL_ERROR, 2, 200, 4) == 0);
    CHECK(private_binding(&data, false) == 0 && private_binding(&data, true) == 0);
    puts("change recovery faults: four RNG/codec steps, live cleanup, private wallet binding, competing repair, durability failures and refused ambiguous retry passed");
    return 0;
}
