/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
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
    size_t size = 80;
    memcpy(original, data.state[0], 80);
    if ((bytes[0] & 4U) != 0) {
        size = length - 6; /* Entry bounds6..246, payload<=240. */
        memcpy(original, bytes + 6, size);
    }
    if (*present) require(fixture_write(&fixture, ".change.index", original, size) == 0);
    return *present ? size : 0;
}

static void verify_success(const zcl_change_reservation *result, size_t size)
{
    require(size >= 80 && size % 80 == 0 && result->index == size / 80 - 1);
    require(result->network == ZCL_TESTNET);
    uint8_t entropy[16] = {0}, blind[32] = {1}, address[35] = {0};
    size_t address_len = 0;
    require(zcl_change_from_entropy(entropy, sizeof(entropy), ZCL_TESTNET, result->index,
        blind, sizeof(blind), address, sizeof(address), &address_len) == ZCL_OK);
    require(address_len == 35 && memcmp(result->address, address, 35) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    require(change_observe(&fixture, &data, &snapshot) == ZCL_OK && snapshot.file_bytes == size + 80);
    uint32_t next = 0;
    require(zcl_change_state_decode(data.wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        snapshot.tail, snapshot.tail_len, &next) == ZCL_OK && next == result->index + 1);
}

static void verify_file(bool present, size_t initial_size, const uint8_t *original, zcl_status status)
{
    struct stat info = {0};
    int found = fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW);
    if (!present) {
        require(found != 0 && errno == ENOENT && status != ZCL_OK);
        return;
    }
    require(found == 0 && change_bytes(&fixture, original, initial_size, 0) == 0);
    if (status != ZCL_OK && status != ZCL_IO_UNCERTAIN) require(info.st_size == (off_t)initial_size);
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
    zcl_change_reservation result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    zcl_change_reservation *output = (bytes[0] & 128U) != 0 ? NULL : &result;
    zcl_status status = zcl_wallet_change_reserve((const uint8_t *)fixture.path, fixture_path_len(),
        wallet, wallet_len, secret, entropy_len, output);
    if (status == ZCL_OK) verify_success(&result, size);
    else require(memcmp(&result, &before, sizeof(result)) == 0);
    verify_file(present, size, original, status);
    return 0;
}
