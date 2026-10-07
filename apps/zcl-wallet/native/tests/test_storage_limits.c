/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

enum { DESCRIPTOR_LIMIT = 64 };
typedef enum { CREATE, READ, PROMOTE } operation;

static int descriptor_count(size_t *count)
{
    *count = 0;
    for (int fd = 0; fd < DESCRIPTOR_LIMIT; ++fd) {
        errno = 0;
        if (fcntl(fd, F_GETFD) >= 0) ++*count;
        else CHECK(errno == EBADF);
    }
    return 0;
}

static int consume_descriptors(int source, int owners[DESCRIPTOR_LIMIT], size_t *count)
{
    const struct rlimit limit = {DESCRIPTOR_LIMIT, DESCRIPTOR_LIMIT};
    CHECK(setrlimit(RLIMIT_NOFILE, &limit) == 0);
    *count = 0;
    for (size_t i = 0; i < DESCRIPTOR_LIMIT; ++i) {
        const int fd = fcntl(source, F_DUPFD_CLOEXEC, 0);
        if (fd < 0) {
            CHECK(errno == EMFILE);
            return 0;
        }
        owners[(*count)++] = fd;
    }
    return 1; /* The kernel must have refused within this fixed bound. */
}

static int limited_operation(const storage_fixture *fixture, const uint8_t *record,
    size_t length, operation selected, size_t available)
{
    uint8_t actual[140], before[140];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = 777;
    bool pending = true;
    zcl_status status;
    if (selected == CREATE) status = fixture_create(fixture, record, length);
    else if (selected == PROMOTE) status = fixture_promote(fixture, record, length);
    else status = fixture_read(fixture, actual, sizeof(actual), &actual_len, &pending);
    CHECK(status == (available < 3 ? ZCL_IO_FAILURE : ZCL_OK));
    if (selected != READ) return 0;
    if (available < 3) {
        CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
    } else {
        CHECK(actual_len == length && !pending && memcmp(actual, record, length) == 0);
        CHECK(memcmp(actual + length, before + length, sizeof(actual) - length) == 0);
    }
    return 0;
}

static int pressured_child(const storage_fixture *fixture, const uint8_t *record,
    size_t length, operation selected, size_t available)
{
    int owners[DESCRIPTOR_LIMIT] = {0};
    size_t count = 0;
    CHECK(consume_descriptors(fixture->directory, owners, &count) == 0);
    CHECK(count >= available);
    for (size_t i = 0; i < available; ++i) CHECK(close(owners[--count]) == 0);
    size_t before = 0, after = 0;
    CHECK(descriptor_count(&before) == 0);
    CHECK(before == DESCRIPTOR_LIMIT - available);
    const int result = limited_operation(fixture, record, length, selected, available);
    CHECK(descriptor_count(&after) == 0);
    /* Retire only these filler owners before reporting a regression. Child
     * exit also retires any leaked product descriptor; parent owns the fixture. */
    for (size_t i = 0; i < count; ++i) CHECK(close(owners[i]) == 0);
    CHECK(result == 0 && before == after);
    return 0;
}

static int recovered_operation(const storage_fixture *fixture, const uint8_t *record,
    size_t length, operation selected, size_t available)
{
    if (selected == CREATE && available < 3)
        CHECK(fixture_create(fixture, record, length) == ZCL_OK);
    uint8_t actual[140] = {0};
    size_t actual_len = 0;
    bool pending = false;
    CHECK(fixture_read(fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK);
    CHECK(actual_len == length && memcmp(actual, record, length) == 0);
    CHECK(pending == (selected == PROMOTE && available < 3));
    CHECK(fixture_promote(fixture, record, length) == ZCL_OK);
    CHECK(fixture_read(fixture, actual, sizeof(actual), &actual_len, &pending) == ZCL_OK && !pending);
    CHECK(actual_len == length && memcmp(actual, record, length) == 0);
    return 0;
}

static int pressure_case(const uint8_t *record, size_t length, operation selected, size_t available)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    if (selected == READ) CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    if (selected == PROMOTE) CHECK(fixture_write(&fixture, ".wallet.pending", record, length) == 0);
    const pid_t child = fork();
    if (child == 0) _exit(pressured_child(&fixture, record, length, selected, available) == 0 ? 77 : 78);
    int status = 0;
    const pid_t waited = child < 0 ? -1 : waitpid(child, &status, 0);
    const int recovered = recovered_operation(&fixture, record, length, selected, available);
    const int closed = fixture_close(&fixture);
    CHECK(waited == child && child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 77);
    CHECK(recovered == 0 && closed == 0);
    return 0;
}

static int file_limit_control(const storage_fixture *fixture, size_t limit)
{
    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    CHECK(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGXFSZ, &action, NULL) == 0);
    const struct rlimit bound = {(rlim_t)limit, (rlim_t)limit};
    CHECK(setrlimit(RLIMIT_FSIZE, &bound) == 0);
    const int fd = openat(fixture->directory, "target", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    const uint8_t bytes[140] = {0};
    const ssize_t written = write(fd, bytes, sizeof(bytes));
    CHECK(limit == 0 ? written == -1 && errno == EFBIG : written == (ssize_t)limit);
    CHECK(write(fd, bytes, 1) == -1 && errno == EFBIG);
    CHECK(close(fd) == 0 && unlinkat(fixture->directory, "target", 0) == 0);
    return 0;
}

static int limited_file_child(const storage_fixture *fixture, const uint8_t *record,
    size_t length, size_t limit)
{
    CHECK(file_limit_control(fixture, limit) == 0);
    const zcl_status status = fixture_create(fixture, record, length);
    CHECK(status == (limit < length ? ZCL_IO_UNCERTAIN : ZCL_OK));
    return 0;
}

static int exact_prefix(const storage_fixture *fixture, const char *name,
    const uint8_t *record, size_t length)
{
    const int fd = openat(fixture->directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    struct stat info = {0};
    uint8_t bytes[140] = {0};
    const int stated = fstat(fd, &info);
    const ssize_t got = read(fd, bytes, sizeof(bytes));
    const int closed = close(fd);
    CHECK(stated == 0 && info.st_size == (off_t)length && got == (ssize_t)length && closed == 0);
    CHECK(memcmp(bytes, record, length) == 0);
    return 0;
}

static int file_limit_recovery(const storage_fixture *fixture, const uint8_t *record,
    size_t length, size_t limit)
{
    uint8_t actual[140], before[140];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = 777;
    bool pending = true;
    const bool complete = limit == length;
    const zcl_status wanted = complete ? ZCL_OK : ZCL_INVALID_ENCODING;
    CHECK(fixture_read(fixture, actual, sizeof(actual), &actual_len, &pending) == wanted);
    CHECK(fixture_create(fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_promote(fixture, record, length) == wanted);
    if (complete) {
        CHECK(actual_len == length && !pending && memcmp(actual, record, length) == 0);
    } else {
        CHECK(actual_len == 777 && pending && memcmp(actual, before, sizeof(actual)) == 0);
        struct stat info = {0};
        CHECK(fstatat(fixture->directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) == -1 && errno == ENOENT);
    }
    return exact_prefix(fixture, complete ? "wallet.zcl" : ".wallet.pending", record, limit);
}

static int file_limit_case(const uint8_t *record, size_t length, size_t limit)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    const pid_t child = fork();
    if (child == 0) _exit(limited_file_child(&fixture, record, length, limit) == 0 ? 77 : 78);
    int status = 0;
    const pid_t waited = child < 0 ? -1 : waitpid(child, &status, 0);
    const int recovered = file_limit_recovery(&fixture, record, length, limit);
    const int closed = fixture_close(&fixture);
    CHECK(waited == child && child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 77);
    CHECK(recovered == 0 && closed == 0);
    return 0;
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    CHECK(fixture_record(record, sizeof(record), &length) == 0);
    for (unsigned selected = CREATE; selected <= PROMOTE; ++selected) {
        for (size_t available = 0; available <= 3; ++available)
            CHECK(pressure_case(record, length, (operation)selected, available) == 0);
    }
    const size_t limits[] = {0, 1, 79, 80, 123, length};
    for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i)
        CHECK(file_limit_case(record, length, limits[i]) == 0);
    puts("storage pressure: 12 descriptor and 6 file-size kernel exhaustion/recovery cases passed");
    return 0;
}
