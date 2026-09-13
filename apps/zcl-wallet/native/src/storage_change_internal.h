/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_STORAGE_CHANGE_INTERNAL_H
#define ZCL_STORAGE_CHANGE_INTERNAL_H
#include "storage_internal.h"
#include "zcl_change_storage.h"
#include "change_state_internal.h"

zcl_status zcl_store_match_wallet(const zcl_store *store, const uint8_t *record, size_t length);
/* Transfers opened descriptor to caller immediately, even on validation error.
 * Initialize fd=-1; close once, never retain/copy/reopen the path. */
zcl_status zcl_store_change_open(const zcl_store *store, bool append, int *fd,
    zcl_change_storage_snapshot *snapshot);
zcl_status zcl_store_change_close(int *fd, zcl_status status);
zcl_status zcl_store_change_matches(const zcl_change_storage_snapshot *actual,
    const zcl_change_storage_snapshot *expected);

typedef struct { uint32_t next_index; size_t padding; } zcl_change_repair_plan;
/* Checked public metadata only, not authentication or index authority.
 * Plan remains unchanged on failure; snapshots own all of their bytes. */
zcl_status zcl_store_change_plan_repair(const zcl_change_storage_snapshot *snapshot,
    zcl_change_repair_plan *plan);
#endif
