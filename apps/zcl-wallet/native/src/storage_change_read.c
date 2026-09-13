/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_change_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

_Static_assert(ZCL_CHANGE_STORAGE_MAX_BYTES <= INT32_MAX, "offset fits signed32 off_t");
_Static_assert(ZCL_CHANGE_STORAGE_MAX_BYTES / 80 == ZCL_CHANGE_STORAGE_MAX_RECORDS,
    "fixed bounded record capacity");

zcl_status zcl_store_match_wallet(const zcl_store *store, const uint8_t *record, size_t length)
{
    uint8_t existing[140] = {0};
    size_t existing_len = 0;
    zcl_status status = zcl_store_read_file(store, ZCL_STORE_COMMITTED,
        existing, sizeof(existing), &existing_len, false);
    if (status != ZCL_OK) return status;
    if (existing_len != length || memcmp(existing, record, length) != 0)
        return ZCL_ALREADY_EXISTS;
    return ZCL_OK;
}

static zcl_status change_size(int fd, uint32_t *size)
{
    struct stat info = {0};
    if (fstat(fd, &info) != 0) return ZCL_IO_FAILURE;
    if (!S_ISREG(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077) != 0)
        return ZCL_IO_FAILURE;
    if (info.st_nlink != 1) return ZCL_IO_FAILURE;
    if (info.st_size < 0 || info.st_size > ZCL_CHANGE_STORAGE_MAX_BYTES)
        return ZCL_OUT_OF_RANGE;
    *size = (uint32_t)info.st_size;
    return ZCL_OK;
}

static zcl_status read_tail(int fd, zcl_change_storage_snapshot *snapshot)
{
    size_t offset = 0;
    const uint32_t start = snapshot->file_bytes - (uint32_t)snapshot->tail_len;
    for (size_t attempt = 0; attempt < 256 && offset < snapshot->tail_len; ++attempt) {
        /* start+offset <= validated file size <=5MiB, fits uint32 and off_t. */
        off_t position = (off_t)(start + (uint32_t)offset);
        ssize_t count = pread(fd, snapshot->tail + offset, snapshot->tail_len - offset, position);
        if (count < 0) {
            if (errno == EINTR) continue;
            return ZCL_IO_FAILURE;
        }
        if (count == 0 || (size_t)count > snapshot->tail_len - offset)
            return ZCL_INVALID_ENCODING;
        offset += (size_t)count;
    }
    return offset == snapshot->tail_len ? ZCL_OK : ZCL_IO_FAILURE;
}

zcl_status zcl_store_change_open(const zcl_store *store, bool append, int *fd,
    zcl_change_storage_snapshot *snapshot)
{
    int mode = append ? O_RDWR | O_APPEND : O_RDONLY;
    *fd = openat(store->directory, zcl_store_name(ZCL_STORE_CHANGE),
        mode | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (*fd < 0) return errno == ENOENT ? ZCL_NOT_FOUND : ZCL_IO_FAILURE;
    zcl_status status = change_size(*fd, &snapshot->file_bytes);
    if (status != ZCL_OK) return status;
    snapshot->tail_len = snapshot->file_bytes < 80 ? (size_t)snapshot->file_bytes : 80;
    status = read_tail(*fd, snapshot);
    uint32_t final_size = 0;
    if (status == ZCL_OK) status = change_size(*fd, &final_size);
    if (status == ZCL_OK && final_size != snapshot->file_bytes) status = ZCL_BUSY;
    return status;
}

zcl_status zcl_store_change_close(int *fd, zcl_status status)
{
    if (*fd >= 0 && close(*fd) != 0 && status == ZCL_OK) status = ZCL_IO_UNCERTAIN;
    *fd = -1; /* Linux/Android close is consumed even on EINTR; never retry. */
    return status;
}

zcl_status zcl_store_change_matches(const zcl_change_storage_snapshot *actual,
    const zcl_change_storage_snapshot *expected)
{
    if (actual->tail_len > sizeof(actual->tail) || expected->tail_len > sizeof(expected->tail))
        return ZCL_INVALID_ENCODING;
    if (actual->file_bytes != expected->file_bytes || actual->tail_len != expected->tail_len)
        return ZCL_BUSY;
    return memcmp(actual->tail, expected->tail, expected->tail_len) == 0 ? ZCL_OK : ZCL_BUSY;
}

zcl_status zcl_storage_change_observe(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, zcl_change_storage_snapshot *snapshot)
{
    if (snapshot == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(wallet_record, wallet_len, &parsed);
    if (status != ZCL_OK) return status;
    zcl_store store = {-1, -1};
    int fd = -1;
    zcl_change_storage_snapshot candidate = {0};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK) status = zcl_store_match_wallet(&store, wallet_record, wallet_len);
    if (status == ZCL_OK) status = zcl_store_change_open(&store, false, &fd, &candidate);
    status = zcl_store_change_close(&fd, status);
    status = zcl_store_close(&store, status);
    if (status == ZCL_OK) *snapshot = candidate;
    return status;
}
