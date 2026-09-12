/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "sync_fixture.h"
#include "electrum_genesis_fixture.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sync fixture failed at %d\n", __LINE__); abort(); } } while (0)

void sync_fixture_start(zcl_sync *session, zcl_network network, uint32_t first_id)
{
    const uint8_t zero_hash[20] = {0};
    uint8_t address[35];
    size_t length = 0;
    CHECK(zcl_address_from_hash(zero_hash, sizeof(zero_hash), network,
        address, sizeof(address), &length) == ZCL_OK);
    CHECK(zcl_sync_start(session, address, length, network, first_id) == ZCL_OK);
}

size_t sync_fixture_reply(zcl_network network, unsigned step, uint32_t id,
                           char *frame, size_t capacity)
{
    const char *header = network == ZCL_MAINNET ? main_genesis : test_genesis;
    const char *genesis = network == ZCL_MAINNET ?
        "0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602" :
        "03e1c4bb705c871bf9bfda3e74b7f8f86bff267993c215a89d5795e3708e5e1f";
    int count = -1;
    switch (step) {
    case 1:
        count = snprintf(frame, capacity, "{\"id\":%" PRIu32 ",\"result\":[\"public fixture\",\"1.2\"]}", id);
        break;
    case 2:
        count = snprintf(frame, capacity,
            "{\"id\":%" PRIu32 ",\"result\":{\"genesis_hash\":\"%s\",\"hash_function\":\"sha256\"}}", id, genesis);
        break;
    case 3:
        count = snprintf(frame, capacity,
            "{\"id\":%" PRIu32 ",\"result\":{\"count\":1,\"max\":2016,\"hex\":\"%s\"}}", id, header);
        break;
    case 4: case 6:
        count = snprintf(frame, capacity,
            "{\"id\":%" PRIu32 ",\"result\":{\"height\":0,\"hex\":\"%s\"}}", id, header);
        break;
    case 5:
        count = snprintf(frame, capacity,
            "{\"id\":%" PRIu32 ",\"result\":{\"confirmed\":1000,\"unconfirmed\":-7}}", id);
        break;
    default:
        abort();
    }
    CHECK(count > 0 && (size_t)count < capacity);
    return (size_t)count;
}
