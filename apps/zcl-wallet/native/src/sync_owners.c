/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_owners.h"
#include "zcl_keys.h"

static zcl_sync_owner_slot *find_slot(zcl_sync_owners *owners, uint64_t id)
{
    for (size_t index = 0; index < ZCL_SYNC_OWNER_CAPACITY; ++index) {
        if (owners->slots[index].id == id) return &owners->slots[index];
    }
    return NULL;
}

static zcl_status open_owner(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id, bool include_history)
{
    if (owners == NULL || id == NULL) return ZCL_INVALID_ARGUMENT;
    if (owners->issued >= ZCL_SYNC_OWNER_ID_MAX) return ZCL_RESOURCE_EXHAUSTED;
    zcl_sync_owner_slot *slot = find_slot(owners, 0);
    if (slot == NULL) return ZCL_RESOURCE_EXHAUSTED;
    zcl_sync_watch watch = {0};
    const zcl_status status = include_history
        ? zcl_sync_watch_init_with_history(&watch, address, length, network, source, source_length)
        : zcl_sync_watch_init(&watch, address, length, network, source, source_length);
    if (status == ZCL_OK) {
        slot->watch = watch;
        slot->id = ++owners->issued; /* Bounded above before increment. */
        *id = slot->id;
    }
    zcl_secure_zero(&watch, sizeof(watch));
    return status;
}

zcl_status zcl_sync_owners_open(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id)
{
    return open_owner(owners, address, length, network, source, source_length, id, false);
}

zcl_status zcl_sync_owners_open_with_history(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id)
{
    return open_owner(owners, address, length, network, source, source_length, id, true);
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
    zcl_secure_zero(slot, sizeof(*slot));
    return ZCL_OK;
}

void zcl_sync_owners_close_all(zcl_sync_owners *owners)
{
    if (owners != NULL) zcl_secure_zero(owners->slots, sizeof(owners->slots));
}
