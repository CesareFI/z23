/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { STOP_NONE, STOP_WRITE, STOP_SYNC, STOP_RENAME, STOP_CLOSE } stop_kind;
static stop_kind selected;
static size_t selected_at, calls[5];
static bool partial;

ssize_t __real_write(int fd, const void *bytes, size_t length);
int __real_fsync(int fd);
int __real_close(int fd);
int __real_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);
ssize_t __wrap_write(int fd, const void *bytes, size_t length);
int __wrap_fsync(int fd);
int __wrap_close(int fd);
int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);

static bool stopping(stop_kind kind)
{
    ++calls[kind];
    return selected == kind && calls[kind] == selected_at;
}

static ssize_t after_write(bool stop, size_t wanted, ssize_t result)
{
    if (stop) _exit(result >= 0 && (size_t)result == wanted ? 77 : 78);
    return result;
}

ssize_t __wrap_write(int fd, const void *bytes, size_t length)
{
    bool stop = stopping(STOP_WRITE);
    size_t wanted = stop && partial ? length / 2 : length;
    return after_write(stop, wanted, __real_write(fd, bytes, wanted));
}

#if defined(__ANDROID__)
ssize_t __real___write_chk(int, const void *, size_t, size_t);
ssize_t __wrap___write_chk(int, const void *, size_t, size_t);

ssize_t __wrap___write_chk(int fd, const void *bytes, size_t length, size_t capacity)
{
    /* Preserve Bionic's original bounds refusal before partial-write injection.
     * The release archive may use this entry point instead of ordinary write. */
    if (length > capacity) return __real___write_chk(fd, bytes, length, capacity);
    const bool stop = stopping(STOP_WRITE);
    const size_t wanted = stop && partial ? length / 2 : length;
    return after_write(stop, wanted, __real___write_chk(fd, bytes, wanted, capacity));
}

static int fortified_bounds(void)
{
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        const struct rlimit no_core = {0, 0};
        if (setrlimit(RLIMIT_CORE, &no_core) != 0) _exit(81);
        selected = STOP_WRITE;
        selected_at = 1;
        partial = true;
        memset(calls, 0, sizeof(calls));
        const uint8_t bytes[2] = {0};
        (void)__wrap___write_chk(-1, bytes, sizeof(bytes), 1);
        _exit(79);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    return 0;
}
#endif

int __wrap_fsync(int fd)
{
    int result = __real_fsync(fd);
    if (stopping(STOP_SYNC)) _exit(result == 0 ? 77 : 78);
    return result;
}

int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (stopping(STOP_CLOSE)) _exit(result == 0 ? 77 : 78);
    return result;
}

int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags)
{
    int result = __real_renameat2(olddir, oldpath, newdir, newpath, flags);
    if (stopping(STOP_RENAME)) _exit(result == 0 ? 77 : 78);
    return result;
}

static int wait_child(pid_t child, int *code)
{
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status));
    *code = WEXITSTATUS(status);
    return 0;
}

static int inspect_creation(const storage_fixture *fixture, const change_storage_data *data)
{
    struct stat state = {0};
    int found = fstatat(fixture->directory, ".change.index", &state, AT_SYMLINK_NOFOLLOW);
    if (found != 0) {
        CHECK(errno == ENOENT);
        CHECK(change_create(fixture, data) == ZCL_OK);
        return 0;
    }
    CHECK(change_create(fixture, data) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_create(fixture, data->wallet, data->wallet_len) == ZCL_ALREADY_EXISTS);
    CHECK(state.st_size == 40 || state.st_size == 80);
    CHECK(change_bytes(fixture, data->state[0], (size_t)state.st_size, 0) == 0);
    uint8_t wallet[140] = {0};
    size_t wallet_len = 0;
    bool pending = false;
    zcl_status status = fixture_read(fixture, wallet, sizeof(wallet), &wallet_len, &pending);
    if (status == ZCL_OK) {
        CHECK(state.st_size == 80 && wallet_len == data->wallet_len);
        CHECK(memcmp(wallet, data->wallet, wallet_len) == 0);
        CHECK(fixture_promote(fixture, data->wallet, data->wallet_len) == ZCL_OK);
        zcl_change_storage_snapshot snapshot = {0};
        CHECK(change_observe(fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == 80);
    } else {
        CHECK(status == ZCL_ALREADY_EXISTS || status == ZCL_INVALID_ENCODING);
        if (status == ZCL_INVALID_ENCODING) CHECK(state.st_size == 80);
    }
    return 0;
}

static int inspect_append(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *old)
{
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(fixture, data, &current) == ZCL_OK);
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    if (current.file_bytes == 80) CHECK(change_append(fixture, data, old, 1) == ZCL_OK);
    else if (current.file_bytes == 120) {
        CHECK(change_append(fixture, data, old, 1) == ZCL_BUSY);
        CHECK(change_append(fixture, data, &current, 1) == ZCL_INVALID_ENCODING);
        CHECK(change_bytes(fixture, data->state[1], 40, 80) == 0);
    } else {
        CHECK(current.file_bytes == 160 && memcmp(current.tail, data->state[1], 80) == 0);
        CHECK(change_append(fixture, data, old, 1) == ZCL_BUSY);
        CHECK(change_append(fixture, data, &current, 2) == ZCL_OK);
        CHECK(change_bytes(fixture, data->state[1], 80, 80) == 0);
    }
    return 0;
}

static int crash_case(const change_storage_data *data, bool append, stop_kind kind, size_t at, bool short_write)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    zcl_change_storage_snapshot old = {0};
    if (append) {
        CHECK(change_create(&fixture, data) == ZCL_OK);
        CHECK(change_observe(&fixture, data, &old) == ZCL_OK);
    }
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        selected = kind;
        selected_at = at;
        partial = short_write;
        memset(calls, 0, sizeof(calls));
        if (append) (void)change_append(&fixture, data, &old, 1);
        else (void)change_create(&fixture, data);
        _exit(79); /* Every selected boundary must actually be reached. */
    }
    int code = 0;
    CHECK(wait_child(child, &code) == 0 && code == 77);
    if (append) CHECK(inspect_append(&fixture, data, &old) == 0);
    else CHECK(inspect_creation(&fixture, data) == 0);
    return fixture_close(&fixture);
}

static int concurrent_append(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot old = {0};
    CHECK(change_observe(&fixture, data, &old) == ZCL_OK);
    pid_t children[12] = {0};
    for (size_t i = 0; i < 12; ++i) {
        children[i] = fork();
        CHECK(children[i] >= 0);
        if (children[i] == 0) {
            zcl_status status = change_append(&fixture, data, &old, 1);
            _exit(status == ZCL_OK ? 10 : (status == ZCL_BUSY ? 11 : 12));
        }
    }
    size_t winners = 0;
    for (size_t i = 0; i < 12; ++i) {
        int code = 0;
        CHECK(wait_child(children[i], &code) == 0 && (code == 10 || code == 11));
        if (code == 10) ++winners;
    }
    CHECK(winners == 1);
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(&fixture, data, &current) == ZCL_OK && current.file_bytes == 160);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture, data->state[1], 80, 80) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
#if defined(__ANDROID__)
    CHECK(fortified_bounds() == 0);
#endif
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    for (size_t at = 1; at <= 2; ++at) {
        CHECK(crash_case(&data, false, STOP_WRITE, at, false) == 0);
        CHECK(crash_case(&data, false, STOP_WRITE, at, true) == 0);
    }
    for (size_t at = 1; at <= 6; ++at) CHECK(crash_case(&data, false, STOP_SYNC, at, false) == 0);
    for (size_t at = 1; at <= 5; ++at) CHECK(crash_case(&data, false, STOP_CLOSE, at, false) == 0);
    CHECK(crash_case(&data, false, STOP_RENAME, 1, false) == 0);
    CHECK(crash_case(&data, true, STOP_WRITE, 1, false) == 0);
    CHECK(crash_case(&data, true, STOP_WRITE, 1, true) == 0);
    for (size_t at = 1; at <= 3; ++at) CHECK(crash_case(&data, true, STOP_SYNC, at, false) == 0);
    for (size_t at = 1; at <= 5; ++at) CHECK(crash_case(&data, true, STOP_CLOSE, at, false) == 0);
    CHECK(concurrent_append(&data) == 0);
    puts("change storage processes:26 reached interruption boundaries and12 competing appenders passed; no power-loss simulation claim");
    return 0;
}
