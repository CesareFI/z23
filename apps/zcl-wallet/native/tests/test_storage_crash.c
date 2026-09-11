/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { CRASH_NONE, CRASH_PARTIAL, CRASH_WRITE, CRASH_FILE_SYNC, CRASH_PENDING_SYNC, CRASH_RENAME } crash_point;
static crash_point selected_crash;
static size_t sync_count;

ssize_t __real_write(int fd, const void *bytes, size_t length);
int __real_fsync(int fd);
int __real_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);
ssize_t __wrap_write(int fd, const void *bytes, size_t length);
int __wrap_fsync(int fd);
int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);

ssize_t __wrap_write(int fd, const void *bytes, size_t length)
{
    size_t count = selected_crash == CRASH_PARTIAL ? length / 2 : length;
    ssize_t result = __real_write(fd, bytes, count);
    if (selected_crash == CRASH_PARTIAL || selected_crash == CRASH_WRITE)
        _exit(result >= 0 && (size_t)result == count ? 77 : 78);
    return result;
}

int __wrap_fsync(int fd)
{
    int result = __real_fsync(fd);
    ++sync_count;
    if ((selected_crash == CRASH_FILE_SYNC && sync_count == 2) ||
        (selected_crash == CRASH_PENDING_SYNC && sync_count == 3))
        _exit(result == 0 ? 77 : 78);
    return result;
}

int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags)
{
    int result = __real_renameat2(olddir, oldpath, newdir, newpath, flags);
    if (selected_crash == CRASH_RENAME)
        _exit(result == 0 ? 77 : 78);
    return result;
}

static int wait_for(pid_t child, int *exit_code)
{
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    *exit_code = WEXITSTATUS(status);
    return 0;
}

static int crash_case(const uint8_t *record, size_t length, crash_point point)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        selected_crash = point;
        sync_count = 0;
        (void)fixture_create(&fixture, record, length);
        _exit(79); /* The intended interruption must have happened. */
    }
    int exit_code = 0;
    CHECK(wait_for(child, &exit_code) == 0 && exit_code == 77);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    zcl_status status = fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending);
    if (point == CRASH_PARTIAL) {
        CHECK(status == ZCL_INVALID_ENCODING);
        CHECK(fixture_promote(&fixture, record, length) == ZCL_INVALID_ENCODING);
    } else {
        CHECK(status == ZCL_OK && actual_len == length && memcmp(actual, record, length) == 0);
        CHECK(pending == (point < CRASH_RENAME));
        CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
        CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK && !pending);
    }
    return fixture_close(&fixture);
}

static int competing_creators(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    pid_t children[12] = {0};
    for (size_t i = 0; i < sizeof(children) / sizeof(children[0]); ++i) {
        children[i] = fork();
        CHECK(children[i] >= 0);
        if (children[i] == 0) {
            zcl_status status = fixture_create(&fixture, record, length);
            if (status == ZCL_OK)
                _exit(10);
            if (status == ZCL_BUSY || status == ZCL_ALREADY_EXISTS)
                _exit(11);
            _exit(12);
        }
    }
    size_t winners = 0;
    for (size_t i = 0; i < sizeof(children) / sizeof(children[0]); ++i) {
        int exit_code = 0;
        CHECK(wait_for(children[i], &exit_code) == 0);
        CHECK(exit_code == 10 || exit_code == 11);
        if (exit_code == 10)
            ++winners;
    }
    CHECK(winners == 1);
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = true;
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK);
    CHECK(!pending && actual_len == length && memcmp(actual, record, length) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    CHECK(fixture_record(record, sizeof(record), &length) == 0);
    for (unsigned point = CRASH_PARTIAL; point <= CRASH_RENAME; ++point)
        CHECK(crash_case(record, length, (crash_point)point) == 0);
    CHECK(competing_creators(record, length) == 0);
    puts("storage processes: five interrupted commit stages and twelve competing creators passed");
    return 0;
}
