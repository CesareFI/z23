/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_faults.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int descriptors(size_t *count)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    size_t found = 0;
    errno = 0;
    while (found < 256 && readdir(directory) != NULL) ++found;
    int error = errno;
    CHECK(closedir(directory) == 0);
    CHECK(error == 0 && found < 256);
    *count = found;
    return 0;
}

static int file_length(const storage_fixture *fixture, const char *name, off_t expected)
{
    struct stat info = {0};
    int result = fstatat(fixture->directory, name, &info, AT_SYMLINK_NOFOLLOW);
    if (expected < 0) CHECK(result != 0 && errno == ENOENT);
    else CHECK(result == 0 && info.st_size == expected);
    return 0;
}

static int successful_retries(const change_storage_data *data, io_mode mode, size_t at)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_write_fault = (io_fault){mode, at, 0};
    if (mode == IO_INTERRUPT) storage_sync_fault = (io_fault){mode, 2, 0};
    CHECK(change_create(&fixture, data) == ZCL_OK);
    CHECK(storage_write_fault.calls == (mode == IO_SHORT ? 80 + data->wallet_len : 3));
    storage_faults_reset();
    storage_pread_fault = (io_fault){mode, at, 0};
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    CHECK(storage_pread_fault.calls == (mode == IO_SHORT ? 80 : 2));
    storage_faults_reset();
    storage_write_fault = (io_fault){mode, at, 0};
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_OK);
    CHECK(storage_write_fault.calls == (mode == IO_SHORT ? 80 : 2));
    storage_faults_reset();
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture, data->state[1], 80, 80) == 0);
    return fixture_close(&fixture);
}

static int failed_creation(const change_storage_data *data, io_fault *fault,
    io_mode mode, size_t at, bool state_exists, bool wallet_started)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    storage_faults_reset();
    *fault = (io_fault){mode, at, 0};
    CHECK(change_create(&fixture, data) != ZCL_OK);
    CHECK(fault->calls <= 256);
    storage_faults_reset();
    CHECK(descriptors(&after) == 0 && before == after);
    if (state_exists) {
        CHECK(change_create(&fixture, data) == ZCL_ALREADY_EXISTS);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_ALREADY_EXISTS);
    }
    uint8_t wallet[140] = {0};
    size_t length = 0;
    bool pending = false;
    zcl_status status = fixture_read(&fixture, wallet, sizeof(wallet), &length, &pending);
    if (wallet_started) {
        CHECK(status == ZCL_OK && length == data->wallet_len && memcmp(wallet, data->wallet, length) == 0);
        CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
        CHECK(fixture_promote(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    } else CHECK(status == ZCL_NOT_FOUND);
    return fixture_close(&fixture);
}

static int failed_append(const change_storage_data *data, io_fault *fault,
    io_mode mode, size_t at, uint32_t expected_bytes)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot snapshot = {0}, after_snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    storage_faults_reset();
    *fault = (io_fault){mode, at, 0};
    CHECK(change_append(&fixture, data, &snapshot, 1) != ZCL_OK);
    CHECK(fault->calls <= 256);
    storage_faults_reset();
    CHECK(descriptors(&after) == 0 && before == after);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_observe(&fixture, data, &after_snapshot) == ZCL_OK);
    CHECK(after_snapshot.file_bytes == expected_bytes);
    if (expected_bytes == 160) {
        CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_BUSY);
        CHECK(change_append(&fixture, data, &after_snapshot, 2) == ZCL_OK);
        CHECK(change_bytes(&fixture, data->state[1], 80, 80) == 0);
    } else if (expected_bytes == 120) {
        CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_BUSY);
        CHECK(change_append(&fixture, data, &after_snapshot, 1) == ZCL_INVALID_ENCODING);
        CHECK(change_bytes(&fixture, data->state[1], 40, 80) == 0);
    } else CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_OK);
    return fixture_close(&fixture);
}

static int failed_observe(const change_storage_data *data, io_fault *fault, io_mode mode, size_t at)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot snapshot, before_snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    memcpy(&before_snapshot, &snapshot, sizeof(snapshot));
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    storage_faults_reset();
    *fault = (io_fault){mode, at, 0};
    CHECK(change_observe(&fixture, data, &snapshot) != ZCL_OK);
    CHECK(fault->calls <= 256);
    CHECK(memcmp(&snapshot, &before_snapshot, sizeof(snapshot)) == 0);
    storage_faults_reset();
    CHECK(descriptors(&after) == 0 && before == after);
    CHECK(file_length(&fixture, ".change.index", 80) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == 80);
    return fixture_close(&fixture);
}

static int bounded_faults(const change_storage_data *data)
{
    static const io_mode failures[] = {IO_ZERO, IO_ERROR, IO_OVERSIZE, IO_INTERRUPT, IO_PARTIAL_ERROR};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        io_mode mode = failures[i];
        CHECK(failed_creation(data, &storage_write_fault, mode, 0, true, false) == 0);
        CHECK(failed_append(data, &storage_write_fault, mode, 0, mode == IO_PARTIAL_ERROR ? 120 : 80) == 0);
        CHECK(failed_observe(data, &storage_pread_fault, mode, 0) == 0);
        CHECK(failed_append(data, &storage_pread_fault, mode, 0, 80) == 0);
    }
    for (size_t at = 1; at <= 6; ++at)
        CHECK(failed_creation(data, &storage_sync_fault, IO_ERROR, at, at >= 2, at >= 4) == 0);
    for (size_t at = 1; at <= 5; ++at) {
        CHECK(failed_creation(data, &storage_close_fault, IO_INTERRUPT, at, at >= 2, at >= 3) == 0);
        CHECK(failed_append(data, &storage_close_fault, IO_INTERRUPT, at, at >= 3 ? 160 : 80) == 0);
        CHECK(failed_observe(data, &storage_close_fault, IO_INTERRUPT, at) == 0);
    }
    for (size_t at = 1; at <= 3; ++at)
        CHECK(failed_append(data, &storage_sync_fault, IO_ERROR, at, at >= 2 ? 160 : 80) == 0);
    CHECK(failed_creation(data, &storage_rename_fault, IO_ERROR, 1, true, true) == 0);
    return 0;
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(successful_retries(&data, IO_SHORT, 0) == 0);
    CHECK(successful_retries(&data, IO_INTERRUPT, 1) == 0);
    CHECK(bounded_faults(&data) == 0);
    puts("change storage faults: bounded short/EINTR IO, partial preservation, durability/close refusal, consumption and descriptor cleanup passed");
    return 0;
}
