/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_owners.h"
#include "sync_fixture.h"
#include <stdlib.h>
#include <string.h>

#define REQUIRE(v) do { if (!(v)) abort(); } while (0)

static void inspect_owner(zcl_sync_owners *owners, uint64_t id, bool live, uint64_t now)
{
    zcl_sync_watch *watch = NULL;
    const zcl_status expected = live ? ZCL_OK : ZCL_CANCELLED;
    REQUIRE(zcl_sync_owners_get(owners, id, &watch) == expected);
    if (expected == ZCL_OK) {
        uint64_t token = 0;
        const zcl_status status = zcl_sync_watch_begin(watch, now, 1, 1, &token);
        REQUIRE(status == ZCL_OK || status == ZCL_BUSY);
        zcl_sync_snapshot snapshot = {0};
        REQUIRE(zcl_sync_watch_snapshot(watch, now, &snapshot) == ZCL_OK);
        REQUIRE(snapshot.freshness == ZCL_BALANCE_UNAVAILABLE);
    } else REQUIRE(watch == NULL);
}

static void inspect_history(zcl_sync_owners *owners, const uint64_t *ids, const bool *live, size_t issued)
{
    for (size_t old = 0; old < issued; ++old) {
        zcl_sync_watch *watch = NULL;
        REQUIRE(zcl_sync_owners_get(owners, ids[old], &watch) == (live[old] ? ZCL_OK : ZCL_CANCELLED));
        if (!live[old]) REQUIRE(watch == NULL);
    }
}

/* Exercise acquire/release/callback histories, including slot reuse, closed
 * owners, resource bounds and attempts whose numeric tokens coincide. No live
 * sockets or process-wide registry. The harness caps histories at 128 events. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
    if (length == 0 || length > 128) return 0;
    zcl_sync_owners owners = {0};
    zcl_sync fixture;
    sync_fixture_start(&fixture, (data[0] & 1) != 0 ? ZCL_TESTNET : ZCL_MAINNET, 1);
    uint64_t ids[128] = {0};
    bool live[128] = {false};
    size_t issued = 0;
    const uint8_t source[32] = {1};
    for (size_t event = 0; event < length; ++event) {
        const size_t selected = issued == 0 ? 0 : (size_t)data[event] % issued;
        const unsigned action = (unsigned)data[event] % 4;
        if (action == 0 && issued < 128) {
            uint64_t id = UINT64_MAX;
            const zcl_status status = zcl_sync_owners_open(&owners, fixture.candidate.address, 35,
                fixture.candidate.network, source, sizeof(source), &id);
            if (status == ZCL_OK) {
                if (issued != 0) REQUIRE(id > ids[issued - 1]);
                ids[issued] = id;
                live[issued++] = true;
            } else {
                REQUIRE(status == ZCL_RESOURCE_EXHAUSTED && id == UINT64_MAX);
            }
        } else if (action == 1) {
            const zcl_status expected = live[selected] ? ZCL_OK : ZCL_CANCELLED;
            REQUIRE(zcl_sync_owners_close(&owners, ids[selected]) == expected);
            live[selected] = false;
        } else if (action == 2) {
            zcl_sync_owners_close_all(&owners);
            memset(live, 0, sizeof(live));
        } else {
            inspect_owner(&owners, ids[selected], live[selected], (uint64_t)event);
        }
        inspect_history(&owners, ids, live, issued);
    }
    zcl_sync_owners_close_all(&owners);
    return 0;
}
