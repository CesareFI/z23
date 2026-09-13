/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "storage_change_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static storage_fixture fixture;
static change_storage_data data;
static uint8_t state3[80];
static bool initialized;
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);
static void require(bool condition) { if (!condition) abort(); }
static void cleanup(void) { require(fixture_close(&fixture) == 0); }

static void initialize(void)
{
    if (initialized) return;
    require(change_data_init(&data) == 0);
    uint8_t entropy[16] = {0}, blind[32] = {1};
    require(zcl_change_state_encode(data.wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        3, state3, sizeof(state3)) == ZCL_OK);
    require(fixture_open(&fixture) == 0);
    require(fixture_create(&fixture, data.wallet, data.wallet_len) == ZCL_OK);
    require(atexit(cleanup) == 0);
    initialized = true;
}

static size_t prepare_file(const uint8_t *bytes, size_t length, uint8_t *original, bool *present)
{
    int removed = unlinkat(fixture.directory, ".change.index", 0);
    require(removed == 0 || errno == ENOENT);
    unsigned mode = bytes[0] & 3U;
    *present = mode != 3;
    size_t size = mode == 0 ? 0 : 120;
    memcpy(original, data.state[0], 80);
    memcpy(original + 80, data.state[1], 80);
    if (mode == 2) {
        size = length - 6;
        memcpy(original, bytes + 6, size);
    }
    if (*present) require(fixture_write(&fixture, ".change.index", original, size) == 0);
    return *present ? size : 0;
}

static void edit_snapshot(zcl_change_storage_snapshot *snapshot, const uint8_t *bytes)
{
    if ((bytes[0] & 4U) != 0) snapshot->tail_len = bytes[1] == 255 ? SIZE_MAX : (size_t)bytes[1];
    if ((bytes[0] & 8U) != 0) {
        snapshot->file_bytes = 0;
        for (size_t i = 0; i < 4; ++i) snapshot->file_bytes |= (uint32_t)bytes[1 + i] << (8 * i);
    }
    if ((bytes[0] & 16U) != 0) snapshot->tail[(size_t)bytes[2] % 80] ^= bytes[3];
}

static void replacement_for(const zcl_change_storage_snapshot *snapshot, uint8_t *replacement)
{
    zcl_change_repair_plan plan = {0};
    uint32_t index = 1;
    if (zcl_store_change_plan_repair(snapshot, &plan) == ZCL_OK) index = plan.next_index;
    if (index <= 2) memcpy(replacement, data.state[index], 80);
    else memcpy(replacement, state3, 80);
    /* Artificial high-position snapshots cannot match the<=240-byte file;
     * exercise their CAS refusal without deriving unbounded seed fixtures. */
    for (size_t i = 0; i < 4; ++i)
        replacement[8 + i] = (uint8_t)((index >> (8 * i)) & UINT32_C(0xff));
}

static void verify_file(bool present, size_t initial, const uint8_t *original,
    zcl_status status, const uint8_t *replacement)
{
    struct stat info = {0};
    int found = fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW);
    if (!present) {
        require(found != 0 && errno == ENOENT && status != ZCL_OK);
        return;
    }
    require(found == 0 && change_bytes(&fixture, original, initial, 0) == 0);
    if (status != ZCL_OK) {
        if (status != ZCL_IO_UNCERTAIN) require(info.st_size == (off_t)initial);
        return;
    }
    uint32_t index = 0;
    for (size_t i = 0; i < 4; ++i) index |= (uint32_t)replacement[8 + i] << (8 * i);
    require(index >= 1 && index <= 3);
    size_t start = (size_t)index * 80;
    require(start >= initial && start - initial <= 80 && info.st_size == (off_t)(start + 80));
    uint8_t zeros[80] = {0};
    require(change_bytes(&fixture, zeros, start - initial, (off_t)initial) == 0);
    require(change_bytes(&fixture, replacement, 80, (off_t)start) == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length)
{
    if (length < 6 || length > 246) return 0;
    initialize();
    uint8_t original[240] = {0}, replacement[80] = {0};
    bool present = false;
    size_t initial = prepare_file(bytes, length, original, &present);
    zcl_change_storage_snapshot snapshot = {0};
    require(change_observe(&fixture, &data, &snapshot) == (present ? ZCL_OK : ZCL_NOT_FOUND));
    edit_snapshot(&snapshot, bytes);
    replacement_for(&snapshot, replacement);
    const uint8_t *state = (bytes[0] & 32U) != 0 ? NULL : replacement;
    size_t state_len = (bytes[0] & 64U) != 0 ? (size_t)bytes[5] : 80;
    const zcl_change_storage_snapshot *expected = (bytes[0] & 128U) != 0 ? NULL : &snapshot;
    zcl_status status = zcl_storage_change_repair((const uint8_t *)fixture.path, fixture_path_len(),
        data.wallet, data.wallet_len, expected, state, state_len);
    verify_file(present, initial, original, status, replacement);
    return 0;
}
