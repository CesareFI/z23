/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_change_internal.h"
#include "zcl_change_reservation.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static zcl_status repair(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *snapshot, const uint8_t *state)
{
    return zcl_storage_change_repair((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, snapshot, state, 80);
}

static int partial_case(const change_storage_data *data, size_t length)
{
    storage_fixture fixture;
    CHECK(length < 160 && fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t original[160] = {0}, zero[80] = {0};
    memcpy(original, data->state[0], 80);
    memcpy(original + 80, data->state[1], 80);
    if (length == 80) original[79] ^= 1; /* Complete v1 head with bad MAC. */
    CHECK(fixture_write(&fixture, ".change.index", original, length) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    uint32_t expected_index = length <= 80 ? 1 : 2;
    size_t record_start = (size_t)expected_index * 80;
    CHECK(repair(&fixture, data, &snapshot, data->state[0]) == ZCL_INVALID_ENCODING);
    CHECK(repair(&fixture, data, &snapshot, data->state[expected_index]) == ZCL_OK);
    CHECK(change_bytes(&fixture, original, length, 0) == 0);
    CHECK(change_bytes(&fixture, zero, record_start - length, (off_t)length) == 0);
    CHECK(change_bytes(&fixture, data->state[expected_index], 80, (off_t)record_start) == 0);
    CHECK(repair(&fixture, data, &snapshot, data->state[expected_index]) == ZCL_BUSY);
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(&fixture, data, &current) == ZCL_OK && current.file_bytes == record_start + 80);
    if (length == 0 || length == 80 || length == 159) {
        uint8_t entropy[16] = {0};
        zcl_change_reservation result = {0};
        CHECK(zcl_wallet_change_reserve((const uint8_t *)fixture.path, fixture_path_len(),
            data->wallet, data->wallet_len, entropy, sizeof(entropy), &result) == ZCL_OK);
        CHECK(result.index == expected_index && result.network == ZCL_TESTNET);
    }
    return fixture_close(&fixture);
}

static int missing_and_binding(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    zcl_change_storage_snapshot empty = {0};
    CHECK(repair(&fixture, data, &empty, data->state[1]) == ZCL_NOT_FOUND);
    CHECK(fixture_write(&fixture, ".change.index", data->state[0], 1) == 0);
    CHECK(repair(&fixture, data, &empty, data->state[1]) == ZCL_BUSY);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    change_storage_data other = *data;
    other.wallet[other.wallet_len - 1] ^= 1;
    CHECK(repair(&fixture, &other, &snapshot, data->state[1]) == ZCL_ALREADY_EXISTS);
    snapshot.tail[0] ^= 1;
    CHECK(repair(&fixture, data, &snapshot, data->state[1]) == ZCL_BUSY);
    CHECK(change_bytes(&fixture, data->state[0], 1, 0) == 0);
    snapshot.tail[0] ^= 1;
    CHECK(repair(&fixture, data, &snapshot, data->state[1]) == ZCL_OK);
    return fixture_close(&fixture);
}

static int plan_boundaries(void)
{
    static const uint32_t lengths[] = {0, 1, 79, 80, 81, 159, 160, 161,
        ZCL_CHANGE_STORAGE_MAX_BYTES - 161, ZCL_CHANGE_STORAGE_MAX_BYTES - 160,
        ZCL_CHANGE_STORAGE_MAX_BYTES - 81, ZCL_CHANGE_STORAGE_MAX_BYTES - 80,
        ZCL_CHANGE_STORAGE_MAX_BYTES - 79, ZCL_CHANGE_STORAGE_MAX_BYTES - 1,
        ZCL_CHANGE_STORAGE_MAX_BYTES, ZCL_CHANGE_STORAGE_MAX_BYTES + 1, UINT32_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        zcl_change_storage_snapshot snapshot = {.file_bytes = lengths[i], .tail_len = lengths[i] < 80 ? lengths[i] : 80};
        zcl_change_repair_plan plan, before;
        memset(&plan, 0xa5, sizeof(plan));
        memcpy(&before, &plan, sizeof(before));
        zcl_status status = zcl_store_change_plan_repair(&snapshot, &plan);
        if (lengths[i] > ZCL_CHANGE_STORAGE_MAX_BYTES - 80) {
            CHECK(status == ZCL_OUT_OF_RANGE && memcmp(&plan, &before, sizeof(plan)) == 0);
            continue;
        }
        CHECK(status == ZCL_OK && plan.padding <= 80 && plan.next_index >= 1);
        uint64_t start = (uint64_t)lengths[i] + plan.padding;
        CHECK(start % 80 == 0 && start == (uint64_t)plan.next_index * 80);
        CHECK(start + 80 <= ZCL_CHANGE_STORAGE_MAX_BYTES);
        if (lengths[i] != 0) CHECK(plan.padding < 80);
        snapshot.tail_len = SIZE_MAX;
        CHECK(zcl_store_change_plan_repair(&snapshot, &plan) == ZCL_INVALID_ENCODING);
    }
    zcl_change_storage_snapshot snapshot = {0};
    zcl_change_repair_plan plan = {0};
    CHECK(zcl_store_change_plan_repair(NULL, &plan) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_store_change_plan_repair(&snapshot, NULL) == ZCL_INVALID_ARGUMENT);
    return 0;
}

static int final_capacity(const change_storage_data *data, uint32_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)length) == 0 && close(fd) == 0);
    uint8_t entropy[16] = {0}, blind[32] = {1}, last[80] = {0};
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        65535, last, sizeof(last)) == ZCL_OK);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    zcl_status status = repair(&fixture, data, &snapshot, last);
    bool fits = length <= ZCL_CHANGE_STORAGE_MAX_BYTES - 80;
    CHECK(status == (fits ? ZCL_OK : ZCL_OUT_OF_RANGE));
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(info.st_size == (off_t)(fits ? ZCL_CHANGE_STORAGE_MAX_BYTES : length));
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    if (fits) CHECK(change_bytes(&fixture, last, 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    for (size_t length = 0; length < 160; ++length) CHECK(partial_case(&data, length) == 0);
    CHECK(missing_and_binding(&data) == 0);
    CHECK(plan_boundaries() == 0);
    CHECK(final_capacity(&data, ZCL_CHANGE_STORAGE_MAX_BYTES - 81) == 0);
    CHECK(final_capacity(&data, ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 0);
    CHECK(final_capacity(&data, ZCL_CHANGE_STORAGE_MAX_BYTES - 79) == 0);
    puts("change repair IO: preserved160 empty/partial/corrupt prefixes, checked padding/CAS, no missing initialization and final capacity passed");
    return 0;
}
