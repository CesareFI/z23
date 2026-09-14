/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static zcl_status file_size(int fd, size_t *length)
{
    struct stat info = {0};
    if (fstat(fd, &info) != 0)
        return ZCL_IO_FAILURE;
    if (!S_ISREG(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077) != 0
        || info.st_nlink != 1)
        return ZCL_IO_FAILURE;
    if (info.st_size < 124 || info.st_size > 140)
        return ZCL_INVALID_ENCODING;
    *length = (size_t)info.st_size;
    return ZCL_OK;
}

static zcl_status read_bytes(int fd, uint8_t *bytes, size_t length)
{
    size_t offset = 0;
    for (size_t attempt = 0; attempt < 256 && offset < length; ++attempt) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return ZCL_IO_FAILURE;
        }
        if (count == 0 || (size_t)count > length - offset)
            return ZCL_INVALID_ENCODING;
        offset += (size_t)count;
    }
    return offset == length ? ZCL_OK : ZCL_IO_FAILURE;
}

static zcl_status require_eof(int fd)
{
    uint8_t extra = 0;
    for (size_t attempt = 0; attempt < 16; ++attempt) {
        ssize_t count = read(fd, &extra, sizeof(extra));
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            return ZCL_IO_FAILURE;
        return count == 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
    }
    return ZCL_IO_FAILURE;
}

static zcl_status read_descriptor(int fd, uint8_t *bytes, size_t *size)
{
    zcl_status status = file_size(fd, size);
    if (status == ZCL_OK)
        status = read_bytes(fd, bytes, *size);
    if (status == ZCL_OK)
        status = require_eof(fd);
    return status;
}

/* Consumes fd exactly once, including read/sync failure. */
static zcl_status read_and_close(int fd, uint8_t *bytes, size_t *size, bool durable)
{
    zcl_status status = read_descriptor(fd, bytes, size);
    if (status == ZCL_OK && durable)
        status = zcl_store_sync(fd);
    if (close(fd) != 0 && status == ZCL_OK)
        status = ZCL_IO_FAILURE;
    return status;
}

zcl_status zcl_store_read_file(const zcl_store *store, zcl_store_slot slot,
                              uint8_t *record, size_t capacity, size_t *length, bool durable)
{
    const char *name = zcl_store_name(slot);
    if (store == NULL || name == NULL || record == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    int fd = openat(store->directory, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return errno == ENOENT ? ZCL_NOT_FOUND : ZCL_IO_FAILURE;
    uint8_t scratch[140] = {0};
    size_t size = 0;
    zcl_status status = read_and_close(fd, scratch, &size, durable);
    if (status != ZCL_OK)
        return status;
    if (capacity < size)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(record, scratch, size);
    *length = size;
    return ZCL_OK;
}

static zcl_status read_record(const zcl_store *store, uint8_t *record, size_t capacity,
                              size_t *length, bool *pending)
{
    zcl_status status = zcl_store_read_file(store, ZCL_STORE_COMMITTED, record, capacity, length, false);
    if (status == ZCL_NOT_FOUND) {
        status = zcl_store_read_file(store, ZCL_STORE_PENDING, record, capacity, length, false);
        *pending = status == ZCL_OK;
    }
    if (status != ZCL_OK)
        return status;
    zcl_wallet_record parsed = {0};
    return zcl_wallet_record_parse(record, *length, &parsed);
}

zcl_status zcl_storage_read(const uint8_t *directory, size_t directory_len,
                           uint8_t *record, size_t capacity, size_t *record_len, bool *pending)
{
    if (record == NULL || record_len == NULL || pending == NULL)
        return ZCL_INVALID_ARGUMENT;
    uint8_t scratch[140] = {0};
    size_t length = 0;
    bool was_pending = false;
    zcl_store store = {-1, -1};
    zcl_status status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK)
        status = read_record(&store, scratch, sizeof(scratch), &length, &was_pending);
    status = zcl_store_close(&store, status);
    if (status != ZCL_OK)
        return status;
    if (capacity < length)
        return ZCL_BUFFER_TOO_SMALL;
    memcpy(record, scratch, length);
    *record_len = length;
    *pending = was_pending;
    return ZCL_OK;
}
