/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_sync_watch_init
#undef zcl_sync_watch_init_with_history
#undef zcl_secure_zero
#include "zcl_sync_owners.h"
#include "zcl_keys.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sync owner retirement at %d\n", __LINE__); abort(); } } while (0)
static zcl_sync_owners owners, before;
static zcl_sync fixture;
static uint8_t source[32] = {1};
static uintptr_t staged_identity;
static void *expected_clear;
static size_t expected_length;
static unsigned starts, staged_clears, retained_clears;
static bool dirty_failure;

zcl_status zcl_owner_test_init(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length);
zcl_status zcl_owner_test_init_history(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length);
void zcl_owner_test_zero(void *buffer, size_t length);

static zcl_status initialize(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length, bool history)
{
    CHECK(staged_identity == 0 && starts == staged_clears);
    staged_identity = (uintptr_t)watch;
    ++starts;
    if (dirty_failure) {
        memset(watch, 0xa5, sizeof(*watch));
        return ZCL_CRYPTO_FAILURE;
    }
    return history ? zcl_sync_watch_init_with_history(watch, address, length, network, source_id, source_length)
        : zcl_sync_watch_init(watch, address, length, network, source_id, source_length);
}

zcl_status zcl_owner_test_init(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length)
{
    return initialize(watch, address, length, network, source_id, source_length, false);
}

zcl_status zcl_owner_test_init_history(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length)
{
    return initialize(watch, address, length, network, source_id, source_length, true);
}

void zcl_owner_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if ((uintptr_t)buffer == staged_identity) {
        CHECK(length == sizeof(zcl_sync_watch));
        staged_identity = 0;
        ++staged_clears;
    } else {
        CHECK(buffer == expected_clear && length == expected_length);
        CHECK(staged_identity == 0 && starts == staged_clears);
        expected_clear = NULL;
        expected_length = 0;
        ++retained_clears;
    }
}

static zcl_status open_owner(unsigned mode, bool history, uint64_t *id)
{
    const uint8_t *address = mode == 1 ? NULL : fixture.candidate.address;
    const size_t length = mode == 2 ? 34 : 35;
    const zcl_network network = mode == 3 ? ZCL_TESTNET : ZCL_MAINNET;
    const size_t source_length = mode == 4 ? 31 : 32;
    dirty_failure = mode == 5;
    const zcl_status status = history
        ? zcl_sync_owners_open_with_history(&owners, address, length, network, source, source_length, id)
        : zcl_sync_owners_open(&owners, address, length, network, source, source_length, id);
    CHECK(staged_identity == 0 && starts == staged_clears);
    dirty_failure = false;
    return status;
}

static void close_slot(size_t index)
{
    const uint64_t issued = owners.issued;
    const uint64_t id = owners.slots[index].id;
    CHECK(id != 0 && expected_clear == NULL);
    expected_clear = &owners.slots[index];
    expected_length = sizeof(owners.slots[index]);
    const unsigned count = retained_clears;
    CHECK(zcl_sync_owners_close(&owners, id) == ZCL_OK);
    CHECK(retained_clears == count + 1 && expected_clear == NULL && owners.issued == issued);
    CHECK(zcl_sync_owners_close(&owners, id) == ZCL_CANCELLED && retained_clears == count + 1);
}

static void close_all(void)
{
    const uint64_t issued = owners.issued;
    CHECK(expected_clear == NULL);
    expected_clear = owners.slots;
    expected_length = sizeof(owners.slots);
    const unsigned count = retained_clears;
    zcl_sync_owners_close_all(&owners);
    CHECK(retained_clears == count + 1 && expected_clear == NULL && owners.issued == issued);
}

static void refusal_and_replacement(void)
{
    uint64_t first = 0;
    CHECK(open_owner(0, false, &first) == ZCL_OK);
    for (unsigned mode = 1; mode <= 5; ++mode) for (unsigned history = 0; history < 2; ++history) {
        memcpy(&before, &owners, sizeof(before));
        uint64_t id = UINT64_MAX;
        CHECK(open_owner(mode, history != 0, &id) != ZCL_OK && id == UINT64_MAX);
        CHECK(memcmp(&owners, &before, sizeof(owners)) == 0);
    }
    close_slot(0);
    uint64_t second = 0;
    CHECK(open_owner(0, true, &second) == ZCL_OK && second > first);
    zcl_sync_watch *watch = NULL;
    CHECK(zcl_sync_owners_get(&owners, first, &watch) == ZCL_CANCELLED && watch == NULL);
    CHECK(zcl_sync_owners_get(&owners, second, &watch) == ZCL_OK);
    CHECK(watch->initialized && watch->include_history);
    CHECK(memcmp(watch->source_id, source, sizeof(source)) == 0);
    CHECK(memcmp(watch->address, fixture.candidate.address, sizeof(watch->address)) == 0);
    close_all();
}

static void capacity_and_saturation(void)
{
    uint64_t id = 0;
    for (size_t i = 0; i < ZCL_SYNC_OWNER_CAPACITY; ++i) CHECK(open_owner(0, true, &id) == ZCL_OK);
    memcpy(&before, &owners, sizeof(before));
    const unsigned count = starts;
    CHECK(open_owner(0, false, &id) == ZCL_RESOURCE_EXHAUSTED && starts == count);
    CHECK(memcmp(&owners, &before, sizeof(owners)) == 0);
    close_all();
    owners.issued = ZCL_SYNC_OWNER_ID_MAX - 1;
    CHECK(open_owner(0, true, &id) == ZCL_OK && id == ZCL_SYNC_OWNER_ID_MAX);
    close_slot(0);
    CHECK(open_owner(0, false, &id) == ZCL_RESOURCE_EXHAUSTED && id == ZCL_SYNC_OWNER_ID_MAX);
    close_all();
}

int main(void)
{
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    owners.issued = 41;
    refusal_and_replacement();
    capacity_and_saturation();
    CHECK(zcl_sync_owners_close(NULL, 1) == ZCL_INVALID_ARGUMENT);
    zcl_sync_owners_close_all(NULL);
    CHECK(staged_identity == 0 && expected_clear == NULL && starts == staged_clears);
    puts("Sync owner staged and retained retirement checks passed");
    return 0;
}
