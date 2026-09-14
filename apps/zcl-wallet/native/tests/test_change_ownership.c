/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static zcl_status reconstruct(const storage_fixture *fixture, const change_storage_data *data,
    const uint8_t *entropy, size_t entropy_len, uint32_t index, uint8_t *address, size_t capacity)
{
    return zcl_wallet_change_reserved_address((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, entropy_len, index, address, capacity);
}

static int unchanged(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *before)
{
    zcl_change_storage_snapshot after = {0};
    CHECK(change_observe(fixture, data, &after) == ZCL_OK);
    CHECK(memcmp(before, &after, sizeof(after)) == 0);
    CHECK(change_bytes(fixture, data->state[0], 80, 0) == 0);
    return 0;
}

static int matrix_case(zcl_network network, size_t entropy_len)
{
    change_storage_data data = {0};
    uint8_t entropy[32] = {0}, blind[32] = {1}, header[80] = {0};
    uint8_t ciphertext[48] = {0}, iv[12] = {0};
    CHECK(zcl_wallet_header_create(entropy, entropy_len, network, blind, sizeof(blind), header, 80) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, 80, iv, 12, ciphertext, entropy_len + 16,
        data.wallet, sizeof(data.wallet), &data.wallet_len) == ZCL_OK);
    CHECK(zcl_change_state_encode(header, 80, entropy, entropy_len, blind, sizeof(blind),
        0, data.state[0], 80) == ZCL_OK);
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(zcl_wallet_change_create((const uint8_t *)fixture.path, fixture_path_len(), data.wallet,
        data.wallet_len, entropy, entropy_len) == ZCL_OK);
    uint8_t address[35];
    memset(address, 0xa5, sizeof(address));
    CHECK(reconstruct(&fixture, &data, entropy, entropy_len, 0, address, 35) == ZCL_NOT_FOUND);
    for (size_t i = 0; i < sizeof(address); ++i) CHECK(address[i] == 0xa5);
    for (uint32_t index = 0; index < 2; ++index) {
        zcl_change_reservation reservation = {0};
        CHECK(zcl_wallet_change_reserve((const uint8_t *)fixture.path, fixture_path_len(),
            data.wallet, data.wallet_len, entropy, entropy_len, &reservation) == ZCL_OK);
        CHECK(reservation.index == index && reservation.network == network);
        zcl_change_storage_snapshot before = {0};
        CHECK(change_observe(&fixture, &data, &before) == ZCL_OK);
        CHECK(reconstruct(&fixture, &data, entropy, entropy_len, index, address, 35) == ZCL_OK);
        CHECK(memcmp(address, reservation.address, sizeof(address)) == 0);
        CHECK(reconstruct(&fixture, &data, entropy, entropy_len, index + 1, address, 35) == ZCL_NOT_FOUND);
        CHECK(memcmp(address, reservation.address, sizeof(address)) == 0);
        CHECK(unchanged(&fixture, &data, &before) == 0);
    }
    return fixture_close(&fixture);
}

static int consumed_fixture(storage_fixture *fixture, const change_storage_data *data)
{
    CHECK(fixture_open(fixture) == 0 && change_create(fixture, data) == ZCL_OK);
    zcl_change_storage_snapshot before = {0};
    CHECK(change_observe(fixture, data, &before) == ZCL_OK);
    CHECK(change_append(fixture, data, &before, 1) == ZCL_OK);
    return 0;
}

static int capacities(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(consumed_fixture(&fixture, data) == 0);
    zcl_change_storage_snapshot before = {0};
    CHECK(change_observe(&fixture, data, &before) == ZCL_OK);
    uint8_t entropy[16] = {0}, blind[32] = {1}, expected[35] = {0};
    size_t length = 0;
    CHECK(zcl_change_from_entropy(entropy, 16, ZCL_TESTNET, 0, blind, 32, expected, 35, &length) == ZCL_OK);
    CHECK(length == 35);
    for (size_t capacity = 0; capacity <= 64; ++capacity) {
        struct { uint8_t before[8], address[64], after[8]; } box;
        memset(&box, 0xa5, sizeof(box));
        const zcl_status status = reconstruct(&fixture, data, entropy, 16, 0, box.address, capacity);
        CHECK(status == (capacity < 35 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK));
        if (status == ZCL_OK) CHECK(memcmp(box.address, expected, 35) == 0);
        for (size_t i = status == ZCL_OK ? 35 : 0; i < sizeof(box.address); ++i) CHECK(box.address[i] == 0xa5);
        for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
        CHECK(unchanged(&fixture, data, &before) == 0);
    }
    return fixture_close(&fixture);
}

static int malformed_state(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    uint8_t entropy[16] = {0}, bytes[160], address[35], original[35];
    memset(address, 0xa5, sizeof(address));
    memcpy(original, address, sizeof(original));
    memcpy(bytes, data->state[0], 80);
    memcpy(bytes + 80, data->state[1], 80);
    CHECK(reconstruct(&fixture, data, entropy, 16, 0, address, 35) == ZCL_NOT_FOUND);
    for (size_t length = 0; length < sizeof(bytes); ++length) {
        CHECK(fixture_write(&fixture, ".change.index", bytes, length) == 0);
        CHECK(reconstruct(&fixture, data, entropy, 16, 0, address, 35) ==
            (length == 80 ? ZCL_NOT_FOUND : ZCL_INVALID_ENCODING));
        CHECK(memcmp(address, original, 35) == 0 && change_bytes(&fixture, bytes, length, 0) == 0);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    for (size_t byte = 0; byte < 80; ++byte) {
        bytes[80 + byte] ^= 1;
        CHECK(fixture_write(&fixture, ".change.index", bytes, sizeof(bytes)) == 0);
        CHECK(reconstruct(&fixture, data, entropy, 16, 0, address, 35) != ZCL_OK);
        CHECK(memcmp(address, original, 35) == 0 && change_bytes(&fixture, bytes, sizeof(bytes), 0) == 0);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
        bytes[80 + byte] ^= 1;
    }
    memcpy(bytes + 80, data->state[2], 80); /* Authentic but at the wrong position. */
    CHECK(fixture_write(&fixture, ".change.index", bytes, sizeof(bytes)) == 0);
    CHECK(reconstruct(&fixture, data, entropy, 16, 0, address, 35) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(address, original, 35) == 0 && change_bytes(&fixture, bytes, sizeof(bytes), 0) == 0);
    return fixture_close(&fixture);
}

static int exhausted_state(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(consumed_fixture(&fixture, data) == 0);
    uint8_t entropy[16] = {0}, blind[32] = {1}, head[80] = {0};
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, 16, blind, 32,
        ZCL_CHANGE_STORAGE_MAX_RECORDS - 1, head, sizeof(head)) == ZCL_OK);
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES) == 0);
    CHECK(pwrite(fd, head, 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 80 && close(fd) == 0);
    /* Synthetic authenticated head/position only; intervening holes do not
     * claim a reproduced reservation history or filesystem-rollback defense. */
    zcl_change_storage_snapshot before = {0};
    CHECK(change_observe(&fixture, data, &before) == ZCL_OK);
    uint8_t address[35], expected[35];
    size_t length = 0;
    CHECK(zcl_change_from_entropy(entropy, 16, ZCL_TESTNET, 65534, blind, 32, expected, 35, &length) == ZCL_OK);
    CHECK(length == 35 && reconstruct(&fixture, data, entropy, 16, 65534, address, 35) == ZCL_OK);
    CHECK(memcmp(address, expected, 35) == 0);
    CHECK(reconstruct(&fixture, data, entropy, 16, 65535, address, 35) == ZCL_OUT_OF_RANGE);
    CHECK(memcmp(address, expected, 35) == 0 && unchanged(&fixture, data, &before) == 0);
    zcl_change_reservation reservation = {0};
    CHECK(zcl_wallet_change_reserve((const uint8_t *)fixture.path, fixture_path_len(), data->wallet,
        data->wallet_len, entropy, 16, &reservation) == ZCL_OUT_OF_RANGE);
    CHECK(change_bytes(&fixture, head, 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 0);
    return fixture_close(&fixture);
}

static int bad_custody(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(consumed_fixture(&fixture, data) == 0);
    zcl_change_storage_snapshot before = {0};
    CHECK(change_observe(&fixture, data, &before) == ZCL_OK);
    uint8_t entropy[16] = {0}, wrong[16] = {1}, address[35], original[35];
    memset(address, 0xa5, sizeof(address));
    memcpy(original, address, sizeof(original));
    CHECK(reconstruct(&fixture, data, wrong, 16, 0, address, 35) != ZCL_OK);
    CHECK(memcmp(address, original, 35) == 0);
    change_storage_data other = *data;
    other.wallet[other.wallet_len - 1] ^= 1;
    CHECK(reconstruct(&fixture, &other, entropy, 16, 0, address, 35) == ZCL_ALREADY_EXISTS);
    CHECK(memcmp(address, original, 35) == 0 && unchanged(&fixture, data, &before) == 0);
    return fixture_close(&fixture);
}

static int bounds(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(consumed_fixture(&fixture, data) == 0);
    uint8_t entropy[16] = {0}, address[35];
    memset(address, 0xa5, sizeof(address));
    const uint8_t *path = (const uint8_t *)fixture.path;
    CHECK(reconstruct(&fixture, data, NULL, 16, 0, address, 35) == ZCL_INVALID_ARGUMENT);
    CHECK(reconstruct(&fixture, data, entropy, SIZE_MAX, 0, address, 35) == ZCL_OUT_OF_RANGE);
    CHECK(reconstruct(&fixture, data, entropy, 0, 0, address, 35) == ZCL_OUT_OF_RANGE);
    CHECK(reconstruct(&fixture, data, entropy, 16, 0, NULL, 35) == ZCL_INVALID_ARGUMENT);
    CHECK(reconstruct(&fixture, data, entropy, 16, UINT32_MAX, address, 35) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_change_reserved_address(NULL, 1, data->wallet, data->wallet_len, entropy, 16, 0, address, 35) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_reserved_address(path, SIZE_MAX, data->wallet, data->wallet_len, entropy, 16, 0, address, 35) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_change_reserved_address(path, fixture_path_len(), NULL, data->wallet_len, entropy, 16, 0, address, 35) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_reserved_address(path, fixture_path_len(), data->wallet, SIZE_MAX, entropy, 16, 0, address, 35) == ZCL_OUT_OF_RANGE);
    for (size_t length = 0; length < data->wallet_len; ++length)
        CHECK(zcl_wallet_change_reserved_address(path, fixture_path_len(), data->wallet, length, entropy, 16, 0, address, 35) != ZCL_OK);
    for (size_t i = 0; i < sizeof(address); ++i) CHECK(address[i] == 0xa5);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    const zcl_network networks[2] = {ZCL_MAINNET, ZCL_TESTNET};
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4) CHECK(matrix_case(networks[network], length) == 0);
    CHECK(capacities(&data) == 0);
    CHECK(malformed_state(&data) == 0);
    CHECK(exhausted_state(&data) == 0);
    CHECK(bad_custody(&data) == 0);
    CHECK(bounds(&data) == 0);
    CHECK(puts("Previously consumed change addresses reconstruct without journal mutation; invalid state refuses") >= 0);
    return 0;
}
