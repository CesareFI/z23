/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static zcl_status reserve(const storage_fixture *fixture, const change_storage_data *data,
    const uint8_t *entropy, size_t entropy_len, zcl_change_reservation *result)
{
    return zcl_wallet_change_reserve((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, entropy_len, result);
}

static zcl_status create(const storage_fixture *fixture, const change_storage_data *data,
    const uint8_t *entropy, size_t entropy_len)
{
    return zcl_wallet_change_create((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, entropy_len);
}

static int matched_result(const zcl_change_reservation *result, const uint8_t *entropy,
    size_t entropy_len, zcl_network network, uint32_t index)
{
    uint8_t expected[35] = {0}, blinding[32] = {1};
    size_t length = 0;
    CHECK(zcl_change_from_entropy(entropy, entropy_len, network, index, blinding, sizeof(blinding),
        expected, sizeof(expected), &length) == ZCL_OK);
    CHECK(length == 35 && result->index == index && result->network == network);
    CHECK(memcmp(result->address, expected, 35) == 0);
    return 0;
}

static int matrix_case(zcl_network network, size_t entropy_len)
{
    change_storage_data data = {0};
    uint8_t entropy[32] = {0}, blinding[32] = {1}, header[80] = {0};
    uint8_t ciphertext[48] = {0}, iv[12] = {0};
    CHECK(zcl_wallet_header_create(entropy, entropy_len, network, blinding, sizeof(blinding),
        header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, sizeof(header), iv, sizeof(iv), ciphertext, entropy_len + 16,
        data.wallet, sizeof(data.wallet), &data.wallet_len) == ZCL_OK);
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(create(&fixture, &data, entropy, entropy_len) == ZCL_OK);
    CHECK(create(&fixture, &data, entropy, entropy_len) == ZCL_ALREADY_EXISTS);
    struct { uint8_t before[8]; zcl_change_reservation result; uint8_t after[8]; } guarded;
    memset(&guarded, 0xa5, sizeof(guarded));
    for (uint32_t index = 0; index < 2; ++index) {
        CHECK(reserve(&fixture, &data, entropy, entropy_len, &guarded.result) == ZCL_OK);
        CHECK(matched_result(&guarded.result, entropy, entropy_len, network, index) == 0);
        for (size_t i = 0; i < 8; ++i) CHECK(guarded.before[i] == 0xa5 && guarded.after[i] == 0xa5);
        zcl_change_storage_snapshot snapshot = {0};
        CHECK(change_observe(&fixture, &data, &snapshot) == ZCL_OK);
        uint32_t next_index = 0;
        CHECK(zcl_change_state_decode(data.wallet, 80, entropy, entropy_len, blinding, sizeof(blinding),
            snapshot.tail, snapshot.tail_len, &next_index) == ZCL_OK);
        CHECK(next_index == index + 1 && snapshot.file_bytes == (index + 2) * 80);
    }
    return fixture_close(&fixture);
}

static int refused_custody(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t wrong[16] = {1}, entropy[16] = {0};
    CHECK(fixture_open(&fixture) == 0);
    CHECK(create(&fixture, data, wrong, sizeof(wrong)) != ZCL_OK);
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(fstatat(fixture.directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(create(&fixture, data, entropy, sizeof(entropy)) == ZCL_OK);
    zcl_change_reservation result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    CHECK(reserve(&fixture, data, wrong, sizeof(wrong), &result) != ZCL_OK);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    change_storage_data different = *data;
    different.wallet[different.wallet_len - 1] ^= 1;
    CHECK(reserve(&fixture, &different, entropy, sizeof(entropy), &result) == ZCL_ALREADY_EXISTS);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    return fixture_close(&fixture);
}

static int malformed_state(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t entropy[16] = {0}, changed[80] = {0};
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    zcl_change_reservation result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) == ZCL_NOT_FOUND);
    for (size_t length = 0; length < 80; ++length) {
        CHECK(fixture_write(&fixture, ".change.index", data->state[0], length) == 0);
        CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) == ZCL_INVALID_ENCODING);
        CHECK(memcmp(&result, &before, sizeof(result)) == 0);
        CHECK(change_bytes(&fixture, data->state[0], length, 0) == 0);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    for (size_t byte = 0; byte < 80; ++byte) {
        memcpy(changed, data->state[0], 80);
        changed[byte] ^= 1;
        CHECK(fixture_write(&fixture, ".change.index", changed, 80) == 0);
        CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) != ZCL_OK);
        CHECK(memcmp(&result, &before, sizeof(result)) == 0);
        CHECK(change_bytes(&fixture, changed, 80, 0) == 0);
        CHECK(unlinkat(fixture.directory, ".change.index", 0) == 0);
    }
    CHECK(fixture_write(&fixture, ".change.index", data->state[1], 80) == 0);
    CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    CHECK(change_bytes(&fixture, data->state[1], 80, 0) == 0);
    return fixture_close(&fixture);
}

static int exhausted_storage(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t entropy[16] = {0}, blinding[32] = {1}, final_head[80] = {0};
    CHECK(fixture_open(&fixture) == 0 && create(&fixture, data, entropy, sizeof(entropy)) == ZCL_OK);
    /* Sparse PUBLIC test history exercises the exact bounded tail/counter
     * boundary. It does not claim that earlier gap records authenticate. */
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, sizeof(entropy), blinding, sizeof(blinding),
        ZCL_CHANGE_STORAGE_MAX_RECORDS - 2, final_head, sizeof(final_head)) == ZCL_OK);
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 80) == 0);
    CHECK(pwrite(fd, final_head, 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 160) == 80);
    CHECK(close(fd) == 0);
    zcl_change_reservation result = {0};
    CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) == ZCL_OK);
    CHECK(matched_result(&result, entropy, sizeof(entropy), ZCL_TESTNET, 65534) == 0);
    zcl_change_reservation before;
    memcpy(&before, &result, sizeof(before));
    CHECK(reserve(&fixture, data, entropy, sizeof(entropy), &result) == ZCL_OUT_OF_RANGE);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == ZCL_CHANGE_STORAGE_MAX_BYTES);
    uint32_t next_index = 0;
    CHECK(zcl_change_state_decode(data->wallet, 80, entropy, sizeof(entropy), blinding, sizeof(blinding),
        snapshot.tail, snapshot.tail_len, &next_index) == ZCL_OK && next_index == 65535);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    CHECK(change_bytes(&fixture, final_head, 80, (off_t)ZCL_CHANGE_STORAGE_MAX_BYTES - 160) == 0);
    return fixture_close(&fixture);
}

static int bounds(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t entropy[16] = {0};
    CHECK(fixture_open(&fixture) == 0 && create(&fixture, data, entropy, sizeof(entropy)) == ZCL_OK);
    zcl_change_reservation result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    const uint8_t *path = (const uint8_t *)fixture.path;
    CHECK(reserve(&fixture, data, NULL, 16, &result) == ZCL_INVALID_ARGUMENT);
    CHECK(reserve(&fixture, data, entropy, SIZE_MAX, &result) == ZCL_OUT_OF_RANGE);
    CHECK(reserve(&fixture, data, entropy, 0, &result) == ZCL_OUT_OF_RANGE);
    CHECK(reserve(&fixture, data, entropy, 16, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_reserve(NULL, 1, data->wallet, data->wallet_len, entropy, 16, &result) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_reserve(path, SIZE_MAX, data->wallet, data->wallet_len, entropy, 16, &result) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_change_reserve(path, fixture_path_len(), NULL, data->wallet_len, entropy, 16, &result) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_reserve(path, fixture_path_len(), data->wallet, SIZE_MAX, entropy, 16, &result) == ZCL_OUT_OF_RANGE);
    for (size_t length = 0; length < data->wallet_len; ++length) {
        CHECK(zcl_wallet_change_reserve(path, fixture_path_len(), data->wallet, length, entropy, 16, &result) != ZCL_OK);
        CHECK(zcl_wallet_change_create(path, fixture_path_len(), data->wallet, length, entropy, 16) != ZCL_OK);
    }
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    return fixture_close(&fixture);
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    const zcl_network networks[2] = {ZCL_MAINNET, ZCL_TESTNET};
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4)
            CHECK(matrix_case(networks[network], length) == 0);
    CHECK(refused_custody(&data) == 0);
    CHECK(malformed_state(&data) == 0);
    CHECK(exhausted_storage(&data) == 0);
    CHECK(bounds(&data) == 0);
    puts("change reservation: recovered wallet/MAC/index binding, both networks/all entropy widths, no reset, final index and output atomicity passed");
    return 0;
}
