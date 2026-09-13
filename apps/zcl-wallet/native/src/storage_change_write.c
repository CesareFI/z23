/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_change_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static zcl_status initial_record(const uint8_t *state, size_t length)
{
    uint32_t next_index = 0;
    zcl_status status = zcl_change_state_inspect(state, length, &next_index);
    if (status == ZCL_OK && next_index != 0) status = ZCL_INVALID_ENCODING;
    return status;
}

static zcl_status create_initial(const zcl_store *store, const uint8_t *state, size_t length)
{
    int fd = openat(store->directory, zcl_store_name(ZCL_STORE_CHANGE),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if (fd < 0) return errno == EEXIST ? ZCL_ALREADY_EXISTS : ZCL_IO_FAILURE;
    zcl_status status = zcl_store_write_bytes(fd, state, length);
    if (status == ZCL_OK) status = zcl_store_sync(fd);
    status = zcl_store_change_close(&fd, status);
    /* No wallet pending write may start before the state name is durable. */
    if (status == ZCL_OK) status = zcl_store_sync(store->directory);
    return status;
}

static zcl_status create_pair(const zcl_store *store, const uint8_t *wallet, size_t wallet_len,
    const uint8_t *state, size_t state_len)
{
    zcl_status status = zcl_store_absent(store, ZCL_STORE_COMMITTED);
    if (status == ZCL_OK) status = zcl_store_absent(store, ZCL_STORE_PENDING);
    if (status == ZCL_OK) status = zcl_store_absent(store, ZCL_STORE_CHANGE);
    if (status == ZCL_OK) status = create_initial(store, state, state_len);
    if (status == ZCL_OK) status = zcl_store_write_pending(store, wallet, wallet_len);
    if (status == ZCL_OK) status = zcl_store_commit(store);
    return status;
}

zcl_status zcl_storage_create_with_change(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *initial_state, size_t state_len)
{
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(wallet_record, wallet_len, &parsed);
    if (status == ZCL_OK) status = initial_record(initial_state, state_len);
    if (status != ZCL_OK) return status;
    zcl_store store = {-1, -1};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK)
        status = create_pair(&store, wallet_record, wallet_len, initial_state, state_len);
    return zcl_store_close(&store, status);
}

static zcl_status append_records(const zcl_change_storage_snapshot *expected,
    const uint8_t *next_state, size_t state_len)
{
    if (expected == NULL) return ZCL_INVALID_ARGUMENT;
    if (expected->file_bytes < 80 || expected->file_bytes % 80 != 0 || expected->tail_len != 80)
        return ZCL_INVALID_ENCODING;
    if (expected->file_bytes >= ZCL_CHANGE_STORAGE_MAX_BYTES) return ZCL_OUT_OF_RANGE;
    uint32_t previous = 0, next = 0;
    zcl_status status = zcl_change_state_inspect(expected->tail, expected->tail_len, &previous);
    if (status == ZCL_OK) status = zcl_change_state_inspect(next_state, state_len, &next);
    if (status != ZCL_OK) return status;
    const uint32_t position = expected->file_bytes / 80;
    if (previous != position - 1 || next != position) return ZCL_INVALID_ENCODING;
    return ZCL_OK;
}

static zcl_status append_locked(const zcl_store *store, const zcl_change_storage_snapshot *expected,
    const uint8_t *state, size_t state_len)
{
    int fd = -1;
    zcl_change_storage_snapshot actual = {0};
    zcl_status status = zcl_store_change_open(store, true, &fd, &actual);
    if (status == ZCL_OK && actual.file_bytes != expected->file_bytes) status = ZCL_BUSY;
    if (status == ZCL_OK && (actual.tail_len != expected->tail_len ||
        memcmp(actual.tail, expected->tail, expected->tail_len) != 0)) status = ZCL_BUSY;
    if (status == ZCL_OK) status = zcl_store_write_bytes(fd, state, state_len);
    if (status == ZCL_OK) status = zcl_store_sync(fd);
    status = zcl_store_change_close(&fd, status);
    if (status == ZCL_OK) status = zcl_store_sync(store->directory);
    return status;
}

zcl_status zcl_storage_change_append(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len,
    const zcl_change_storage_snapshot *expected, const uint8_t *next_state, size_t state_len)
{
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(wallet_record, wallet_len, &parsed);
    if (status == ZCL_OK) status = append_records(expected, next_state, state_len);
    if (status != ZCL_OK) return status;
    zcl_store store = {-1, -1};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK) status = zcl_store_match_wallet(&store, wallet_record, wallet_len);
    if (status == ZCL_OK) status = append_locked(&store, expected, next_state, state_len);
    return zcl_store_close(&store, status);
}
