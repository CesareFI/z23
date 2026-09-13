/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_change_internal.h"
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

static zcl_status repair(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *snapshot, const uint8_t *replacement)
{
    return zcl_storage_change_repair((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot, replacement, 80);
}

static int complete_after_crash(const storage_fixture *fixture, const change_storage_data *data)
{
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(fixture, data, &current) == ZCL_OK);
    CHECK(current.file_bytes == 120 || current.file_bytes == 140 || current.file_bytes == 160 ||
        current.file_bytes == 200 || current.file_bytes == 240);
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(fixture, data->state[1], 40, 80) == 0);
    if (current.file_bytes == 240) {
        CHECK(memcmp(current.tail, data->state[2], 80) == 0);
        return 0;
    }
    uint8_t saved[240] = {0}, entropy[16] = {0}, blind[32] = {1}, replacement[80] = {0};
    int fd = openat(fixture->directory, ".change.index", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    ssize_t count = read(fd, saved, current.file_bytes);
    int closed = close(fd);
    CHECK(count >= 0 && (size_t)count == current.file_bytes && closed == 0);
    zcl_change_repair_plan plan = {0};
    CHECK(zcl_store_change_plan_repair(&current, &plan) == ZCL_OK && plan.next_index >= 2);
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        plan.next_index, replacement, sizeof(replacement)) == ZCL_OK);
    CHECK(repair(fixture, data, &current, replacement) == ZCL_OK);
    CHECK(change_bytes(fixture, saved, current.file_bytes, 0) == 0);
    CHECK(change_bytes(fixture, replacement, 80, (off_t)plan.next_index * 80) == 0);
    return 0;
}

static int crash_case(const change_storage_data *data, stop_kind kind, size_t at, bool short_write)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t original[120] = {0};
    memcpy(original, data->state[0], 80);
    memcpy(original + 80, data->state[1], 40);
    CHECK(fixture_write(&fixture, ".change.index", original, sizeof(original)) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        selected = kind;
        selected_at = at;
        partial = short_write;
        memset(calls, 0, sizeof(calls));
        (void)repair(&fixture, data, &snapshot, data->state[2]);
        _exit(79); /* Selected boundary must actually be reached. */
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
    CHECK(complete_after_crash(&fixture, data) == 0);
    CHECK(change_bytes(&fixture, original, sizeof(original), 0) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    for (size_t at = 1; at <= 2; ++at) {
        CHECK(crash_case(&data, STOP_WRITE, at, false) == 0);
        CHECK(crash_case(&data, STOP_WRITE, at, true) == 0);
    }
    for (size_t at = 1; at <= 3; ++at) CHECK(crash_case(&data, STOP_SYNC, at, false) == 0);
    for (size_t at = 1; at <= 5; ++at) CHECK(crash_case(&data, STOP_CLOSE, at, false) == 0);
    puts("change repair processes:12 reached padding/replacement/sync/close boundaries; repeated recovery preserves every byte");
    return 0;
}
