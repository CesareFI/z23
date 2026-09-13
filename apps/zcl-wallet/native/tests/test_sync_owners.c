/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_owners.h"
#include "sync_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sync owner check failed at %d\n", __LINE__); abort(); } } while (0)

static zcl_status open_owner(zcl_sync_owners *owners, zcl_network network, uint64_t *id)
{
    zcl_sync fixture;
    sync_fixture_start(&fixture, network, 1);
    const uint8_t source[32] = {1};
    return zcl_sync_owners_open(owners, fixture.candidate.address, 35, network, source, sizeof(source), id);
}

static void closed_callback_cannot_reach_reused_slot(void)
{
    zcl_sync_owners owners = {0};
    uint64_t first = 0, second = 0, first_attempt = 0, second_attempt = 0;
    zcl_sync_watch *watch = NULL;
    CHECK(open_owner(&owners, ZCL_MAINNET, &first) == ZCL_OK);
    CHECK(zcl_sync_owners_get(&owners, first, &watch) == ZCL_OK);
    CHECK(zcl_sync_watch_begin(watch, 0, 100, 1, &first_attempt) == ZCL_OK);
    CHECK(zcl_sync_owners_close(&owners, first) == ZCL_OK);
    watch = NULL; /* Borrow never spans a close/reopen. */
    CHECK(open_owner(&owners, ZCL_TESTNET, &second) == ZCL_OK && second > first);
    CHECK(zcl_sync_owners_get(&owners, second, &watch) == ZCL_OK);
    CHECK(zcl_sync_watch_begin(watch, 0, 100, 1, &second_attempt) == ZCL_OK);
    CHECK(first_attempt == second_attempt); /* Numeric attempt tokens can match. */
    zcl_sync_owners before;
    memcpy(&before, &owners, sizeof(before));
    zcl_sync_watch *late = NULL;
    CHECK(zcl_sync_owners_get(&owners, first, &late) == ZCL_CANCELLED && late == NULL);
    CHECK(zcl_sync_owners_close(&owners, first) == ZCL_CANCELLED);
    CHECK(memcmp(&before, &owners, sizeof(before)) == 0);
    CHECK(watch->network == ZCL_TESTNET && watch->in_flight);
    zcl_sync_owners_close_all(&owners);
}

static void bounded_capacity_and_close_all(void)
{
    zcl_sync_owners owners = {0};
    uint64_t ids[ZCL_SYNC_OWNER_CAPACITY] = {0};
    for (size_t i = 0; i < ZCL_SYNC_OWNER_CAPACITY; ++i)
        CHECK(open_owner(&owners, ZCL_MAINNET, &ids[i]) == ZCL_OK && ids[i] == i + 1);
    zcl_sync_owners before;
    memcpy(&before, &owners, sizeof(before));
    uint64_t unchanged = 99;
    CHECK(open_owner(&owners, ZCL_MAINNET, &unchanged) == ZCL_RESOURCE_EXHAUSTED && unchanged == 99);
    CHECK(memcmp(&before, &owners, sizeof(before)) == 0);
    zcl_sync_owners_close_all(&owners);
    const zcl_sync_owner_slot empty = {0};
    for (size_t i = 0; i < ZCL_SYNC_OWNER_CAPACITY; ++i) {
        zcl_sync_watch *watch = NULL;
        CHECK(zcl_sync_owners_get(&owners, ids[i], &watch) == ZCL_CANCELLED && watch == NULL);
        CHECK(memcmp(&owners.slots[i], &empty, sizeof(empty)) == 0);
    }
    CHECK(open_owner(&owners, ZCL_MAINNET, &unchanged) == ZCL_OK && unchanged > ids[ZCL_SYNC_OWNER_CAPACITY - 1]);
    zcl_sync_owners_close_all(&owners);
}

static void saturation_and_invalid_arguments(void)
{
    zcl_sync_owners owners = {0};
    uint64_t id = 0;
    owners.issued = ZCL_SYNC_OWNER_ID_MAX - 1; /* Explicit exhaustion fixture. */
    CHECK(open_owner(&owners, ZCL_MAINNET, &id) == ZCL_OK && id == ZCL_SYNC_OWNER_ID_MAX);
    CHECK(zcl_sync_owners_close(&owners, id) == ZCL_OK);
    zcl_sync_owners_close_all(&owners);
    CHECK(open_owner(&owners, ZCL_MAINNET, &id) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(id == ZCL_SYNC_OWNER_ID_MAX);
    zcl_sync_watch *watch = NULL;
    CHECK(zcl_sync_owners_get(&owners, 0, &watch) == ZCL_CANCELLED);
    CHECK(zcl_sync_owners_get(&owners, UINT64_MAX, &watch) == ZCL_CANCELLED);
    CHECK(zcl_sync_owners_close(&owners, 0) == ZCL_CANCELLED);
    CHECK(zcl_sync_owners_close(&owners, UINT64_MAX) == ZCL_CANCELLED);
    CHECK(zcl_sync_owners_close(NULL, 1) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_sync_owners_get(NULL, 1, &watch) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_sync_owners_get(&owners, 1, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(open_owner(NULL, ZCL_MAINNET, &id) == ZCL_INVALID_ARGUMENT);
    CHECK(open_owner(&owners, ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    zcl_sync_owners_close_all(NULL);
}

static void invalid_open_is_transactional(void)
{
    zcl_sync_owners owners = {0};
    uint64_t id = 55;
    const uint8_t source[32] = {1};
    zcl_sync fixture;
    sync_fixture_start(&fixture, ZCL_MAINNET, 1);
    CHECK(zcl_sync_owners_open(&owners, fixture.candidate.address, 35, ZCL_TESTNET,
        source, sizeof(source), &id) == ZCL_UNSUPPORTED);
    CHECK(zcl_sync_owners_open(&owners, fixture.candidate.address, 35, ZCL_MAINNET,
        source, sizeof(source) - 1, &id) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_sync_owners_open(&owners, NULL, 0, ZCL_MAINNET,
        source, sizeof(source), &id) == ZCL_INVALID_ARGUMENT);
    const zcl_sync_owners empty = {0};
    CHECK(id == 55 && memcmp(&owners, &empty, sizeof(empty)) == 0);
    CHECK(open_owner(&owners, ZCL_MAINNET, &id) == ZCL_OK && id == 1);
    zcl_sync_owners_close_all(&owners);
}

int main(void)
{
    closed_callback_cannot_reach_reused_slot(); bounded_capacity_and_close_all();
    saturation_and_invalid_arguments(); invalid_open_is_transactional();
    puts("Sync owners: bounded slots, immutable failed opens, no ID reuse, stale callback rejection and clear-all passed");
    return 0;
}
