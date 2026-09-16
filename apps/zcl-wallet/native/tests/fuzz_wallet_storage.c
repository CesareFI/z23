/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static storage_fixture fixture;
static uint8_t base_record[140];
static size_t base_length;
static bool initialized;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void require(bool condition) { if (!condition) abort(); }
static void cleanup(void) { require(fixture_close(&fixture) == 0); }

static void initialize(void)
{
    if (initialized) return;
    require(fixture_record(base_record, sizeof(base_record), &base_length) == 0);
    require(fixture_open(&fixture) == 0);
    require(atexit(cleanup) == 0);
    initialized = true;
}

static void clear_files(void)
{
    const char *names[] = {"wallet.zcl", ".wallet.pending", ".change.index", "target"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const int removed = unlinkat(fixture.directory, names[i], 0);
        require(removed == 0 || errno == ENOENT);
    }
}

static zcl_status expected_read(const uint8_t *record, size_t length, size_t capacity, bool aliased)
{
    if (aliased) return ZCL_IO_FAILURE;
    if (length < 124) return ZCL_INVALID_ENCODING;
    zcl_wallet_record parsed = {0};
    const zcl_status status = zcl_wallet_record_parse(record, length, &parsed);
    if (status != ZCL_OK) return status;
    return capacity < length ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
}

static void unchanged_file(const char *name, const uint8_t *record, size_t length, bool aliased)
{
    struct stat info = {0};
    require(fstatat(fixture.directory, name, &info, AT_SYMLINK_NOFOLLOW) == 0);
    require(info.st_size == (off_t)length && info.st_nlink == (nlink_t)(aliased ? 2 : 1));
    const int fd = openat(fixture.directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    require(fd >= 0);
    uint8_t actual[141] = {0};
    const ssize_t count = read(fd, actual, sizeof(actual));
    const int closed = close(fd);
    require(count >= 0 && (size_t)count == length && closed == 0);
    require(memcmp(actual, record, length) == 0);
}

static void read_record(const uint8_t *record, size_t length, size_t capacity, bool pending,
    zcl_status expected)
{
    uint8_t actual[142], before[142];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_length = SIZE_MAX;
    bool actual_pending = !pending;
    const zcl_status status = fixture_read(&fixture, actual + 1, capacity, &actual_length, &actual_pending);
    require(status == expected);
    if (status == ZCL_OK) {
        require(actual_length == length && actual_pending == pending);
        require(memcmp(actual + 1, record, length) == 0);
        require(memcmp(actual + 1 + length, before + 1 + length, sizeof(actual) - 1 - length) == 0);
        require(actual[0] == before[0]);
    } else {
        require(actual_length == SIZE_MAX && actual_pending == !pending);
        require(memcmp(actual, before, sizeof(actual)) == 0);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 142) return 0;
    initialize();
    clear_files();
    uint8_t record[140] = {0};
    memcpy(record, base_record, base_length);
    for (size_t i = 2; i < size; ++i) record[i - 2] ^= data[i];
    const size_t length = (data[0] & 1U) != 0 ? base_length : size - 2;
    const bool pending = (data[0] & 2U) != 0;
    const bool aliased = (data[0] & 4U) != 0;
    const bool orphan = (data[0] & 8U) != 0;
    const char *name = orphan ? ".change.index" : (pending ? ".wallet.pending" : "wallet.zcl");
    require(fixture_write(&fixture, name, record, length) == 0);
    if (aliased) require(linkat(fixture.directory, name, fixture.directory, "target", 0) == 0);
    const size_t capacity = (size_t)data[1] % 141;
    const zcl_status expected = orphan ? ZCL_ALREADY_EXISTS : expected_read(record, length, capacity, aliased);
    read_record(record, length, capacity, pending, expected);
    unchanged_file(name, record, length, aliased);
    if (aliased) unchanged_file("target", record, length, true);
    return 0;
}
