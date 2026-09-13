/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_faults.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t stat_calls, fail_stat;
static int grow_fd = -1;
int __real_fstat(int fd, struct stat *info);
int __wrap_fstat(int fd, struct stat *info);

int __wrap_fstat(int fd, struct stat *info)
{
    ++stat_calls;
    if (stat_calls == fail_stat) {
        errno = EIO;
        return -1;
    }
    if (grow_fd >= 0 && stat_calls == 6) {
        if (ftruncate(grow_fd, 241) != 0) abort();
    }
    return __real_fstat(fd, info);
}

static void reset(void)
{
    storage_faults_reset();
    stat_calls = fail_stat = 0;
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
    CHECK(fixture_open(fixture) == 0);
    CHECK(fixture_create(fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t records[240] = {0};
    for (size_t i = 0; i < 3; ++i) memcpy(records + i * 80, data->state[i], 80);
    CHECK(fixture_write(fixture, ".change.index", records, sizeof(records)) == 0);
    return 0;
}

static zcl_status probe(const storage_fixture *fixture, const change_storage_data *data,
    zcl_change_recovery_snapshot *snapshot)
{
    return zcl_storage_change_probe((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot);
}

static int positive_retries(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(setup(&fixture, data) == 0);
    reset();
    storage_pread_fault.mode = IO_SHORT;
    zcl_change_recovery_snapshot result = {0};
    CHECK(probe(&fixture, data, &result) == ZCL_OK && storage_pread_fault.calls == 160);
    CHECK(result.has_predecessor && memcmp(result.predecessor, data->state[1], 80) == 0);
    reset();
    storage_pread_fault = (io_fault){IO_INTERRUPT, 2, 0};
    CHECK(probe(&fixture, data, &result) == ZCL_OK && storage_pread_fault.calls == 3);
    CHECK(memcmp(result.current.tail, data->state[2], 80) == 0);
    reset();
    return fixture_close(&fixture);
}

static int failed_probe(const change_storage_data *data, io_fault *fault,
    io_mode mode, size_t at, size_t stat_at)
{
    storage_fixture fixture;
    CHECK(setup(&fixture, data) == 0);
    zcl_change_recovery_snapshot result, before_result;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before_result, &result, sizeof(result));
    size_t before = 0, after = 0;
    CHECK(descriptors(&before) == 0);
    reset();
    fail_stat = stat_at;
    if (fault != NULL) *fault = (io_fault){mode, at, 0};
    CHECK(probe(&fixture, data, &result) != ZCL_OK);
    CHECK(memcmp(&result, &before_result, sizeof(result)) == 0);
    CHECK(storage_pread_fault.calls <= 256);
    if (stat_at != 0) CHECK(stat_calls == stat_at);
    reset();
    CHECK(descriptors(&after) == 0 && before == after);
    CHECK(probe(&fixture, data, &result) == ZCL_OK && result.has_predecessor);
    CHECK(memcmp(result.predecessor, data->state[1], 80) == 0);
    CHECK(memcmp(result.current.tail, data->state[2], 80) == 0);
    reset();
    return fixture_close(&fixture);
}

static int changed_size(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(setup(&fixture, data) == 0);
    grow_fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(grow_fd >= 0);
    zcl_change_recovery_snapshot result, before_result;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before_result, &result, sizeof(result));
    reset();
    CHECK(probe(&fixture, data, &result) == ZCL_BUSY && stat_calls == 6);
    CHECK(memcmp(&result, &before_result, sizeof(result)) == 0);
    int owned = grow_fd;
    grow_fd = -1;
    reset();
    CHECK(close(owned) == 0);
    CHECK(probe(&fixture, data, &result) == ZCL_OK && result.current.file_bytes == 241);
    CHECK(result.has_predecessor && memcmp(result.predecessor, data->state[2], 80) == 0);
    reset();
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(positive_retries(&data) == 0);
    static const io_mode failures[] = {IO_ZERO, IO_OVERSIZE, IO_ERROR};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
        for (size_t at = 1; at <= 2; ++at)
            CHECK(failed_probe(&data, &storage_pread_fault, failures[i], at, 0) == 0);
    CHECK(failed_probe(&data, &storage_pread_fault, IO_INTERRUPT, 0, 0) == 0);
    for (size_t at = 1; at <= 5; ++at)
        CHECK(failed_probe(&data, &storage_close_fault, IO_INTERRUPT, at, 0) == 0);
    for (size_t at = 1; at <= 6; ++at)
        CHECK(failed_probe(&data, NULL, IO_NORMAL, 0, at) == 0);
    CHECK(changed_size(&data) == 0);
    puts("change probe faults: tail/predecessor IO, bounded EINTR, every stat/close, changed size and descriptor cleanup passed");
    return 0;
}
