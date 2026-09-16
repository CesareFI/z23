/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#undef zcl_random_bytes
#undef zcl_secure_zero
#undef zcl_change_state_encode
#undef zcl_change_state_decode
#undef zcl_wallet_recovered_change
#include "change_storage_fixture.h"
#include "change_custody_retirement.h"
#include "../src/change_custody_internal.h"
#include "storage_faults.h"
#include "zcl_change_reservation.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "reservation fault at%d\n", __LINE__); abort(); } } while (0)
typedef enum { FAIL_NONE, FAIL_DECODE, FAIL_DERIVE, FAIL_ENCODE } crypto_failure;
typedef struct { void *pointer; size_t length; bool cleared; } blind_span;
static blind_span spans[3];
static size_t random_calls, fail_random;
static crypto_failure fail_crypto;
static uint8_t *mutate_cipher;
static const storage_fixture *competing_fixture;
static const change_storage_data *competing_data;

zcl_status zcl_reservation_test_random(uint8_t *output, size_t length)
{
    REQUIRE(output != NULL && (length == 32 || length == 64) && random_calls < 3);
    spans[random_calls] = (blind_span){output, length, false};
    ++random_calls;
    if (mutate_cipher != NULL) {
        *mutate_cipher ^= 1; /* Adversarial callback after the private copy. */
        mutate_cipher = NULL;
    }
    if (competing_fixture != NULL && random_calls == 3) {
        zcl_change_storage_snapshot old = {0};
        REQUIRE(change_observe(competing_fixture, competing_data, &old) == ZCL_OK);
        REQUIRE(change_append(competing_fixture, competing_data, &old, 1) == ZCL_OK);
        competing_fixture = NULL;
        competing_data = NULL;
    }
    if (random_calls == fail_random) {
        memset(output, 0x42, length / 2); /* Stronger partial-error contract. */
        return ZCL_IO_FAILURE;
    }
    memset(output, 0x42, length); /* Public fixture blinding, source-only hook. */
    return ZCL_OK;
}

void zcl_reservation_test_zero(void *pointer, size_t length)
{
    if (change_custody_retirement_zero(pointer, length)) return;
    REQUIRE(pointer != NULL && (length == 32 || length == 64));
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    bool found = false;
    for (size_t i = 0; i < random_calls; ++i) {
        if (spans[i].pointer != pointer) continue;
        REQUIRE(spans[i].length == length && !spans[i].cleared);
        spans[i].pointer = NULL; /* Retire while the span is still live. */
        spans[i].cleared = true;
        found = true;
    }
    REQUIRE(found);
}

zcl_status zcl_reservation_test_encode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint32_t index, uint8_t *record, size_t capacity)
{
    if (fail_crypto == FAIL_ENCODE) {
        REQUIRE(record != NULL && capacity >= 80);
        memset(record, 0x42, 16);
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_change_state_encode(header, header_len, entropy, entropy_len, blinding, blinding_len,
        index, record, capacity);
}

zcl_status zcl_reservation_test_decode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    const uint8_t *record, size_t record_len, uint32_t *index)
{
    if (fail_crypto == FAIL_DECODE) {
        REQUIRE(index != NULL);
        *index = UINT32_MAX;
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_change_state_decode(header, header_len, entropy, entropy_len, blinding, blinding_len,
        record, record_len, index);
}

zcl_status zcl_reservation_test_derive(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, uint32_t index, const uint8_t *blinding, size_t blinding_len,
    uint8_t *address, size_t capacity)
{
    if (fail_crypto == FAIL_DERIVE) {
        REQUIRE(address != NULL && capacity >= 35);
        memset(address, 0x42, 16);
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_wallet_recovered_change(header, header_len, entropy, entropy_len, index,
        blinding, blinding_len, address, capacity);
}

static void checked_clear(void)
{
    change_custody_retirement_check();
    for (size_t i = 0; i < random_calls; ++i) REQUIRE(spans[i].cleared && spans[i].pointer == NULL);
    memset(spans, 0, sizeof(spans));
    random_calls = fail_random = 0;
    fail_crypto = FAIL_NONE;
    REQUIRE(mutate_cipher == NULL && competing_fixture == NULL && competing_data == NULL);
    storage_faults_reset();
}

static zcl_status reserve(const storage_fixture *fixture, const change_storage_data *data,
    zcl_change_reservation *result)
{
    static const uint8_t entropy[16] = {0};
    return zcl_wallet_change_reserve((const uint8_t *)fixture->path, fixture_path_len(),
        data->wallet, data->wallet_len, entropy, sizeof(entropy), result);
}

static int failed_creation(const change_storage_data *data, bool random_failure)
{
    storage_fixture fixture;
    uint8_t entropy[16] = {0};
    CHECK(fixture_open(&fixture) == 0);
    checked_clear();
    fail_random = random_failure ? 1 : 0;
    fail_crypto = random_failure ? FAIL_NONE : FAIL_ENCODE;
    CHECK(zcl_wallet_change_create((const uint8_t *)fixture.path, fixture_path_len(), data->wallet,
        data->wallet_len, entropy, sizeof(entropy)) != ZCL_OK);
    CHECK(random_calls == 1);
    checked_clear();
    struct stat info = {0};
    CHECK(fstatat(fixture.directory, ".change.index", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    CHECK(fstatat(fixture.directory, "wallet.zcl", &info, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT);
    return fixture_close(&fixture);
}

static int failed_reservation(const change_storage_data *data, size_t rng_failure,
    crypto_failure crypto, io_fault *fault, io_mode mode, size_t at, uint32_t bytes, size_t expected_random)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    zcl_change_reservation result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    checked_clear();
    fail_random = rng_failure;
    fail_crypto = crypto;
    if (fault != NULL) *fault = (io_fault){mode, at, 0};
    CHECK(reserve(&fixture, data, &result) != ZCL_OK);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0 && random_calls == expected_random);
    checked_clear();
    zcl_change_storage_snapshot snapshot = {0};
    CHECK(change_observe(&fixture, data, &snapshot) == ZCL_OK && snapshot.file_bytes == bytes);
    CHECK(change_bytes(&fixture, data->state[0], 80, 0) == 0);
    if (bytes == 120) {
        CHECK(reserve(&fixture, data, &result) == ZCL_INVALID_ENCODING && random_calls == 0);
        CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    } else {
        CHECK(reserve(&fixture, data, &result) == ZCL_OK);
        CHECK(result.index == (bytes == 160 ? 1U : 0U));
    }
    checked_clear();
    return fixture_close(&fixture);
}

static int private_binding_and_competition(const change_storage_data *data)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    change_storage_data mutable = *data;
    zcl_change_reservation result = {0};
    checked_clear();
    mutate_cipher = &mutable.wallet[mutable.wallet_len - 1];
    CHECK(reserve(&fixture, &mutable, &result) == ZCL_OK && result.index == 0);
    checked_clear();
    CHECK(change_bytes(&fixture, data->state[1], 80, 80) == 0);
    CHECK(fixture_close(&fixture) == 0);
    CHECK(fixture_open(&fixture) == 0 && change_create(&fixture, data) == ZCL_OK);
    memset(&result, 0xa5, sizeof(result));
    zcl_change_reservation before;
    memcpy(&before, &result, sizeof(before));
    competing_fixture = &fixture;
    competing_data = data;
    CHECK(reserve(&fixture, data, &result) == ZCL_BUSY);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    checked_clear();
    CHECK(reserve(&fixture, data, &result) == ZCL_OK && result.index == 1);
    checked_clear();
    return fixture_close(&fixture);
}

static int preparation_refusals(const change_storage_data *data)
{
    const uint8_t entropy[16] = {0};
    uint8_t invalid[140];
    memcpy(invalid, data->wallet, sizeof(invalid));
    invalid[4] = 0xff;
    for (unsigned failure = 0; failure < 6; ++failure) {
        zcl_change_custody wallet = {0};
        const uint8_t *record = failure == 0 ? NULL : failure == 1 ? invalid : data->wallet;
        const uint8_t *secret = failure == 2 ? NULL : entropy;
        const size_t record_len = failure == 3 ? 123 : data->wallet_len;
        const size_t entropy_len = failure == 4 ? 15 : sizeof(entropy);
        const zcl_status status = zcl_change_custody_prepare(record, record_len, secret, entropy_len, &wallet);
        CHECK((status == ZCL_OK) == (failure == 5));
        if (status == ZCL_OK) {
            CHECK(wallet.entropy == entropy && wallet.entropy_len == sizeof(entropy));
            CHECK(wallet.record_len == data->wallet_len && memcmp(wallet.record, data->wallet, data->wallet_len) == 0);
        }
        zcl_change_custody_clear(&wallet);
        checked_clear();
    }
    CHECK(zcl_change_custody_prepare(data->wallet, data->wallet_len, entropy, sizeof(entropy), NULL)
        == ZCL_INVALID_ARGUMENT);
    zcl_change_custody_clear(NULL);
    checked_clear();
    CHECK(zcl_wallet_change_create(NULL, 0, NULL, 0, entropy, sizeof(entropy)) == ZCL_INVALID_ARGUMENT);
    checked_clear();
    zcl_change_reservation reservation = {0};
    CHECK(zcl_wallet_change_reserve(NULL, 0, NULL, 0, entropy, sizeof(entropy), &reservation)
        == ZCL_INVALID_ARGUMENT);
    checked_clear();
    return 0;
}

int main(void)
{
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    CHECK(preparation_refusals(&data) == 0);
    CHECK(failed_creation(&data, true) == 0 && failed_creation(&data, false) == 0);
    for (size_t at = 1; at <= 3; ++at) {
        CHECK(failed_reservation(&data, at, FAIL_NONE, NULL, IO_NORMAL, 0, 80, at) == 0);
        CHECK(failed_reservation(&data, 0, (crypto_failure)at, NULL, IO_NORMAL, 0, 80, at) == 0);
    }
    for (size_t at = 1; at <= 10; ++at)
        CHECK(failed_reservation(&data, 0, FAIL_NONE, &storage_close_fault, IO_INTERRUPT, at,
            at >= 8 ? 160 : 80, at >= 6 ? 3 : 0) == 0);
    for (size_t at = 1; at <= 4; ++at)
        CHECK(failed_reservation(&data, 0, FAIL_NONE, &storage_sync_fault, IO_ERROR, at,
            at >= 3 ? 160 : 80, at >= 2 ? 3 : 0) == 0);
    CHECK(failed_reservation(&data, 0, FAIL_NONE, &storage_write_fault, IO_PARTIAL_ERROR, 0, 120, 3) == 0);
    CHECK(private_binding_and_competition(&data) == 0);
    puts("change reservation faults: private wallet binding, live blinding cleanup, crypto failures, uncertain consumption and CAS refusal passed");
    return 0;
}
