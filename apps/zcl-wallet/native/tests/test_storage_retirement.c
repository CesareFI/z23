/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#include "storage_fixture.h"
#include "storage_faults.h"
#include "storage_internal.h"

#include <stdlib.h>
#include <string.h>

static unsigned byte_clears, record_clears;

void zcl_storage_test_zero(void *pointer, size_t length);
void zcl_storage_test_zero(void *pointer, size_t length)
{
    if (pointer == NULL) abort();
    if (length == ZCL_WALLET_RECORD_MAX) ++byte_clears;
    else if (length == sizeof(zcl_wallet_record)) ++record_clears;
    else abort();
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0) abort();
    }
}

static void reset_observation(void)
{
    byte_clears = record_clears = 0;
    storage_faults_reset();
}

static int admission_retirement(void)
{
    reset_observation();
    CHECK(zcl_storage_create(NULL, 0, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(byte_clears == 0 && record_clears == 1);
    reset_observation();
    CHECK(zcl_storage_promote(NULL, 0, NULL, 0) == ZCL_INVALID_ARGUMENT);
    CHECK(byte_clears == 0 && record_clears == 1);
    return 0;
}

static int read_result(const storage_fixture *fixture, size_t capacity,
                       zcl_status expected, unsigned arrays, unsigned records)
{
    uint8_t output[140], before[140];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    size_t length = 999;
    bool pending = true;
    CHECK(fixture_read(fixture, output, capacity, &length, &pending) == expected);
    CHECK(byte_clears == arrays && record_clears == records);
    if (expected == ZCL_OK) {
        CHECK(length == 124 && !pending && memcmp(output, "ZCLW", 4) == 0);
    } else {
        CHECK(length == 999 && pending && memcmp(output, before, sizeof(output)) == 0);
    }
    return 0;
}

static int read_failures(const storage_fixture *fixture)
{
    const io_mode modes[] = {IO_ZERO, IO_OVERSIZE, IO_INTERRUPT};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        reset_observation();
        storage_read_fault.mode = modes[i];
        const zcl_status expected = modes[i] == IO_INTERRUPT ? ZCL_IO_FAILURE : ZCL_INVALID_ENCODING;
        CHECK(read_result(fixture, 140, expected, 2, 0) == 0);
    }
    for (size_t at = 1; at <= 4; ++at) {
        reset_observation();
        storage_close_fault = (io_fault){IO_ERROR, at, 0};
        CHECK(read_result(fixture, 140, at == 2 ? ZCL_IO_FAILURE : ZCL_IO_UNCERTAIN,
            at == 1 ? 1 : 2, at <= 2 ? 0 : 1) == 0);
    }
    reset_observation();
    storage_sync_fault = (io_fault){IO_ERROR, 1, 0};
    CHECK(read_result(fixture, 140, ZCL_IO_UNCERTAIN, 1, 0) == 0);
    return 0;
}

static int reads_and_creation(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    reset_observation();
    CHECK(read_result(&fixture, 140, ZCL_NOT_FOUND, 1, 0) == 0);
    reset_observation();
    CHECK(fixture_create(&fixture, record, length) == ZCL_OK);
    CHECK(byte_clears == 0 && record_clears == 2);
    reset_observation();
    CHECK(fixture_create(&fixture, record, length) == ZCL_ALREADY_EXISTS);
    CHECK(byte_clears == 0 && record_clears == 1);
    reset_observation();
    CHECK(read_result(&fixture, 140, ZCL_OK, 2, 1) == 0);
    reset_observation();
    CHECK(read_result(&fixture, 123, ZCL_BUFFER_TOO_SMALL, 2, 1) == 0);
    for (size_t at = 1; at <= 2; ++at) {
        reset_observation();
        storage_read_fault = (io_fault){IO_ERROR, at, 0};
        CHECK(read_result(&fixture, 140, ZCL_IO_FAILURE, 2, 0) == 0);
    }
    reset_observation();
    storage_read_fault.mode = IO_SHORT;
    CHECK(read_result(&fixture, 140, ZCL_OK, 2, 1) == 0);
    CHECK(read_failures(&fixture) == 0);
    reset_observation();
    return fixture_close(&fixture);
}

static int promotion(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t different[140];
    memcpy(different, record, length);
    different[length - 1] ^= 1;
    CHECK(fixture_open(&fixture) == 0);
    reset_observation();
    CHECK(fixture_promote(&fixture, record, length) == ZCL_NOT_FOUND);
    CHECK(byte_clears == 1 && record_clears == 1);
    CHECK(fixture_write(&fixture, ".wallet.pending", record, length) == 0);
    reset_observation();
    CHECK(fixture_promote(&fixture, different, length) == ZCL_INVALID_ENCODING);
    CHECK(byte_clears == 2 && record_clears == 1);
    reset_observation();
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    CHECK(byte_clears == 2 && record_clears == 1);
    reset_observation();
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    CHECK(byte_clears == 2 && record_clears == 1);
    reset_observation();
    CHECK(fixture_promote(&fixture, different, length) == ZCL_ALREADY_EXISTS);
    CHECK(byte_clears == 2 && record_clears == 1);
    for (size_t at = 1; at <= 3; ++at) {
        reset_observation();
        storage_sync_fault = (io_fault){IO_ERROR, at, 0};
        CHECK(fixture_promote(&fixture, record, length) == ZCL_IO_UNCERTAIN);
        CHECK(byte_clears == (at == 1 ? 0 : 2) && record_clears == 1);
    }
    reset_observation();
    storage_read_fault = (io_fault){IO_ERROR, 2, 0};
    CHECK(fixture_promote(&fixture, record, length) == ZCL_IO_FAILURE);
    CHECK(byte_clears == 2 && record_clears == 1);
    reset_observation();
    CHECK(read_result(&fixture, 140, ZCL_OK, 2, 1) == 0);
    return fixture_close(&fixture);
}

static int malformed_record(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    uint8_t invalid[140];
    memcpy(invalid, record, length);
    invalid[4] = 0xff;
    CHECK(fixture_open(&fixture) == 0);
    reset_observation();
    CHECK(fixture_create(&fixture, invalid, length) == ZCL_UNSUPPORTED);
    CHECK(byte_clears == 0 && record_clears == 1);
    reset_observation();
    CHECK(fixture_promote(&fixture, invalid, length) == ZCL_UNSUPPORTED);
    CHECK(byte_clears == 0 && record_clears == 1);
    CHECK(fixture_write(&fixture, "wallet.zcl", invalid, length) == 0);
    reset_observation();
    CHECK(read_result(&fixture, 140, ZCL_UNSUPPORTED, 2, 1) == 0);
    return fixture_close(&fixture);
}

static int internal_read_capacity(const uint8_t *record, size_t length)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, "wallet.zcl", record, length) == 0);
    zcl_store store = {.directory = fixture.directory, .lock = -1};
    uint8_t output[140], before[140];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    size_t output_len = 999;
    reset_observation();
    CHECK(zcl_store_read_file(&store, ZCL_STORE_COMMITTED, output, 123,
        &output_len, false) == ZCL_BUFFER_TOO_SMALL);
    CHECK(byte_clears == 1 && record_clears == 0);
    CHECK(output_len == 999 && memcmp(output, before, sizeof(output)) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    if (admission_retirement() || fixture_record(record, sizeof(record), &length) ||
        reads_and_creation(record, length) || promotion(record, length) ||
        malformed_record(record, length) || internal_read_capacity(record, length)) return 1;
    puts("storage ciphertext scratch retirement and atomic refusal checks passed");
    return 0;
}
