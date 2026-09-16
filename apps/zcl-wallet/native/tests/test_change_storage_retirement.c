/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_store_write_bytes
#include "change_storage_fixture.h"
#include "storage_faults.h"
#include "storage_internal.h"
#include <stdlib.h>
#include <string.h>

static unsigned records, wallets, heads, recoveries;
static bool writing_snapshot;

void zcl_change_storage_test_zero(void *pointer, size_t length);
void zcl_change_storage_test_zero(void *pointer, size_t length)
{
    if (pointer == NULL) abort();
    if (length == sizeof(zcl_wallet_record)) ++records;
    else if (length == ZCL_WALLET_RECORD_MAX) ++wallets;
    else if (length == sizeof(zcl_change_storage_snapshot)) ++heads;
    else if (length == sizeof(zcl_change_recovery_snapshot)) ++recoveries;
    else abort();
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0) abort();
    }
}

zcl_status zcl_change_storage_test_write_bytes(int fd, const uint8_t *record, size_t length);
zcl_status zcl_change_storage_test_write_bytes(int fd, const uint8_t *record, size_t length)
{
    /* Source-only interception: consumed comparison data must be retired
     * before either append or repair makes its first filesystem write. */
    if (writing_snapshot && (records != 1 || wallets != 1 || heads != 1)) abort();
    return zcl_store_write_bytes(fd, record, length);
}

static void reset_observation(void)
{
    records = wallets = heads = recoveries = 0;
    writing_snapshot = false;
    storage_faults_reset();
}

static int saw(unsigned expected_records, unsigned expected_wallets,
               unsigned expected_heads, unsigned expected_recoveries)
{
    CHECK(records == expected_records && wallets == expected_wallets);
    CHECK(heads == expected_heads && recoveries == expected_recoveries);
    return 0;
}

static int refused_admission(void)
{
    zcl_change_storage_snapshot head = {0};
    zcl_change_recovery_snapshot recovery = {0};
    reset_observation();
    CHECK(zcl_storage_create_with_change(NULL, 0, NULL, 0, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(saw(1, 0, 0, 0) == 0);
    reset_observation();
    CHECK(zcl_storage_change_observe(NULL, 0, NULL, 0, &head) == ZCL_INVALID_ARGUMENT);
    CHECK(saw(1, 0, 0, 0) == 0);
    reset_observation();
    CHECK(zcl_storage_change_probe(NULL, 0, NULL, 0, &recovery) == ZCL_INVALID_ARGUMENT);
    CHECK(saw(1, 0, 0, 0) == 0);
    reset_observation();
    CHECK(zcl_storage_change_append(NULL, 0, NULL, 0, &head, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(saw(1, 0, 0, 0) == 0);
    reset_observation();
    CHECK(zcl_storage_change_repair(NULL, 0, NULL, 0, &head, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(saw(1, 0, 0, 0) == 0);
    return 0;
}

static int observation(const storage_fixture *fixture, const change_storage_data *data,
                       bool probe, zcl_status expected)
{
    zcl_change_recovery_snapshot result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    const zcl_status status = probe
        ? zcl_storage_change_probe((const uint8_t *)fixture->path, fixture_path_len(),
            data->wallet, data->wallet_len, &result)
        : change_observe(fixture, data, &result.current);
    CHECK(status == expected);
    CHECK(saw(1, 1, probe ? 0 : 1, probe ? 1 : 0) == 0);
    if (status != ZCL_OK) {
        CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    } else {
        CHECK(result.current.file_bytes == 160 && result.current.tail_len == 80);
        CHECK(memcmp(result.current.tail, data->state[1], 80) == 0);
        if (probe) CHECK(result.has_predecessor && memcmp(result.predecessor, data->state[0], 80) == 0);
    }
    return 0;
}

static int observation_failures(const storage_fixture *fixture, const change_storage_data *data)
{
    for (unsigned operation = 0; operation < 2; ++operation) {
        const bool probe = operation != 0;
        reset_observation();
        CHECK(observation(fixture, data, probe, ZCL_OK) == 0);
        reset_observation();
        storage_read_fault = (io_fault){IO_ERROR, 2, 0};
        CHECK(observation(fixture, data, probe, ZCL_IO_FAILURE) == 0);
        reset_observation();
        storage_pread_fault = (io_fault){IO_ERROR, probe ? 2 : 1, 0};
        CHECK(observation(fixture, data, probe, ZCL_IO_FAILURE) == 0);
        reset_observation();
        storage_close_fault = (io_fault){IO_ERROR, 3, 0};
        CHECK(observation(fixture, data, probe, ZCL_IO_UNCERTAIN) == 0);
        change_storage_data different = *data;
        different.wallet[different.wallet_len - 1] ^= 1;
        reset_observation();
        CHECK(observation(fixture, &different, probe, ZCL_ALREADY_EXISTS) == 0);
    }
    return 0;
}

static int append_and_observe(const change_storage_data *data)
{
    storage_fixture fixture;
    zcl_change_storage_snapshot head = {0};
    CHECK(fixture_open(&fixture) == 0);
    reset_observation();
    CHECK(change_create(&fixture, data) == ZCL_OK && saw(1, 0, 0, 0) == 0);
    reset_observation();
    CHECK(change_observe(&fixture, data, &head) == ZCL_OK && saw(1, 1, 1, 0) == 0);
    CHECK(head.file_bytes == 80 && head.tail_len == 80 && memcmp(head.tail, data->state[0], 80) == 0);
    reset_observation();
    storage_pread_fault = (io_fault){IO_ERROR, 1, 0};
    CHECK(change_append(&fixture, data, &head, 1) == ZCL_IO_FAILURE && saw(1, 1, 1, 0) == 0);
    reset_observation();
    writing_snapshot = true;
    storage_write_fault = (io_fault){IO_ZERO, 1, 0};
    CHECK(change_append(&fixture, data, &head, 1) == ZCL_IO_UNCERTAIN && saw(1, 1, 1, 0) == 0);
    reset_observation();
    writing_snapshot = true;
    CHECK(change_append(&fixture, data, &head, 1) == ZCL_OK && saw(1, 1, 1, 0) == 0);
    reset_observation();
    CHECK(change_append(&fixture, data, &head, 1) == ZCL_BUSY && saw(1, 1, 1, 0) == 0);
    CHECK(observation_failures(&fixture, data) == 0);
    reset_observation();
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture, data->state[1], 80, 80) == 0);
    return fixture_close(&fixture);
}

static zcl_status repair(const storage_fixture *fixture, const change_storage_data *data,
                         const zcl_change_storage_snapshot *head)
{
    return zcl_storage_change_repair((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, head, data->state[2], 80);
}

static int repair_retirement(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t partial[120];
    memcpy(partial, data->state[0], 80);
    memcpy(partial + 80, data->state[1], 40);
    CHECK(fixture_write(&fixture, ".change.index", partial, sizeof(partial)) == 0);
    zcl_change_storage_snapshot head = {0};
    reset_observation();
    CHECK(change_observe(&fixture, data, &head) == ZCL_OK && saw(1, 1, 1, 0) == 0);
    reset_observation();
    storage_pread_fault = (io_fault){IO_ERROR, 1, 0};
    CHECK(repair(&fixture, data, &head) == ZCL_IO_FAILURE && saw(1, 1, 1, 0) == 0);
    reset_observation();
    writing_snapshot = true;
    storage_write_fault = (io_fault){IO_ZERO, 1, 0};
    CHECK(repair(&fixture, data, &head) == ZCL_IO_UNCERTAIN && saw(1, 1, 1, 0) == 0);
    reset_observation();
    writing_snapshot = true;
    CHECK(repair(&fixture, data, &head) == ZCL_OK && saw(1, 1, 1, 0) == 0);
    reset_observation();
    CHECK(repair(&fixture, data, &head) == ZCL_BUSY && saw(1, 1, 1, 0) == 0);
    CHECK(change_bytes(&fixture, partial, sizeof(partial), 0) == 0);
    CHECK(change_bytes(&fixture, data->state[2], 80, 160) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    if (refused_admission() || change_data_init(&data) || append_and_observe(&data) ||
        repair_retirement(&data)) return 1;
    puts("change storage record/snapshot retirement and exact output checks passed");
    return 0;
}
