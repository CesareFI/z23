/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_change_internal.h"
#include "storage_faults.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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

static zcl_status repair(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *snapshot, const uint8_t *state)
{
    return zcl_storage_change_repair((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot, state, 80);
}

static int initial_partial(storage_fixture *fixture, const change_storage_data *data,
    zcl_change_storage_snapshot *snapshot)
{
    CHECK(fixture_open(fixture) == 0);
    CHECK(fixture_create(fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t original[120] = {0};
    memcpy(original, data->state[0], 80);
    memcpy(original + 80, data->state[1], 40);
    CHECK(fixture_write(fixture, ".change.index", original, sizeof(original)) == 0);
    CHECK(change_observe(fixture, data, snapshot) == ZCL_OK && snapshot->file_bytes == 120);
    return 0;
}

static int verify_preserved(const storage_fixture *fixture, const change_storage_data *data)
{
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(fixture, data->state[1], 40, 80) == 0);
    return 0;
}

static int recover_again(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *damaged)
{
    uint8_t entropy[16] = {0}, blind[32] = {1}, replacement[80] = {0};
    zcl_change_repair_plan plan = {0};
    CHECK(zcl_store_change_plan_repair(damaged, &plan) == ZCL_OK);
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        plan.next_index, replacement, sizeof(replacement)) == ZCL_OK);
    /* Save ALL currently existing bytes too, including previous padding and a
     * partial replacement. Repeated repair must never overwrite any of them. */
    uint8_t prior[240] = {0};
    CHECK(damaged->file_bytes <= sizeof(prior));
    int fd = openat(fixture->directory, ".change.index", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    ssize_t got = read(fd, prior, damaged->file_bytes);
    int closed = close(fd);
    CHECK(got >= 0 && (size_t)got == damaged->file_bytes && closed == 0);
    CHECK(repair(fixture, data, damaged, replacement) == ZCL_OK);
    CHECK(change_bytes(fixture, prior, damaged->file_bytes, 0) == 0);
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(fixture, data, &current) == ZCL_OK);
    uint32_t index = 0;
    CHECK(zcl_change_state_decode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        current.tail, current.tail_len, &index) == ZCL_OK && index == plan.next_index);
    CHECK(index >= 2 && current.file_bytes == (index + 1) * 80);
    return 0;
}

static int failed_repair(const change_storage_data *data, io_fault *fault,
    io_mode mode, size_t at, uint32_t expected_size)
{
    storage_fixture fixture;
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(initial_partial(&fixture, data, &snapshot) == 0);
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    storage_faults_reset();
    *fault = (io_fault){mode, at, 0};
    CHECK(repair(&fixture, data, &snapshot, data->state[2]) != ZCL_OK);
    CHECK(fault->calls <= 256);
    storage_faults_reset();
    CHECK(descriptors(&after) == 0 && before == after);
    CHECK(verify_preserved(&fixture, data) == 0);
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(&fixture, data, &current) == ZCL_OK && current.file_bytes == expected_size);
    if (expected_size != 120) CHECK(repair(&fixture, data, &snapshot, data->state[2]) == ZCL_BUSY);
    if (expected_size < 240) CHECK(recover_again(&fixture, data, &current) == 0);
    else CHECK(memcmp(current.tail, data->state[2], 80) == 0);
    CHECK(verify_preserved(&fixture, data) == 0);
    return fixture_close(&fixture);
}

static int short_repair(const change_storage_data *data)
{
    storage_fixture fixture;
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(initial_partial(&fixture, data, &snapshot) == 0);
    storage_faults_reset();
    storage_write_fault.mode = IO_SHORT;
    storage_pread_fault.mode = IO_SHORT;
    CHECK(repair(&fixture, data, &snapshot, data->state[2]) == ZCL_OK);
    CHECK(storage_write_fault.calls == 120 && storage_pread_fault.calls == 80);
    storage_faults_reset();
    CHECK(verify_preserved(&fixture, data) == 0);
    CHECK(change_bytes(&fixture, data->state[2], 80, 160) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(short_repair(&data) == 0);
    static const io_mode bad[] = {IO_ZERO, IO_ERROR, IO_OVERSIZE, IO_INTERRUPT};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        CHECK(failed_repair(&data, &storage_write_fault, bad[i], 0, 120) == 0);
        CHECK(failed_repair(&data, &storage_pread_fault, bad[i], 0, 120) == 0);
    }
    CHECK(failed_repair(&data, &storage_write_fault, IO_PARTIAL_ERROR, 1, 140) == 0);
    CHECK(failed_repair(&data, &storage_write_fault, IO_ERROR, 2, 160) == 0);
    CHECK(failed_repair(&data, &storage_write_fault, IO_PARTIAL_ERROR, 2, 200) == 0);
    for (size_t at = 1; at <= 3; ++at)
        CHECK(failed_repair(&data, &storage_sync_fault, IO_ERROR, at, at >= 2 ? 240 : 120) == 0);
    for (size_t at = 1; at <= 5; ++at)
        CHECK(failed_repair(&data, &storage_close_fault, IO_INTERRUPT, at, at >= 3 ? 240 : 120) == 0);
    puts("change repair faults: padding/replacement failures, repeated prefix preservation, conservative advancement and descriptor cleanup passed");
    return 0;
}
