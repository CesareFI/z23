/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_random_bytes
#undef zcl_secure_zero
#undef zcl_change_state_decode
#undef zcl_wallet_recovered_change
#include "change_storage_fixture.h"
#include "change_custody_retirement.h"
#include "../src/change_custody_internal.h"
#include "storage_faults.h"
#include "zcl_change_reservation.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "change ownership fault at%d\n", __LINE__); abort(); } } while (0)
static struct { void *pointer; size_t length; bool cleared; } spans[2];
static size_t random_calls, fail_random, stat_calls, fail_stat;
static unsigned fail_crypto;
static bool fail_observation;
static uint8_t *mutate_record_byte;
int __real_fstat(int fd, struct stat *info);
int __wrap_fstat(int fd, struct stat *info);
zcl_status __real_zcl_storage_change_observe(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet, size_t wallet_len, zcl_change_storage_snapshot *snapshot);
zcl_status __wrap_zcl_storage_change_observe(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet, size_t wallet_len, zcl_change_storage_snapshot *snapshot);

zcl_status __wrap_zcl_storage_change_observe(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet, size_t wallet_len, zcl_change_storage_snapshot *snapshot)
{
    const zcl_status status = __real_zcl_storage_change_observe(directory, directory_len,
        wallet, wallet_len, snapshot);
    /* Stronger than the real provider's failure atomicity: valid-looking
     * private output must still be ignored after its error return. */
    return fail_observation && status == ZCL_OK ? ZCL_IO_UNCERTAIN : status;
}

int __wrap_fstat(int fd, struct stat *info)
{
    if (++stat_calls == fail_stat) { errno = EIO; return -1; }
    return __real_fstat(fd, info);
}

zcl_status zcl_ownership_test_random(uint8_t *output, size_t length)
{
    REQUIRE(output != NULL && random_calls < 2);
    REQUIRE(length == (random_calls == 0 ? 32 : 64));
    spans[random_calls].pointer = output;
    spans[random_calls].length = length;
    spans[random_calls].cleared = false;
    ++random_calls;
    if (mutate_record_byte != NULL) {
        *mutate_record_byte ^= 1; /* Caller header/ciphertext changes after the private copy. */
        mutate_record_byte = NULL;
    }
    if (random_calls == fail_random) {
        memset(output, 0x42, length / 2);
        return ZCL_IO_FAILURE;
    }
    memset(output, 0x42, length); /* Public test blinding only. */
    return ZCL_OK;
}

void zcl_ownership_test_zero(void *pointer, size_t length)
{
    if (change_custody_retirement_zero(pointer, length)) return;
    REQUIRE(pointer != NULL && (length == 32 || length == 64) && random_calls <= 2);
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    bool found = false;
    for (size_t i = 0; i < random_calls; ++i) {
        if (spans[i].pointer != pointer) continue;
        REQUIRE(spans[i].length == length && !spans[i].cleared);
        spans[i].pointer = NULL; /* Retire while the borrowed object is live. */
        spans[i].cleared = true;
        found = true;
    }
    REQUIRE(found);
}

zcl_status zcl_ownership_test_decode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    const uint8_t *record, size_t record_len, uint32_t *index)
{
    REQUIRE(random_calls == 1 && index != NULL);
    if (fail_crypto == 1) { *index = 1; return ZCL_CRYPTO_FAILURE; } /* Plausible dirty counter. */
    return zcl_change_state_decode(header, header_len, entropy, entropy_len, blinding, blinding_len,
        record, record_len, index);
}

zcl_status zcl_ownership_test_derive(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, uint32_t index, const uint8_t *blinding, size_t blinding_len,
    uint8_t *address, size_t capacity)
{
    REQUIRE(random_calls == 2 && address != NULL && capacity == 35);
    if (fail_crypto == 2) { memset(address, 0x42, 16); return ZCL_CRYPTO_FAILURE; }
    return zcl_wallet_recovered_change(header, header_len, entropy, entropy_len, index,
        blinding, blinding_len, address, capacity);
}

static void reset(void)
{
    change_custody_retirement_check();
    REQUIRE(random_calls <= 2 && mutate_record_byte == NULL);
    for (size_t i = 0; i < random_calls; ++i) REQUIRE(spans[i].cleared && spans[i].pointer == NULL);
    memset(spans, 0, sizeof(spans));
    random_calls = fail_random = stat_calls = fail_stat = 0;
    fail_crypto = 0;
    fail_observation = false;
    storage_faults_reset();
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

static int setup(storage_fixture *fixture, const change_storage_data *data)
{
    reset();
    CHECK(fixture_open(fixture) == 0 && change_create(fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(fixture, data, &head) == ZCL_OK && change_append(fixture, data, &head, 1) == ZCL_OK);
    reset();
    return 0;
}

static zcl_status reconstruct(const storage_fixture *fixture, const change_storage_data *data, uint8_t *address)
{
    static const uint8_t entropy[16] = {0};
    return zcl_wallet_change_reserved_address((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, sizeof(entropy), 0, address, 35);
}

static int preserved(const storage_fixture *fixture, const change_storage_data *data)
{
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(fixture, data->state[1], 80, 80) == 0);
    zcl_change_storage_snapshot head = {0};
    CHECK(change_observe(fixture, data, &head) == ZCL_OK && head.file_bytes == 160);
    return 0;
}

static int failure(const change_storage_data *data, size_t rng_at, unsigned crypto_at,
    io_fault *fault, io_mode mode, size_t io_at, size_t stat_at)
{
    storage_fixture fixture;
    CHECK(setup(&fixture, data) == 0);
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    uint8_t address[35];
    memset(address, 0xa5, sizeof(address));
    fail_random = rng_at; fail_crypto = crypto_at; fail_stat = stat_at;
    fail_observation = crypto_at == 3;
    if (fault != NULL) *fault = (io_fault){mode, io_at, 0};
    CHECK(reconstruct(&fixture, data, address) != ZCL_OK);
    for (size_t i = 0; i < sizeof(address); ++i) CHECK(address[i] == 0xa5);
    const size_t expected_random = fail_observation ? 0 : (rng_at == 0 ? (size_t)crypto_at : rng_at);
    CHECK(random_calls == expected_random);
    CHECK(storage_write_fault.calls == 0 && storage_rename_fault.calls == 0);
    if (fault == &storage_sync_fault)
        CHECK(storage_sync_fault.calls == (mode == IO_INTERRUPT ? 16 : 1));
    else CHECK(storage_sync_fault.calls <= 1); /* Shared directory-open parent durability check. */
    CHECK(storage_read_fault.calls <= 256 && storage_pread_fault.calls <= 256);
    if (stat_at != 0) CHECK(stat_calls == stat_at);
    reset();
    CHECK(descriptors(&after) == 0 && before == after && preserved(&fixture, data) == 0);
    reset();
    return fixture_close(&fixture);
}

static int success(const change_storage_data *data, size_t *close_count, size_t *stat_count, size_t mutated_byte)
{
    storage_fixture fixture;
    CHECK(setup(&fixture, data) == 0);
    change_storage_data mutable = *data;
    uint8_t address[35], expected[35], entropy[16] = {0}, blind[32] = {1};
    size_t length = 0;
    CHECK(zcl_change_from_entropy(entropy, 16, ZCL_TESTNET, 0, blind, 32, expected, 35, &length) == ZCL_OK);
    CHECK(length == 35);
    CHECK(mutated_byte < mutable.wallet_len);
    mutate_record_byte = &mutable.wallet[mutated_byte];
    CHECK(reconstruct(&fixture, &mutable, address) == ZCL_OK && random_calls == 2);
    CHECK(memcmp(address, expected, 35) == 0);
    CHECK(storage_write_fault.calls == 0 && storage_rename_fault.calls == 0);
    CHECK(storage_sync_fault.calls == 1); /* Existing store-open syncs its parent even for reads. */
    *close_count = storage_close_fault.calls;
    *stat_count = stat_calls;
    CHECK(*close_count > 0 && *close_count <= 16 && *stat_count > 0 && *stat_count <= 16);
    reset();
    CHECK(preserved(&fixture, data) == 0);
    reset();
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t address[35] = {0};
    CHECK(zcl_wallet_change_reserved_address(NULL, 0, NULL, 0, NULL, 0, 0,
        address, sizeof(address)) == ZCL_INVALID_ARGUMENT);
    reset();
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    size_t closes = 0, stats = 0;
    CHECK(success(&data, &closes, &stats, 0) == 0);
    CHECK(success(&data, &closes, &stats, data.wallet_len - 1) == 0);
    CHECK(failure(&data, 0, 3, NULL, IO_NORMAL, 0, 0) == 0);
    for (size_t at = 1; at <= 2; ++at) {
        CHECK(failure(&data, at, 0, NULL, IO_NORMAL, 0, 0) == 0);
        CHECK(failure(&data, 0, (unsigned)at, NULL, IO_NORMAL, 0, 0) == 0);
    }
    for (size_t at = 1; at <= closes; ++at)
        CHECK(failure(&data, 0, 0, &storage_close_fault, IO_INTERRUPT, at, 0) == 0);
    for (size_t at = 1; at <= stats; ++at)
        CHECK(failure(&data, 0, 0, NULL, IO_NORMAL, 0, at) == 0);
    CHECK(failure(&data, 0, 0, &storage_sync_fault, IO_ERROR, 0, 0) == 0);
    CHECK(failure(&data, 0, 0, &storage_sync_fault, IO_INTERRUPT, 0, 0) == 0);
    const io_mode modes[] = {IO_ERROR, IO_ZERO, IO_OVERSIZE, IO_INTERRUPT};
    for (size_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        CHECK(failure(&data, 0, 0, &storage_read_fault, modes[mode], 0, 0) == 0);
        CHECK(failure(&data, 0, 0, &storage_pread_fault, modes[mode], 0, 0) == 0);
    }
    CHECK(puts("Change ownership failures preserve output/files/descriptors and clear both live blinding spans") >= 0);
    return 0;
}
