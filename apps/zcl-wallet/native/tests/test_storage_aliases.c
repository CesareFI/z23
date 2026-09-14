/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int linked_record_unchanged(const storage_fixture *fixture, const char *name,
    const uint8_t *record, size_t length, ino_t identity)
{
    struct stat info = {0};
    CHECK(fstatat(fixture->directory, name, &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(info.st_ino == identity && info.st_nlink == 2 && info.st_size == (off_t)length);
    int fd = openat(fixture->directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    uint8_t actual[141] = {0};
    ssize_t count = read(fd, actual, sizeof(actual));
    int closed = close(fd);
    CHECK(count >= 0 && (size_t)count == length && closed == 0);
    CHECK(memcmp(actual, record, length) == 0);
    return 0;
}

static int hardlink_record(const uint8_t *record, size_t length, const char *name, bool is_pending)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, "target", record, length) == 0);
    CHECK(linkat(fixture.directory, "target", fixture.directory, name, 0) == 0);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, "target", &info, AT_SYMLINK_NOFOLLOW) == 0);
    uint8_t actual[142], before[142];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = SIZE_MAX;
    bool pending = !is_pending;
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_IO_FAILURE);
    CHECK(actual_len == SIZE_MAX && pending == !is_pending && memcmp(actual, before, sizeof(actual)) == 0);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_IO_FAILURE);
    CHECK(linked_record_unchanged(&fixture, name, record, length, info.st_ino) == 0);
    CHECK(linked_record_unchanged(&fixture, "target", record, length, info.st_ino) == 0);
    // Only this fixture removes its own extra alias; storage never repairs it.
    CHECK(unlinkat(fixture.directory, "target", 0) == 0);
    CHECK(fixture_read(&fixture, actual + 1, 140, &actual_len, &pending) == ZCL_OK);
    CHECK(actual_len == length && pending == is_pending && memcmp(actual + 1, record, length) == 0);
    CHECK(actual[0] == 0xa5 && actual[141] == 0xa5);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    CHECK(fixture_record(record, sizeof(record), &length) == 0);
    CHECK(hardlink_record(record, length, "wallet.zcl", false) == 0);
    CHECK(hardlink_record(record, length, ".wallet.pending", true) == 0);
    puts("storage aliases: real linked records refuse without mutation and retry with one link");
    return 0;
}
