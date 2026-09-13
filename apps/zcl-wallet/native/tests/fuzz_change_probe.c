/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static storage_fixture fixture;
static change_storage_data data;
static bool initialized;
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);
static void require(bool condition) { if (!condition) abort(); }
static void cleanup(void) { require(fixture_close(&fixture) == 0); }

static void initialize(void)
{
    if (initialized) return;
    require(change_data_init(&data) == 0);
    require(fixture_open(&fixture) == 0);
    require(fixture_create(&fixture, data.wallet, data.wallet_len) == ZCL_OK);
    require(atexit(cleanup) == 0);
    initialized = true;
}

static void verify(const zcl_change_recovery_snapshot *result, const uint8_t *bytes, size_t length)
{
    size_t tail_len = length < 80 ? length : 80;
    require(result->current.file_bytes == length && result->current.tail_len == tail_len);
    require(memcmp(result->current.tail, bytes + length - tail_len, tail_len) == 0);
    for (size_t i = tail_len; i < 80; ++i) require(result->current.tail[i] == 0);
    require(result->has_predecessor == (length > 80));
    if (length <= 80) {
        for (size_t i = 0; i < 80; ++i) require(result->predecessor[i] == 0);
        return;
    }
    /* Independent bounded chunk walk finds the last complete record before
     * the current slot; production uses constant-time offset arithmetic. */
    size_t predecessor = 0;
    for (size_t offset = 0; offset + 80 < length; offset += 80) predecessor = offset;
    require(memcmp(result->predecessor, bytes + predecessor, 80) == 0);
}

static void changed_arguments(const uint8_t *bytes, size_t length)
{
    if (length < 3) return;
    uint8_t wallet[140] = {0};
    memcpy(wallet, data.wallet, data.wallet_len);
    if ((bytes[0] & 1U) != 0) wallet[(size_t)bytes[1] % data.wallet_len] ^= bytes[2];
    size_t wallet_len = (bytes[0] & 2U) != 0 ? bytes[1] : data.wallet_len;
    size_t path_len = (bytes[0] & 8U) != 0 ? SIZE_MAX : fixture_path_len();
    zcl_change_recovery_snapshot result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    zcl_change_recovery_snapshot *output = (bytes[0] & 4U) != 0 ? NULL : &result;
    zcl_status status = zcl_storage_change_probe((const uint8_t *)fixture.path, path_len,
        wallet, wallet_len, output);
    if (status == ZCL_OK) verify(&result, bytes, length);
    else require(memcmp(&result, &before, sizeof(result)) == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length)
{
    if (length > 4096 || (bytes == NULL && length != 0)) return 0;
    static const uint8_t empty[1] = {0};
    const uint8_t *input = length == 0 ? empty : bytes;
    initialize();
    int removed = unlinkat(fixture.directory, ".change.index", 0);
    require(removed == 0 || errno == ENOENT);
    require(fixture_write(&fixture, ".change.index", input, length) == 0);
    zcl_change_recovery_snapshot result = {0};
    require(zcl_storage_change_probe((const uint8_t *)fixture.path, fixture_path_len(),
        data.wallet, data.wallet_len, &result) == ZCL_OK);
    verify(&result, input, length);
    changed_arguments(input, length);
    return 0;
}
