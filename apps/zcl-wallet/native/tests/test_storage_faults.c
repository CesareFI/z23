/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"
#include "storage_faults.h"

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__GLIBC__) || defined(__ANDROID__)
ssize_t __wrap___read_chk(int, void *, size_t, size_t);
ssize_t __wrap___pread_chk(int, void *, size_t, off_t, size_t);
#if defined(__ANDROID__)
ssize_t __wrap___write_chk(int, const void *, size_t, size_t);
#endif

static int fortified_refusal(unsigned operation, io_mode mode)
{
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        const struct rlimit no_core = {0, 0};
        if (setrlimit(RLIMIT_CORE, &no_core) != 0) _exit(81);
        uint8_t bytes[2] = {0};
        storage_read_fault.mode = storage_pread_fault.mode = storage_write_fault.mode = mode;
        /* Deliberately smaller declared object bound. Even a short/zero/error
         * injection must reach libc's refusal before changing this request. */
        if (operation == 0) (void)__wrap___read_chk(-1, bytes, 2, 1);
        if (operation == 1) (void)__wrap___pread_chk(-1, bytes, 2, 0, 1);
#if defined(__ANDROID__)
        if (operation == 2) (void)__wrap___write_chk(-1, bytes, 2, 1);
#endif
        _exit(79);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    return 0;
}

static int fortified_bounds(void)
{
    static const io_mode modes[] = {IO_SHORT, IO_ZERO, IO_ERROR};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        CHECK(fortified_refusal(0, modes[i]) == 0);
        CHECK(fortified_refusal(1, modes[i]) == 0);
#if defined(__ANDROID__)
        CHECK(fortified_refusal(2, modes[i]) == 0);
#endif
    }
    return 0;
}
#endif

static int confirm_record(const storage_fixture *fixture, const uint8_t *record, size_t length, bool expected_pending)
{
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_read(fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK);
    CHECK(actual_len == length && pending == expected_pending && memcmp(actual, record, length) == 0);
    return 0;
}

static int short_io(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_write_fault.mode = IO_SHORT;
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    CHECK(storage_write_fault.calls == length);
    storage_faults_reset();
    storage_read_fault.mode = IO_SHORT;
    CHECK(confirm_record(&fixture, record, length, false) == 0);
    CHECK(storage_read_fault.calls == length + 1);
    storage_faults_reset();
    return fixture_close(&fixture);
}

static int interrupted_io(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_write_fault = (io_fault){IO_INTERRUPT, 1, 0};
    storage_sync_fault = (io_fault){IO_INTERRUPT, 1, 0};
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    CHECK(storage_write_fault.calls == 2 && storage_sync_fault.calls == 5);
    for (size_t at = 1; at <= 2; ++at) {
        storage_faults_reset();
        storage_read_fault = (io_fault){IO_INTERRUPT, at, 0};
        CHECK(confirm_record(&fixture, record, length, false) == 0);
        CHECK(storage_read_fault.calls == 3);
    }
    storage_faults_reset();
    return fixture_close(&fixture);
}

static int failed_write(const uint8_t *record, size_t length, io_mode mode)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_write_fault.mode = mode;
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_UNCERTAIN);
    CHECK(storage_write_fault.calls == (mode == IO_INTERRUPT ? 256 : 1));
    storage_faults_reset();
    uint8_t actual[140];
    memset(actual, 0xa5, sizeof(actual));
    size_t actual_len = 777;
    bool pending = false;
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_INVALID_ENCODING);
    CHECK(actual_len == 777 && !pending);
    for (size_t i = 0; i < sizeof(actual); ++i)
        CHECK(actual[i] == 0xa5);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_INVALID_ENCODING);
    return fixture_close(&fixture);
}

static int interrupted_commit(const uint8_t *record, size_t length, io_fault *fault, size_t at, bool expected_pending)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    *fault = (io_fault){IO_ERROR, at, 0};
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_UNCERTAIN);
    storage_faults_reset();
    CHECK(confirm_record(&fixture, record, length, expected_pending) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    CHECK(confirm_record(&fixture, record, length, false) == 0);
    return fixture_close(&fixture);
}

static int parent_sync(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_sync_fault.mode = IO_INTERRUPT;
    CHECK(fixture_create(&fixture, record, length) == ZCL_IO_UNCERTAIN);
    CHECK(storage_sync_fault.calls == 16);
    storage_faults_reset();
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_NOT_FOUND);
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    return fixture_close(&fixture);
}

static int failed_read(const uint8_t *record, size_t length, io_mode mode, size_t at, zcl_status expected)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    uint8_t actual[140], before[140];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = 777;
    bool pending = true;
    storage_faults_reset();
    storage_read_fault = (io_fault){mode, at, 0};
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) == expected);
    CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
    CHECK(storage_read_fault.calls <= 256);
    storage_faults_reset();
    CHECK(confirm_record(&fixture, record, length, false) == 0);
    return fixture_close(&fixture);
}

static int failed_close_read(const uint8_t *record, size_t length, size_t at)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    uint8_t actual[140], before[140];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = 777;
    bool pending = true;
    storage_faults_reset();
    storage_close_fault = (io_fault){IO_INTERRUPT, at, 0};
    CHECK(fixture_read(&fixture, actual, sizeof(actual), &actual_len, &pending) != ZCL_OK);
    CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
    /* parent, optional record, lock, directory: a failed close is never retried. */
    CHECK(storage_close_fault.calls == (at == 1 ? 2 : 4));
    storage_faults_reset();
    CHECK(confirm_record(&fixture, record, length, false) == 0);
    return fixture_close(&fixture);
}

static int failed_promotion(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, ".wallet.pending", record, length) == 0);
    storage_faults_reset();
    storage_sync_fault = (io_fault){IO_ERROR, 2, 0};
    CHECK(fixture_promote(&fixture, record, length) == ZCL_IO_UNCERTAIN);
    CHECK(storage_rename_fault.calls == 0);
    storage_faults_reset();
    CHECK(confirm_record(&fixture, record, length, true) == 0);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    storage_faults_reset();
    storage_sync_fault = (io_fault){IO_ERROR, 2, 0};
    CHECK(fixture_promote(&fixture, record, length) == ZCL_IO_UNCERTAIN);
    storage_faults_reset();
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    return fixture_close(&fixture);
}

static int kernel_collision(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    storage_faults_reset();
    storage_rename_fault.mode = IO_COLLISION;
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    storage_faults_reset();
    int fd = openat(fixture.directory, "wallet.zcl", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    uint8_t bytes[140] = {0};
    ssize_t count = read(fd, bytes, sizeof(bytes));
    int closed = close(fd);
    CHECK(count == 5 && closed == 0 && memcmp(bytes, "older", 5) == 0);
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_read(&fixture, bytes, sizeof(bytes), &actual_len, &pending) == ZCL_INVALID_ENCODING);
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    return fixture_close(&fixture);
}

int main(void)
{
#if defined(__GLIBC__) || defined(__ANDROID__)
    CHECK(fortified_bounds() == 0);
#endif
    uint8_t record[140] = {0};
    size_t length = 0;
    CHECK(fixture_record(record, sizeof(record), &length) == 0);
    CHECK(short_io(record, length) == 0);
    CHECK(interrupted_io(record, length) == 0);
    static const io_mode bad_writes[] = {IO_ZERO, IO_ERROR, IO_OVERSIZE, IO_INTERRUPT};
    for (size_t i = 0; i < sizeof(bad_writes) / sizeof(bad_writes[0]); ++i)
        CHECK(failed_write(record, length, bad_writes[i]) == 0);
    for (size_t at = 2; at <= 4; ++at) {
        CHECK(interrupted_commit(record, length, &storage_sync_fault, at, at <= 3) == 0);
        CHECK(interrupted_commit(record, length, &storage_close_fault, at, at == 2) == 0);
    }
    CHECK(interrupted_commit(record, length, &storage_rename_fault, 1, true) == 0);
    CHECK(parent_sync(record, length) == 0);
    CHECK(failed_read(record, length, IO_ZERO, 1, ZCL_INVALID_ENCODING) == 0);
    CHECK(failed_read(record, length, IO_OVERSIZE, 1, ZCL_INVALID_ENCODING) == 0);
    CHECK(failed_read(record, length, IO_OVERSIZE, 2, ZCL_INVALID_ENCODING) == 0);
    CHECK(failed_read(record, length, IO_ERROR, 1, ZCL_IO_FAILURE) == 0);
    CHECK(failed_read(record, length, IO_ERROR, 2, ZCL_IO_FAILURE) == 0);
    CHECK(failed_read(record, length, IO_INTERRUPT, 0, ZCL_IO_FAILURE) == 0);
    for (size_t at = 1; at <= 4; ++at)
        CHECK(failed_close_read(record, length, at) == 0);
    CHECK(failed_promotion(record, length) == 0);
    CHECK(kernel_collision(record, length) == 0);
    puts("storage faults: bounded retries, short IO, full disks, flush/rename/close failures and durable recovery passed");
    return 0;
}
