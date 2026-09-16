/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include "change_storage_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static int pair_and_append(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(change_create(&fixture, data) == ZCL_OK);
    CHECK(change_create(&fixture, data) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_ALREADY_EXISTS);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(info.st_size == 80 && info.st_nlink == 1 && (info.st_mode & 0777) == 0600);
    uint8_t wallet[140] = {0};
    size_t wallet_len = 0;
    bool pending = true;
    CHECK(fixture_read(&fixture, wallet, sizeof(wallet), &wallet_len, &pending) == ZCL_OK);
    CHECK(!pending && wallet_len == data->wallet_len && memcmp(wallet, data->wallet, wallet_len) == 0);
    zcl_change_storage_snapshot snapshot = {0}, old = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    CHECK(snapshot.file_bytes == 80 && snapshot.tail_len == 80 && memcmp(snapshot.tail, data->state[0], 80) == 0);
    old = snapshot;
    CHECK(change_append(&fixture, data, &snapshot, 2) == ZCL_INVALID_ENCODING);
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_OK);
    CHECK(change_append(&fixture, data, &old, 1) == ZCL_BUSY);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == 160);
    CHECK(memcmp(snapshot.tail, data->state[1], 80) == 0);
    CHECK(change_append(&fixture, data, &snapshot, 2) == ZCL_OK);
    for (size_t index = 0; index < 3; ++index)
        CHECK(change_bytes(&fixture, data->state[index], 80, (off_t)(index * 80)) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == 240);
    return fixture_close(&fixture);
}

static int missing_and_orphan(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    zcl_change_storage_snapshot snapshot, before;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    memcpy(&before, &snapshot, sizeof(before));
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_NOT_FOUND);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    CHECK(fixture_write(&fixture, ".change.index", data->state[0], 80) == 0);
    CHECK(change_create(&fixture, data) == ZCL_ALREADY_EXISTS);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_ALREADY_EXISTS);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_NOT_FOUND);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    CHECK(change_create(&fixture, data) == ZCL_ALREADY_EXISTS);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_NOT_FOUND);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    zcl_change_storage_snapshot valid = {.file_bytes = 80, .tail_len = 80};
    memcpy(valid.tail, data->state[0], 80);
    CHECK(change_append(&fixture, data, &valid, 1) == ZCL_NOT_FOUND);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    return fixture_close(&fixture);
}

static int read_refusal(const storage_fixture *fixture, zcl_status expected)
{
    uint8_t wallet[140], before[140];
    memset(wallet, 0xa5, sizeof(wallet));
    memcpy(before, wallet, sizeof(before));
    size_t length = SIZE_MAX;
    bool pending = true;
    CHECK(fixture_read(fixture, wallet, sizeof(wallet), &length, &pending) == expected);
    CHECK(length == SIZE_MAX && pending && memcmp(wallet, before, sizeof(wallet)) == 0);
    return 0;
}

static int orphan_read(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(read_refusal(&fixture, ZCL_NOT_FOUND) == 0);
    const size_t lengths[] = {0, 40, 80};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        CHECK(fixture_write(&fixture, ".change.index", data->state[0], lengths[i]) == 0);
        CHECK(read_refusal(&fixture, ZCL_ALREADY_EXISTS) == 0);
        CHECK(change_create(&fixture, data) == ZCL_ALREADY_EXISTS);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_ALREADY_EXISTS);
        CHECK(change_bytes(&fixture, data->state[0], lengths[i], 0) == 0);
        struct stat state = {0};
        CHECK(fstatat(fixture.directory, ".change.index", &state, AT_SYMLINK_NOFOLLOW) == 0);
        CHECK(state.st_size == (off_t)lengths[i]);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    /* Presence, including a dangling symlink or FIFO, cannot mean fresh setup.
     * The read must not follow a target or wait for a writer. */
    CHECK(symlinkat("target", fixture.directory, ".change.index") == 0);
    CHECK(read_refusal(&fixture, ZCL_ALREADY_EXISTS) == 0);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(S_ISLNK(info.st_mode));
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(mkfifoat(fixture.directory, ".change.index", 0600) == 0);
    CHECK(read_refusal(&fixture, ZCL_ALREADY_EXISTS) == 0);
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(S_ISFIFO(info.st_mode));
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(read_refusal(&fixture, ZCL_NOT_FOUND) == 0);
    return fixture_close(&fixture);
}

static int wallet_and_snapshot_binding(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    change_storage_data different = *data;
    different.wallet[different.wallet_len - 1] ^= 1;
    CHECK(change_observe(&fixture, &different, &snapshot) == ZCL_ALREADY_EXISTS);
    CHECK(change_append(&fixture, &different, &snapshot, 1) == ZCL_ALREADY_EXISTS);
    for (size_t i = 16; i < 80; ++i) {
        snapshot.tail[i] ^= 1;
        CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_BUSY);
        snapshot.tail[i] ^= 1;
    }
    int fd = openat(fixture.directory, ".lock", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_BUSY);
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_BUSY);
    CHECK(close(fd) == 0);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_OK);
    return fixture_close(&fixture);
}

static int partial_and_wrong_position(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t bytes[160] = {0};
    memcpy(bytes, data->state[0], 80);
    memcpy(bytes + 80, data->state[1], 80);
    for (size_t length = 0; length < sizeof(bytes); ++length) {
        if (length == 80) continue;
        CHECK(fixture_write(&fixture, ".change.index", bytes, length) == 0);
        zcl_change_storage_snapshot snapshot = {0};
        CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
        size_t tail_len = length < 80 ? length : 80;
        CHECK(snapshot.file_bytes == length && snapshot.tail_len == tail_len);
        CHECK(memcmp(snapshot.tail, bytes + length - tail_len, tail_len) == 0);
        for (size_t i = tail_len; i < 80; ++i) CHECK(snapshot.tail[i] == 0);
        CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_INVALID_ENCODING);
        CHECK(change_bytes(&fixture, bytes, length, 0) == 0);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    CHECK(fixture_write(&fixture, ".change.index", data->state[1], 80) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_INVALID_ENCODING);
    CHECK(change_bytes(&fixture, data->state[1], 80, 0) == 0);
    return fixture_close(&fixture);
}

static int file_policy(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    zcl_change_storage_snapshot snapshot, before;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    memcpy(&before, &snapshot, sizeof(before));
    CHECK(fixture_write(&fixture, "target", data->state[0], 80) == 0);
    CHECK(symlinkat("target", fixture.directory, ".change.index") == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(linkat(fixture.directory, "target", fixture.directory, ".change.index", 0) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(mkfifoat(fixture.directory, ".change.index", 0600) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_IO_FAILURE);
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(fixture_write(&fixture, ".change.index", data->state[0], 80) == 0);
    CHECK(fchmodat(fixture.directory, ".change.index", 0644, 0) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_IO_FAILURE);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    return fixture_close(&fixture);
}

static int capacity_and_bounds(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    const uint8_t *path = (const uint8_t *)fixture.path;
    CHECK(zcl_storage_create_with_change(path, fixture_path_len(), data->wallet, data->wallet_len, NULL, 80) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_create_with_change(path, fixture_path_len(), data->wallet, data->wallet_len, data->state[1], 80) == ZCL_INVALID_ENCODING);
    CHECK(zcl_storage_change_observe(path, fixture_path_len(), data->wallet, data->wallet_len, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(change_append(&fixture, data, NULL, 1) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_change_append(path, fixture_path_len(), data->wallet, SIZE_MAX, &snapshot, data->state[1], 80) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_storage_change_append(path, fixture_path_len(), data->wallet, data->wallet_len, &snapshot, NULL, 80) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_change_append(path, fixture_path_len(), data->wallet, data->wallet_len, &snapshot, data->state[1], SIZE_MAX) == ZCL_OUT_OF_RANGE);
    snapshot.tail_len = SIZE_MAX;
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_INVALID_ENCODING);
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == ZCL_CHANGE_STORAGE_MAX_BYTES);
    CHECK(change_append(&fixture, data, &snapshot, 1) == ZCL_OUT_OF_RANGE);
    CHECK(ftruncate(fd, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES + 1) == 0);
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OUT_OF_RANGE);
    CHECK(close(fd) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(pair_and_append(&data) == 0);
    CHECK(missing_and_orphan(&data) == 0);
    CHECK(orphan_read(&data) == 0);
    CHECK(wallet_and_snapshot_binding(&data) == 0);
    CHECK(partial_and_wrong_position(&data) == 0);
    CHECK(file_policy(&data) == 0);
    CHECK(capacity_and_bounds(&data) == 0);
    puts("change storage: paired creation, bounded tail, exact wallet/CAS, no reset, prefix preservation and file policy passed");
    return 0;
}
