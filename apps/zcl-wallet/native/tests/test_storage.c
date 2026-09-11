/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int create_read(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t actual[142], before[142], changed[140];
    size_t actual_len = 777;
    bool pending = true;
    CHECK(fixture_open(&fixture) == 0);
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_NOT_FOUND);
    CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(S_ISREG(info.st_mode) && (info.st_mode & 0777) == 0600 && info.st_nlink == 1);
    CHECK(fstatat(fixture.directory, ".wallet.pending", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    memcpy(changed, record, length);
    changed[length - 1] ^= 1;
    CHECK(fixture_create(&fixture, changed, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_promote(&fixture, changed, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    for (size_t capacity = 0; capacity < length; ++capacity) {
        CHECK(fixture_read(&fixture, actual, capacity, &actual_len, &pending) == ZCL_BUFFER_TOO_SMALL);
        CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
    }
    CHECK(fixture_read(&fixture, actual + 1, 140, &actual_len, &pending) == ZCL_OK);
    CHECK(actual_len == length && !pending && memcmp(actual + 1, record, length) == 0);
    CHECK(actual[0] == 0xa5 && actual[141] == 0xa5);
    return fixture_close(&fixture);
}

static int recovery(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t actual[140] = {0}, changed[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, ".wallet.pending", record, length) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK);
    CHECK(pending && actual_len == length && memcmp(actual, record, length) == 0);
    memcpy(changed, record, length);
    changed[length - 1] ^= 1;
    CHECK(fixture_promote(&fixture, changed, length) == ZCL_INVALID_ENCODING);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK && !pending);
    return fixture_close(&fixture);
}

static int malformed_files(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t actual[140], before[140];
    size_t actual_len = 777;
    bool pending = true;
    CHECK(fixture_open(&fixture) == 0);
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    CHECK(fixture_write(&fixture, ".wallet.pending", record, length) == 0);
    for (size_t size = 0; size < length; ++size) {
        CHECK(fixture_write(&fixture, "wallet.zcl", record, size) == 0);
        CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_INVALID_ENCODING);
        CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
        CHECK(fixture_promote(&fixture, record, length) == ZCL_INVALID_ENCODING);
        CHECK(unlinkat(fixture.directory, "wallet.zcl", 0) == 0);
    }
    uint8_t oversized[141] = {0};
    CHECK(fixture_write(&fixture, "wallet.zcl", oversized, sizeof(oversized)) == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_INVALID_ENCODING);
    CHECK(unlinkat(fixture.directory, "wallet.zcl", 0) == 0);
    CHECK(fixture_write(&fixture, "wallet.zcl", oversized, length) == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) != ZCL_OK);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    return fixture_close(&fixture);
}

static int file_types(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, "target", record, length) == 0);
    CHECK(symlinkat("target", fixture.directory, "wallet.zcl") == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_IO_FAILURE);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(unlinkat(fixture.directory, "wallet.zcl", 0) == 0);
    CHECK(mkfifoat(fixture.directory, "wallet.zcl", 0600) == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, "wallet.zcl", 0) == 0);
    CHECK(fixture_write(&fixture, "wallet.zcl", record, length) == 0);
    CHECK(fchmodat(fixture.directory, "wallet.zcl", 0644, 0) == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_IO_FAILURE);
    CHECK(fchmod(fixture.directory, 0755) == 0);
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_IO_FAILURE);
    CHECK(fchmod(fixture.directory, 0700) == 0);
    return fixture_close(&fixture);
}

static int lock_validation(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, "target", record, length) == 0);
    CHECK(symlinkat("target", fixture.directory, ".lock") == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".lock", 0) == 0);
    CHECK(fixture_write(&fixture, ".lock", record, length) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".lock", 0) == 0);
    CHECK(linkat(fixture.directory, "target", fixture.directory, ".lock", 0) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".lock", 0) == 0);
    int fd = openat(fixture.directory, ".lock", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_BUSY);
    CHECK(close(fd) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    return fixture_close(&fixture);
}

static int bounds(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t output[140] = {0};
    size_t output_len = 0;
    bool pending = false;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(zcl_storage_create(NULL, 1, record, length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_create((const uint8_t *)fixture.path, SIZE_MAX, record, length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_storage_create((const uint8_t *)fixture.path, 0, record, length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_storage_create((const uint8_t *)fixture.path, fixture_path_len() + 1, record, length) == ZCL_INVALID_ENCODING);
    static const char *const bad_paths[] = {"relative", "/", "/tmp/.", "/tmp/..", "/tmp//wallet", "/tmp/wallet/"};
    for (size_t i = 0; i < sizeof(bad_paths) / sizeof(bad_paths[0]); ++i)
        CHECK(zcl_storage_create((const uint8_t *)bad_paths[i], strlen(bad_paths[i]), record, length) == ZCL_INVALID_ENCODING);
    CHECK(fixture_create(&fixture, NULL, length) == ZCL_INVALID_ARGUMENT);
    CHECK(fixture_create(&fixture, record, SIZE_MAX) == ZCL_OUT_OF_RANGE);
    CHECK(fixture_read(&fixture, NULL, sizeof(output), &output_len, &pending) == ZCL_INVALID_ARGUMENT);
    CHECK(fixture_read(&fixture, output, sizeof(output), NULL, &pending) == ZCL_INVALID_ARGUMENT);
    CHECK(fixture_read(&fixture, output, sizeof(output), &output_len, NULL) == ZCL_INVALID_ARGUMENT);
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    if (fixture_record(record, sizeof(record), &length) || create_read(record, length) ||
        recovery(record, length) || malformed_files(record, length) || file_types(record, length) ||
        lock_validation(record, length) || bounds(record, length))
        return 1;
    puts("storage: no-overwrite, pending recovery, corrupt-file refusal, permissions, locks and bounds passed");
    return 0;
}
