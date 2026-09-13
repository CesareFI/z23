/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static zcl_status probe(const storage_fixture *fixture, const change_storage_data *data,
    zcl_change_recovery_snapshot *snapshot)
{
    return zcl_storage_change_probe((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot);
}

static int bounded_positions(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t original[320] = {0};
    for (size_t i = 0; i < sizeof(original); ++i) original[i] = (uint8_t)(i & 0xffU);
    for (size_t length = 0; length <= sizeof(original); ++length) {
        CHECK(fixture_write(&fixture, ".change.index", original, length) == 0);
        zcl_change_recovery_snapshot result;
        memset(&result, 0xa5, sizeof(result));
        CHECK(probe(&fixture, data, &result) == ZCL_OK);
        size_t tail_len = length < 80 ? length : 80;
        CHECK(result.current.file_bytes == length && result.current.tail_len == tail_len);
        CHECK(memcmp(result.current.tail, original + length - tail_len, tail_len) == 0);
        for (size_t i = tail_len; i < 80; ++i) CHECK(result.current.tail[i] == 0);
        CHECK(result.has_predecessor == (length > 80));
        if (length <= 80) {
            for (size_t i = 0; i < 80; ++i) CHECK(result.predecessor[i] == 0);
        } else {
            /* Explicit interval oracle instead of sharing the offset helper. */
            size_t start = length <= 160 ? 0 : (length <= 240 ? 80 : 160);
            CHECK(memcmp(result.predecessor, original + start, 80) == 0);
        }
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    return fixture_close(&fixture);
}

static int authentic_predecessor(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot old = {0};
    CHECK(change_observe(&fixture, data, &old) == ZCL_OK && change_append(&fixture, data, &old, 1) == ZCL_OK);
    zcl_change_recovery_snapshot result = {0};
    CHECK(probe(&fixture, data, &result) == ZCL_OK && result.has_predecessor);
    uint8_t entropy[16] = {0}, blind[32] = {1};
    uint32_t index = UINT32_MAX;
    CHECK(zcl_change_state_decode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        result.predecessor, 80, &index) == ZCL_OK && index == 0);
    CHECK(memcmp(result.current.tail, data->state[1], 80) == 0);
    int fd = openat(fixture.directory, ".change.index", O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && write(fd, data->state[2], 16) == 16 && close(fd) == 0);
    CHECK(probe(&fixture, data, &result) == ZCL_OK && result.current.file_bytes == 176);
    CHECK(zcl_change_state_decode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        result.predecessor, 80, &index) == ZCL_OK && index == 1);
    CHECK(memcmp(result.current.tail + 64, data->state[2], 16) == 0);
    return fixture_close(&fixture);
}

static int cap_offsets(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES) == 0);
    CHECK(pwrite(fd, data->state[1], 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 160) == 80);
    CHECK(pwrite(fd, data->state[2], 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 80);
    CHECK(close(fd) == 0);
    zcl_change_recovery_snapshot result = {0};
    CHECK(probe(&fixture, data, &result) == ZCL_OK);
    CHECK(result.current.file_bytes == ZCL_CHANGE_STORAGE_MAX_BYTES && result.has_predecessor);
    /* Public sparse fixture checks bounded positioning, not MAC-position validity. */
    CHECK(memcmp(result.predecessor, data->state[1], 80) == 0);
    CHECK(memcmp(result.current.tail, data->state[2], 80) == 0);
    return fixture_close(&fixture);
}

static int refusal(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    zcl_change_recovery_snapshot result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    CHECK(probe(&fixture, data, &result) == ZCL_NOT_FOUND);
    CHECK(change_create(&fixture, data) == ZCL_OK);
    change_storage_data other = *data;
    other.wallet[other.wallet_len - 1] ^= 1;
    CHECK(probe(&fixture, &other, &result) == ZCL_ALREADY_EXISTS);
    CHECK(probe(&fixture, data, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_change_probe(NULL, 1, data->wallet, data->wallet_len, &result) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_storage_change_probe((const uint8_t *)fixture.path, SIZE_MAX,
        data->wallet, data->wallet_len, &result) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_storage_change_probe((const uint8_t *)fixture.path, fixture_path_len(),
        data->wallet, SIZE_MAX, &result) == ZCL_OUT_OF_RANGE);
    CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    CHECK(probe(&fixture, data, &result) == ZCL_NOT_FOUND);
    CHECK(symlinkat("wallet.zcl", fixture.directory, ".change.index") == 0);
    CHECK(probe(&fixture, data, &result) == ZCL_IO_FAILURE);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(bounded_positions(&data) == 0);
    CHECK(authentic_predecessor(&data) == 0);
    CHECK(cap_offsets(&data) == 0);
    CHECK(refusal(&data) == 0);
    puts("change probe:321 exact tail/predecessor positions, partial/authenticated evidence, cap and unchanged refusal passed");
    return 0;
}
