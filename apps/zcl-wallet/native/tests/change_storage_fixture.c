/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include "change_storage_fixture.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int change_data_init(change_storage_data *data)
{
    memset(data, 0, sizeof(*data));
    CHECK(fixture_record(data->wallet, sizeof(data->wallet), &data->wallet_len) == 0);
    uint8_t entropy[16] = {0}, blind[32] = {1};
    for (uint32_t index = 0; index < 3; ++index)
        CHECK(zcl_change_state_encode(data->wallet, 80, entropy, sizeof(entropy),
            blind, sizeof(blind), index, data->state[index], 80) == ZCL_OK);
    return 0;
}

zcl_status change_create(const storage_fixture *fixture, const change_storage_data *data)
{
    return zcl_storage_create_with_change((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, data->state[0], 80);
}

zcl_status change_observe(const storage_fixture *fixture, const change_storage_data *data,
    zcl_change_storage_snapshot *snapshot)
{
    return zcl_storage_change_observe((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot);
}

zcl_status change_append(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *snapshot, size_t next)
{
    if (next >= 3) return ZCL_OUT_OF_RANGE;
    return zcl_storage_change_append((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot, data->state[next], 80);
}

int change_bytes(const storage_fixture *fixture, const uint8_t *expected, size_t length, off_t offset)
{
    uint8_t actual[240] = {0};
    CHECK(length <= sizeof(actual));
    int fd = openat(fixture->directory, ".change.index", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0);
    ssize_t got = pread(fd, actual, length, offset);
    int closed = close(fd);
    CHECK(got >= 0 && (size_t)got == length && closed == 0);
    CHECK(memcmp(actual, expected, length) == 0);
    return 0;
}
