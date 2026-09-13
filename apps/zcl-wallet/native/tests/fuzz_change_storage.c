/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static storage_fixture fixture;
static change_storage_data data;
static bool initialized;
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

static void require(bool condition) { if (!condition) abort(); }

static void cleanup(void)
{
    require(fixture_close(&fixture) == 0);
}

static void initialize(void)
{
    if (initialized) return;
    require(change_data_init(&data) == 0);
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
    size_t size = mode == 1 ? 160 : 80;
    memcpy(original, data.state[0], 80);
    memcpy(original + 80, data.state[1], 80);
    if (mode == 2) {
        size = length - 6; /* Entry requires6..246; payload<=240. */
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

static void verify_file(bool present, size_t initial_size, const uint8_t *original,
    zcl_status status, const uint8_t *next)
{
    struct stat info = {0};
    int result = fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW);
    if (!present) {
        require(result != 0 && errno == ENOENT && status != ZCL_OK);
        return;
    }
    require(result == 0);
    require(change_bytes(&fixture, original, initial_size, 0) == 0);
    if (status == ZCL_OK) {
        require(info.st_size == (off_t)(initial_size + 80));
        require(change_bytes(&fixture, next, 80, (off_t)initial_size) == 0);
    } else if (status != ZCL_IO_UNCERTAIN) require(info.st_size == (off_t)initial_size);
}

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length)
{
    if (length < 6 || length > 246) return 0;
    initialize();
    uint8_t original[240] = {0}, next[80] = {0};
    bool present = false;
    size_t initial_size = prepare_file(bytes, length, original, &present);
    zcl_change_storage_snapshot snapshot = {0};
    zcl_status observed = change_observe(&fixture, &data, &snapshot);
    require(observed == (present ? ZCL_OK : ZCL_NOT_FOUND));
    if (present) {
        size_t wanted = initial_size < 80 ? initial_size : 80;
        require(snapshot.file_bytes == initial_size && snapshot.tail_len == wanted);
        require(memcmp(snapshot.tail, original + initial_size - wanted, wanted) == 0);
    }
    edit_snapshot(&snapshot, bytes);
    memcpy(next, data.state[initial_size == 160 ? 2 : 1], 80);
    if ((bytes[0] & 32U) != 0) next[(size_t)bytes[4] % 80] ^= bytes[5];
    size_t state_len = (bytes[0] & 64U) != 0 ? (size_t)bytes[1] : 80;
    const uint8_t *state = (bytes[0] & 128U) != 0 ? NULL : next;
    zcl_status status = zcl_storage_change_append((const uint8_t *)fixture.path, fixture_path_len(),
        data.wallet, data.wallet_len, &snapshot, state, state_len);
    verify_file(present, initial_size, original, status, next);
    return 0;
}
