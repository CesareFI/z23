/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_owners.h"
#include <string.h>

static zcl_sync_owner_slot *find_slot(zcl_sync_owners *owners, uint64_t id)
{
    for (size_t index = 0; index < ZCL_SYNC_OWNER_CAPACITY; ++index) {
        if (owners->slots[index].id == id) return &owners->slots[index];
    }
    return NULL;
}

zcl_status zcl_sync_owners_open(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id)
{
    if (owners == NULL || id == NULL) return ZCL_INVALID_ARGUMENT;
    if (owners->issued >= ZCL_SYNC_OWNER_ID_MAX) return ZCL_RESOURCE_EXHAUSTED;
    zcl_sync_owner_slot *slot = find_slot(owners, 0);
    if (slot == NULL) return ZCL_RESOURCE_EXHAUSTED;
    zcl_sync_watch watch = {0};
    const zcl_status status = zcl_sync_watch_init(&watch, address, length, network, source, source_length);
    if (status != ZCL_OK) return status;
    slot->watch = watch;
    slot->id = ++owners->issued; /* Bounded above before increment. */
    *id = slot->id;
    return ZCL_OK;
}

zcl_status zcl_sync_owners_get(zcl_sync_owners *owners, uint64_t id, zcl_sync_watch **watch)
{
    if (owners == NULL || watch == NULL) return ZCL_INVALID_ARGUMENT;
    if (id == 0 || id > ZCL_SYNC_OWNER_ID_MAX) return ZCL_CANCELLED;
    zcl_sync_owner_slot *slot = find_slot(owners, id);
    if (slot == NULL) return ZCL_CANCELLED;
    *watch = &slot->watch;
    return ZCL_OK;
}

zcl_status zcl_sync_owners_close(zcl_sync_owners *owners, uint64_t id)
{
    if (owners == NULL) return ZCL_INVALID_ARGUMENT;
    if (id == 0 || id > ZCL_SYNC_OWNER_ID_MAX) return ZCL_CANCELLED;
    zcl_sync_owner_slot *slot = find_slot(owners, id);
    if (slot == NULL) return ZCL_CANCELLED;
    memset(slot, 0, sizeof(*slot));
    return ZCL_OK;
}

void zcl_sync_owners_close_all(zcl_sync_owners *owners)
{
    if (owners != NULL) memset(owners->slots, 0, sizeof(owners->slots));
}
