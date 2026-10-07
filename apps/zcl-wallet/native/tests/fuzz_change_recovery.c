/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static size_t prepare_file(const uint8_t *bytes, size_t length, uint8_t *original, bool *present)
{
    int removed = unlinkat(fixture.directory, ".change.index", 0);
    require(removed == 0 || errno == ENOENT);
    *present = (bytes[0] & 64U) == 0;
    size_t size = 120;
    memcpy(original, data.state[0], 80);
    memcpy(original + 80, data.state[1], 40);
    if ((bytes[0] & 4U) != 0) {
        size = length - 6; /* Entry bounds6..246, payload<=240. */
        memcpy(original, bytes + 6, size);
    }
    if (*present) require(fixture_write(&fixture, ".change.index", original, size) == 0);
    return *present ? size : 0;
}

static void verify_success(size_t size, const uint8_t *original)
{
    require(size >= 96 && size <= 240);
    size_t next = size / 80 + (size % 80 != 0 ? 1U : 0U);
    require(next >= 2 && next <= 3);
    size_t fragment = size - (next - 1) * 80;
    require(fragment >= 16 && fragment <= 80);
    /* Independent exact known-record oracle for this <=3-slot public wallet
     * fixture. Earlier/later authentic counters cannot satisfy these bytes. */
    require(memcmp(original + (next - 2) * 80, data.state[next - 2], 80) == 0);
    require(memcmp(original + (next - 1) * 80, data.state[next - 1], 16) == 0);
    if (fragment == 80) require(memcmp(original + (next - 1) * 80, data.state[next - 1], 80) != 0);
    const uint8_t zeros[80] = {0};
    require(change_bytes(&fixture, zeros, next * 80 - size, (off_t)size) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    require(change_observe(&fixture, &data, &snapshot) == ZCL_OK && snapshot.file_bytes == (next + 1) * 80);
    uint8_t entropy[16] = {0}, blind[32] = {1};
    uint32_t index = 0;
    require(zcl_change_state_decode(data.wallet, 80, entropy, 16, blind, 32,
        snapshot.tail, snapshot.tail_len, &index) == ZCL_OK && index == next);
}

static void verify_file(bool present, size_t size, const uint8_t *original, zcl_status status)
{
    struct stat info = {0};
    int found = fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW);
    if (!present) {
        require(found != 0 && errno == ENOENT && status != ZCL_OK);
        return;
    }
    require(found == 0 && change_bytes(&fixture, original, size, 0) == 0);
    if (status == ZCL_OK) verify_success(size, original);
    else if (status != ZCL_IO_UNCERTAIN) require(info.st_size == (off_t)size);
}

static void verify_control(uint8_t selector, zcl_status status)
{
    /* Selector 0 disables every mutation: the correct wallet/entropy/path and
     * an authenticated predecessor plus a 40-byte supported successor must recover.
     * A fuzzer that accepts refusal here can silently lose all success coverage. */
    if (selector == 0 && status != ZCL_OK) {
        fputs("Known recoverable journal was refused\n", stderr);
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length)
{
    if (length < 6 || length > 246) return 0;
    initialize();
    uint8_t original[240] = {0}, wallet[140] = {0}, entropy[32] = {0};
    bool present = false;
    size_t size = prepare_file(bytes, length, original, &present);
    memcpy(wallet, data.wallet, data.wallet_len);
    if ((bytes[0] & 1U) != 0) wallet[(size_t)bytes[1] % data.wallet_len] ^= bytes[2];
    if ((bytes[0] & 2U) != 0) entropy[(size_t)bytes[3] % sizeof(entropy)] ^= bytes[4];
    size_t wallet_len = (bytes[0] & 8U) != 0 ? (size_t)bytes[5] : data.wallet_len;
    size_t entropy_len = (bytes[0] & 16U) != 0 ? (size_t)bytes[5] : 16;
    const uint8_t *secret = (bytes[0] & 32U) != 0 ? NULL : entropy;
    size_t path_len = (bytes[0] & 128U) != 0 ? SIZE_MAX : fixture_path_len();
    zcl_status status = zcl_wallet_change_recover((const uint8_t *)fixture.path, path_len,
        wallet, wallet_len, secret, entropy_len);
    verify_control(bytes[0], status);
    verify_file(present, size, original, status);
    return 0;
}
