/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include "zcl_change_reservation.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static zcl_status recover(const storage_fixture *fixture, const change_storage_data *data)
{
    const uint8_t entropy[16] = {0};
    return zcl_wallet_change_recover((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, sizeof(entropy));
}

static int unchanged(const storage_fixture *fixture, const uint8_t *bytes, size_t length)
{
    struct stat info = {0};
    CHECK(fstatat(fixture->directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) == 0);
    CHECK(info.st_size >= 0 && (uintmax_t)info.st_size == length);
    return change_bytes(fixture, bytes, length, 0);
}

static int repaired_head(const storage_fixture *fixture, const change_storage_data *data, uint32_t index)
{
    uint8_t entropy[16] = {0}, blind[32] = {1};
    uint32_t decoded = 0;
    zcl_change_storage_snapshot current = {0};
    CHECK(change_observe(fixture, data, &current) == ZCL_OK);
    CHECK(current.file_bytes == (index + 1) * 80 && current.tail_len == 80);
    CHECK(zcl_change_state_decode(data->wallet, 80, entropy, sizeof(entropy), blind, sizeof(blind),
        current.tail, current.tail_len, &decoded) == ZCL_OK && decoded == index);
    CHECK(recover(fixture, data) == ZCL_ALREADY_EXISTS);
    zcl_change_reservation result = {0};
    CHECK(zcl_wallet_change_reserve((const uint8_t *)fixture->path, fixture_path_len(), data->wallet,
        data->wallet_len, entropy, sizeof(entropy), &result) == ZCL_OK && result.index == index);
    return 0;
}

static int partial_suffixes(const change_storage_data *data)
{
    uint8_t bytes[160] = {0};
    memcpy(bytes, data->state[0], 80);
    memcpy(bytes + 80, data->state[1], 80);
    for (size_t length = 0; length < 160; ++length) {
        storage_fixture fixture;
        CHECK(fixture_open(&fixture) == 0);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
        CHECK(fixture_write(&fixture, ".change.index", bytes, length) == 0);
        zcl_status status = recover(&fixture, data);
        if (length >= 96) {
            CHECK(status == ZCL_OK);
            CHECK(change_bytes(&fixture, bytes, length, 0) == 0);
            const uint8_t zeros[80] = {0};
            CHECK(change_bytes(&fixture, zeros, 160 - length, (off_t)length) == 0);
            CHECK(repaired_head(&fixture, data, 2) == 0);
        } else {
            CHECK(status == (length == 80 ? ZCL_ALREADY_EXISTS :
                length > 80 ? ZCL_UNSUPPORTED : ZCL_INVALID_ENCODING));
            CHECK(unchanged(&fixture, bytes, length) == 0);
        }
        CHECK(fixture_close(&fixture) == 0);
    }
    return 0;
}

static int complete_and_misaligned(const change_storage_data *data)
{
    uint8_t bytes[240] = {0};
    memcpy(bytes, data->state, sizeof(bytes));
    for (size_t length = 80; length <= 240; length += 80) {
        storage_fixture fixture;
        CHECK(fixture_open(&fixture) == 0);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
        CHECK(fixture_write(&fixture, ".change.index", bytes, length) == 0);
        CHECK(recover(&fixture, data) == ZCL_ALREADY_EXISTS && unchanged(&fixture, bytes, length) == 0);
        CHECK(fixture_close(&fixture) == 0);
    }
    /* A valid MAC at a wrong slot or a misaligned offset must never be treated
     * as mere damage. It can reveal a larger counter than the file permits. */
    for (size_t gap = 0; gap < 2; ++gap) {
        storage_fixture fixture;
        CHECK(fixture_open(&fixture) == 0);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
        memcpy(bytes, data->state[0], 80);
        bytes[80] = 0x42;
        memcpy(bytes + 80 + gap, data->state[gap == 0 ? 2 : 1], 80);
        CHECK(fixture_write(&fixture, ".change.index", bytes, 160 + gap) == 0);
        CHECK(recover(&fixture, data) == ZCL_INVALID_ENCODING);
        CHECK(unchanged(&fixture, bytes, 160 + gap) == 0);
        CHECK(fixture_close(&fixture) == 0);
    }
    return 0;
}

static int corrupted_suffix(const change_storage_data *data)
{
    for (size_t byte = 0; byte < 160; ++byte) {
        storage_fixture fixture;
        uint8_t bytes[160] = {0};
        memcpy(bytes, data->state[0], 80);
        memcpy(bytes + 80, data->state[1], 80);
        bytes[159] ^= 1; /* Current MAC damaged in every case. */
        if (byte != 159) bytes[byte] ^= 1;
        CHECK(fixture_open(&fixture) == 0);
        CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
        CHECK(fixture_write(&fixture, ".change.index", bytes, sizeof(bytes)) == 0);
        zcl_status status = recover(&fixture, data);
        if (byte >= 96) {
            CHECK(status == ZCL_OK && change_bytes(&fixture, bytes, sizeof(bytes), 0) == 0);
            CHECK(repaired_head(&fixture, data, 2) == 0);
        } else {
            CHECK(status != ZCL_OK && status != ZCL_ALREADY_EXISTS);
            CHECK(unchanged(&fixture, bytes, sizeof(bytes)) == 0);
        }
        CHECK(fixture_close(&fixture) == 0);
    }
    return 0;
}

static int custody_and_bounds(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t bytes[120] = {0}, entropy[16] = {0}, wrong[16] = {1};
    memcpy(bytes, data->state[0], 80);
    memcpy(bytes + 80, data->state[1], 40);
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    CHECK(recover(&fixture, data) == ZCL_NOT_FOUND);
    CHECK(fixture_write(&fixture, ".change.index", bytes, sizeof(bytes)) == 0);
    const uint8_t *path = (const uint8_t *)fixture.path;
    CHECK(zcl_wallet_change_recover(path, fixture_path_len(), data->wallet, data->wallet_len,
        wrong, sizeof(wrong)) != ZCL_OK);
    change_storage_data other = *data;
    other.wallet[other.wallet_len - 1] ^= 1;
    CHECK(recover(&fixture, &other) == ZCL_ALREADY_EXISTS);
    CHECK(zcl_wallet_change_recover(NULL, 1, data->wallet, data->wallet_len, entropy, 16) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_recover(path, SIZE_MAX, data->wallet, data->wallet_len, entropy, 16) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_change_recover(path, fixture_path_len(), NULL, 124, entropy, 16) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_recover(path, fixture_path_len(), data->wallet, SIZE_MAX, entropy, 16) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_change_recover(path, fixture_path_len(), data->wallet, data->wallet_len, NULL, 16) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_change_recover(path, fixture_path_len(), data->wallet, data->wallet_len, entropy, SIZE_MAX) == ZCL_OUT_OF_RANGE);
    for (size_t length = 0; length < data->wallet_len; ++length)
        CHECK(zcl_wallet_change_recover(path, fixture_path_len(), data->wallet, length, entropy, 16) != ZCL_OK);
    CHECK(unchanged(&fixture, bytes, sizeof(bytes)) == 0);
    return fixture_close(&fixture);
}

static int matrix_case(zcl_network network, size_t entropy_len)
{
    change_storage_data data = {0};
    uint8_t entropy[32] = {0}, blind[32] = {1}, header[80] = {0};
    uint8_t ciphertext[48] = {0}, iv[12] = {0}, original[120] = {0};
    CHECK(zcl_wallet_header_create(entropy, entropy_len, network, blind, 32, header, 80) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, 80, iv, 12, ciphertext, entropy_len + 16,
        data.wallet, sizeof(data.wallet), &data.wallet_len) == ZCL_OK);
    for (uint32_t i = 0; i < 3; ++i)
        CHECK(zcl_change_state_encode(header, 80, entropy, entropy_len, blind, 32, i, data.state[i], 80) == ZCL_OK);
    memcpy(original, data.state[0], 80);
    memcpy(original + 80, data.state[1], 40);
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data.wallet, data.wallet_len) == ZCL_OK);
    CHECK(fixture_write(&fixture, ".change.index", original, sizeof(original)) == 0);
    CHECK(zcl_wallet_change_recover((const uint8_t *)fixture.path, fixture_path_len(), data.wallet,
        data.wallet_len, entropy, entropy_len) == ZCL_OK);
    CHECK(change_bytes(&fixture, original, sizeof(original), 0) == 0);
    CHECK(change_bytes(&fixture, data.state[2], 80, 160) == 0);
    return fixture_close(&fixture);
}

static int refused_authenticated_predecessor(const change_storage_data *data)
{
    storage_fixture fixture;
    uint8_t bytes[120] = {0};
    memcpy(bytes, data->state[1], 80); /* Valid MAC, wrong predecessor position. */
    memcpy(bytes + 80, data->state[1], 40);
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_create(&fixture, data->wallet, data->wallet_len) == ZCL_OK);
    CHECK(fixture_write(&fixture, ".change.index", bytes, sizeof(bytes)) == 0);
    CHECK(recover(&fixture, data) == ZCL_INVALID_ENCODING);
    CHECK(unchanged(&fixture, bytes, sizeof(bytes)) == 0);
    return fixture_close(&fixture);
}

static int capacity(const change_storage_data *data, bool exhausted)
{
    storage_fixture fixture;
    uint8_t entropy[16] = {0}, blind[32] = {1}, previous[80] = {0}, current[80] = {0};
    uint32_t next = ZCL_CHANGE_STORAGE_MAX_RECORDS - (exhausted ? 0U : 1U);
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, 16, blind, 32, next - 2, previous, 80) == ZCL_OK);
    CHECK(zcl_change_state_encode(data->wallet, 80, entropy, 16, blind, 32, next - 1, current, 80) == ZCL_OK);
    current[79] ^= 1;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    /* Only the bounded tail and predecessor claim is exercised by sparse
     * public test history; no authentication claim for the earlier gap. */
    int fd = openat(fixture.directory, ".change.index", O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)next * 80) == 0);
    CHECK(pwrite(fd, previous, 80, (off_t)(next - 2) * 80) == 80);
    CHECK(pwrite(fd, current, 80, (off_t)(next - 1) * 80) == 80 && close(fd) == 0);
    CHECK(recover(&fixture, data) == (exhausted ? ZCL_OUT_OF_RANGE : ZCL_OK));
    CHECK(change_bytes(&fixture, previous, 80, (off_t)(next - 2) * 80) == 0);
    CHECK(change_bytes(&fixture, current, 80, (off_t)(next - 1) * 80) == 0);
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK);
    CHECK(snapshot.file_bytes == ZCL_CHANGE_STORAGE_MAX_BYTES);
    if (!exhausted) {
        uint32_t index = 0;
        CHECK(zcl_change_state_decode(data->wallet, 80, entropy, 16, blind, 32,
            snapshot.tail, 80, &index) == ZCL_OK && index == next);
    }
    return fixture_close(&fixture);
}

static int remaining_cases(const change_storage_data *data)
{
    CHECK(complete_and_misaligned(data) == 0);
    CHECK(custody_and_bounds(data) == 0);
    CHECK(refused_authenticated_predecessor(data) == 0);
    const zcl_network networks[2] = {ZCL_MAINNET, ZCL_TESTNET};
    for (size_t network = 0; network < 2; ++network)
        for (size_t length = 16; length <= 32; length += 4)
            CHECK(matrix_case(networks[network], length) == 0);
    CHECK(capacity(data, false) == 0 && capacity(data, true) == 0);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const struct { const char *name; int (*run)(const change_storage_data *); } groups[] = {
        {"partial", partial_suffixes}, {"corrupt", corrupted_suffix}, {"bounds", remaining_cases}
    };
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    bool found = false;
    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); ++i) {
        if (strcmp(argv[1], groups[i].name) != 0) continue;
        CHECK(groups[i].run(&data) == 0);
        found = true;
    }
    CHECK(found);
    puts("change recovery: authenticated predecessor, supported suffix, no reset, prefix preservation, safe next reservation and bounded capacity passed");
    return 0;
}
