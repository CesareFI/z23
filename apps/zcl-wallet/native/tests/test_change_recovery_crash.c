/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { STOP_NONE, STOP_WRITE, STOP_SYNC, STOP_CLOSE } stop_kind;
static stop_kind selected;
static size_t selected_at, calls[4];
static bool partial;
ssize_t __real_write(int fd, const void *bytes, size_t length);
int __real_fsync(int fd);
int __real_close(int fd);
ssize_t __wrap_write(int fd, const void *bytes, size_t length);
int __wrap_fsync(int fd);
int __wrap_close(int fd);

static bool stopping(stop_kind kind)
{
    ++calls[kind];
    return selected == kind && calls[kind] == selected_at;
}

ssize_t __wrap_write(int fd, const void *bytes, size_t length)
{
    bool stop = stopping(STOP_WRITE);
    size_t wanted = stop && partial ? length / 2 : length;
    ssize_t result = __real_write(fd, bytes, wanted);
    if (stop) _exit(result >= 0 && (size_t)result == wanted ? 77 : 78);
    return result;
}

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

static zcl_status recover(const storage_fixture *fixture, const change_storage_data *data)
{
    const uint8_t entropy[16] = {0};
    return zcl_wallet_change_recover((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, sizeof(entropy));
}

static int after_crash(const storage_fixture *fixture, const change_storage_data *data, uint32_t expected)
{
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(fixture, data, &current) == ZCL_OK && current.file_bytes == expected);
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(fixture, data->state[1], 40, 80) == 0);
    uint8_t saved[240] = {0};
    CHECK(expected <= sizeof(saved));
    int fd = openat(fixture->directory, ".change.index", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    ssize_t count = read(fd, saved, expected);
    int closed = close(fd);
    CHECK(count >= 0 && (size_t)count == expected && closed == 0);
    zcl_status status = recover(fixture, data);
    CHECK(status == (expected == 200 ? ZCL_INVALID_ENCODING : expected == 240 ? ZCL_ALREADY_EXISTS : ZCL_OK));
    CHECK(change_bytes(fixture, saved, expected, 0) == 0);
    CHECK(change_observe(fixture, data, &current) == ZCL_OK);
    CHECK(current.file_bytes == (expected == 200 ? 200U : 240U));
    if (expected != 200) CHECK(memcmp(current.tail, data->state[2], 80) == 0);
    return 0;
}

static int crash_case(const change_storage_data *data, stop_kind kind, size_t at, bool short_write,
    uint32_t expected)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t original[120] = {0};
    memcpy(original, data->state[0], 80);
    memcpy(original + 80, data->state[1], 40);
    CHECK(fixture_write(&fixture, ".change.index", original, sizeof(original)) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        selected = kind;
        selected_at = at;
        partial = short_write;
        memset(calls, 0, sizeof(calls));
        (void)recover(&fixture, data);
        _exit(79); /* Every selected boundary must actually be reached. */
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
    CHECK(after_crash(&fixture, data, expected) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(crash_case(&data, STOP_WRITE, 1, false, 160) == 0);
    CHECK(crash_case(&data, STOP_WRITE, 1, true, 140) == 0);
    CHECK(crash_case(&data, STOP_WRITE, 2, false, 240) == 0);
    CHECK(crash_case(&data, STOP_WRITE, 2, true, 200) == 0);
    for (size_t at = 1; at <= 4; ++at)
        CHECK(crash_case(&data, STOP_SYNC, at, false, at >= 3 ? 240 : 120) == 0);
    for (size_t at = 1; at <= 10; ++at)
        CHECK(crash_case(&data, STOP_CLOSE, at, false, at >= 8 ? 240 : 120) == 0);
    puts("authenticated change recovery:18 reached process boundaries; preserved every byte, consumed complete successor and refused ambiguous partial replacement");
    return 0;
}
