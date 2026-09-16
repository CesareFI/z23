/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(orphan_read(&data) == 0);
    CHECK(puts("storage orphans: read/creation refusal, unchanged outputs/files, symlinks and FIFOs passed") >= 0);
    return 0;
}
