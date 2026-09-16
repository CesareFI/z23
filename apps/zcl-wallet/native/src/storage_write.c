/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _GNU_SOURCE
#include "storage_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

zcl_status zcl_store_sync(int fd)
{
    for (size_t attempt = 0; attempt < 16; ++attempt) {
        if (fsync(fd) == 0)
            return ZCL_OK;
        if (errno != EINTR)
            return ZCL_IO_UNCERTAIN;
    }
    return ZCL_IO_UNCERTAIN;
}

zcl_status zcl_store_write_bytes(int fd, const uint8_t *record, size_t length)
{
    size_t offset = 0;
    for (size_t attempt = 0; attempt < 256 && offset < length; ++attempt) {
        ssize_t count = write(fd, record + offset, length - offset);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return ZCL_IO_UNCERTAIN;
        }
        if (count == 0 || (size_t)count > length - offset)
            return ZCL_IO_UNCERTAIN;
        offset += (size_t)count;
    }
    return offset == length ? ZCL_OK : ZCL_IO_UNCERTAIN;
}

zcl_status zcl_store_write_pending(const zcl_store *store, const uint8_t *record, size_t length)
{
    if (store == NULL)
        return ZCL_INVALID_ARGUMENT;
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(record, length, &parsed);
    zcl_secure_zero(&parsed, sizeof(parsed));
    if (status != ZCL_OK)
        return status;
    int fd = openat(store->directory, zcl_store_name(ZCL_STORE_PENDING),
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return errno == EEXIST ? ZCL_ALREADY_EXISTS : ZCL_IO_FAILURE;
    status = zcl_store_write_bytes(fd, record, length);
    if (status == ZCL_OK)
        status = zcl_store_sync(fd);
    if (close(fd) != 0)
        status = ZCL_IO_UNCERTAIN;
    /* Never erase a failed pending file. A complete one is recoverable after
     * authentication; incomplete data remains an explicit recovery condition. */
    return status;
}

zcl_status zcl_store_commit(const zcl_store *store)
{
    if (store == NULL)
        return ZCL_INVALID_ARGUMENT;
    /* Persist the pending name first, so an interrupted rename has a durable
     * recovery source. Android app policy may prohibit hard-link creation. */
    zcl_status status = zcl_store_sync(store->directory);
    if (status != ZCL_OK)
        return status;
#if defined(__linux__)
    if (renameat2(store->directory, zcl_store_name(ZCL_STORE_PENDING),
                   store->directory, zcl_store_name(ZCL_STORE_COMMITTED), RENAME_NOREPLACE) != 0)
        return errno == EEXIST ? ZCL_ALREADY_EXISTS : ZCL_IO_UNCERTAIN;
    return zcl_store_sync(store->directory);
#else
    /* No check-then-replacing-rename fallback: that would lose no-overwrite. */
    return ZCL_UNSUPPORTED;
#endif
}

zcl_status zcl_storage_create(const uint8_t *directory, size_t directory_len,
                             const uint8_t *record, size_t record_len)
{
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(record, record_len, &parsed);
    zcl_secure_zero(&parsed, sizeof(parsed));
    if (status != ZCL_OK)
        return status;
    zcl_store store = {-1, -1};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK)
        status = zcl_store_absent(&store, ZCL_STORE_COMMITTED);
    if (status == ZCL_OK)
        status = zcl_store_absent(&store, ZCL_STORE_PENDING);
    if (status == ZCL_OK)
        status = zcl_store_absent(&store, ZCL_STORE_CHANGE);
    if (status == ZCL_OK)
        status = zcl_store_write_pending(&store, record, record_len);
    if (status == ZCL_OK)
        status = zcl_store_commit(&store);
    return zcl_store_close(&store, status);
}

static zcl_status promote_record(const zcl_store *store, const uint8_t *record, size_t length)
{
    uint8_t existing[140] = {0};
    size_t existing_len = 0;
    zcl_status status = zcl_store_read_file(store, ZCL_STORE_COMMITTED, existing, sizeof(existing), &existing_len, true);
    const bool committed = status == ZCL_OK;
    if (status == ZCL_NOT_FOUND)
        status = zcl_store_read_file(store, ZCL_STORE_PENDING, existing, sizeof(existing), &existing_len, true);
    if (status == ZCL_OK && (existing_len != length || memcmp(existing, record, length) != 0))
        status = committed ? ZCL_ALREADY_EXISTS : ZCL_INVALID_ENCODING;
    zcl_secure_zero(existing, sizeof(existing));
    if (status != ZCL_OK)
        return status;
    /* An already committed retry must still re-establish directory durability.
     * The compared copy is retired before either potentially blocking action. */
    return committed ? zcl_store_sync(store->directory) : zcl_store_commit(store);
}

zcl_status zcl_storage_promote(const uint8_t *directory, size_t directory_len,
                              const uint8_t *authenticated_record, size_t record_len)
{
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(authenticated_record, record_len, &parsed);
    zcl_secure_zero(&parsed, sizeof(parsed));
    if (status != ZCL_OK)
        return status;
    zcl_store store = {-1, -1};
    status = zcl_store_open(directory, directory_len, &store);
    if (status == ZCL_OK)
        status = promote_record(&store, authenticated_record, record_len);
    return zcl_store_close(&store, status);
}
