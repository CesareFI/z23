/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "storage_change_internal.h"

zcl_status zcl_store_change_plan_repair(const zcl_change_storage_snapshot *snapshot,
    zcl_change_repair_plan *plan)
{
    if (snapshot == NULL || plan == NULL) return ZCL_INVALID_ARGUMENT;
    if (snapshot->file_bytes > ZCL_CHANGE_STORAGE_MAX_BYTES) return ZCL_OUT_OF_RANGE;
    size_t tail_len = snapshot->file_bytes < 80 ? (size_t)snapshot->file_bytes : 80;
    if (snapshot->tail_len != tail_len) return ZCL_INVALID_ENCODING;
    uint32_t slots = snapshot->file_bytes / 80;
    if (snapshot->file_bytes % 80 != 0) ++slots;
    if (slots == 0) slots = 1; /* Empty existing state must burn at least0. */
    if (slots >= ZCL_CHANGE_STORAGE_MAX_RECORDS) return ZCL_OUT_OF_RANGE;
    /* slots<=65535: multiplication fits uint32 and is>=original size. */
    zcl_change_repair_plan candidate = {slots, (size_t)(slots * UINT32_C(80) - snapshot->file_bytes)};
    *plan = candidate;
    return ZCL_OK;
}

static zcl_status repair_record(const zcl_change_storage_snapshot *expected,
    const uint8_t *replacement, size_t state_len, zcl_change_repair_plan *plan)
{
    zcl_status status = zcl_store_change_plan_repair(expected, plan);
    uint32_t next_index = 0;
    if (status == ZCL_OK) status = zcl_change_state_inspect(replacement, state_len, &next_index);
    if (status == ZCL_OK && next_index != plan->next_index) status = ZCL_INVALID_ENCODING;
    return status;
}

static zcl_status write_repair(int fd, const zcl_change_repair_plan *plan,
    const uint8_t *replacement, size_t state_len)
{
    uint8_t padding[80] = {0};
    /* Private plan proved0..80 padding and <=160 total growth within cap. */
    zcl_status status = zcl_store_write_bytes(fd, padding, plan->padding);
    if (status == ZCL_OK) status = zcl_store_write_bytes(fd, replacement, state_len);
    if (status == ZCL_OK) status = zcl_store_sync(fd);
    return status;
}

static zcl_status repair_locked(const zcl_store *store, const zcl_change_storage_snapshot *expected,
    const zcl_change_repair_plan *plan, const uint8_t *replacement, size_t state_len)
{
    int fd = -1;
    zcl_change_storage_snapshot actual = {0};
    zcl_status status = zcl_store_change_open(store, true, &fd, &actual);
    if (status == ZCL_OK) status = zcl_store_change_matches(&actual, expected);
    if (status == ZCL_OK) status = write_repair(fd, plan, replacement, state_len);
    status = zcl_store_change_close(&fd, status);
    if (status == ZCL_OK) status = zcl_store_sync(store->directory);
    return status;
}

zcl_status zcl_storage_change_repair(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len,
    const zcl_change_storage_snapshot *expected, const uint8_t *replacement, size_t state_len)
{
    zcl_wallet_record parsed = {0};
    zcl_change_repair_plan plan = {0};
    zcl_status status = zcl_wallet_record_parse(wallet_record, wallet_len, &parsed);
    if (status == ZCL_OK) status = repair_record(expected, replacement, state_len, &plan);
    if (status != ZCL_OK) return status;
    zcl_store store = {-1, -1};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK) status = zcl_store_match_wallet(&store, wallet_record, wallet_len);
    if (status == ZCL_OK) status = repair_locked(&store, expected, &plan, replacement, state_len);
    return zcl_store_close(&store, status);
}
