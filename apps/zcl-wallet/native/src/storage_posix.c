/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

const char *zcl_store_name(zcl_store_slot slot)
{
    if (slot == ZCL_STORE_COMMITTED)
        return "wallet.zcl";
    if (slot == ZCL_STORE_PENDING)
        return ".wallet.pending";
    return NULL;
}

static bool valid_component(const uint8_t *part, size_t length)
{
    if (length == 0)
        return false;
    if (length == 1 && part[0] == '.')
        return false;
    return !(length == 2 && part[0] == '.' && part[1] == '.');
}

static bool valid_components(const uint8_t *path, size_t length)
{
    size_t start = 1;
    for (size_t end = 1; end <= length; ++end) {
        if (end != length && path[end] != '/')
            continue;
        if (!valid_component(path + start, end - start))
            return false;
        start = end + 1;
    }
    return true;
}

static zcl_status copy_path(const uint8_t *path, size_t length, char *text, size_t capacity)
{
    if (path == NULL || text == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (length == 0 || length > ZCL_STORAGE_PATH_MAX || capacity <= length)
        return ZCL_OUT_OF_RANGE;
    if (path[0] != '/' || memchr(path, 0, length) != NULL)
        return ZCL_INVALID_ENCODING;
    if (!valid_components(path, length))
        return ZCL_INVALID_ENCODING;
    memcpy(text, path, length);
    text[length] = '\0'; /* Bounded conversion solely for the POSIX path API. */
    return ZCL_OK;
}

static zcl_status sync_parent(const char *path, size_t length)
{
    char parent[1025] = {0};
    size_t slash = 0;
    /* Called only after copy_path validated the bound and every component. */
    for (size_t i = 0; i < length; ++i) {
        if (path[i] == '/')
            slash = i;
    }
    size_t parent_len = slash == 0 ? 1 : slash;
    memcpy(parent, path, parent_len);
    int fd = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return ZCL_IO_FAILURE;
    zcl_status status = zcl_store_sync(fd);
    if (close(fd) != 0 && status == ZCL_OK)
        status = ZCL_IO_UNCERTAIN;
    return status;
}

static zcl_status directory_open(const uint8_t *path, size_t length, int *output)
{
    char text[1025] = {0};
    zcl_status status = copy_path(path, length, text, sizeof(text));
    if (status != ZCL_OK)
        return status;
    if (mkdir(text, 0700) != 0 && errno != EEXIST)
        return ZCL_IO_FAILURE;
    int fd = open(text, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return ZCL_IO_FAILURE;
    /* Ownership transfers immediately; the caller's single close path handles
     * validation failures too. Never close/reopen a validated pathname. */
    *output = fd;
    struct stat info = {0};
    if (fstat(fd, &info) != 0)
        return ZCL_IO_FAILURE;
    if (!S_ISDIR(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077) != 0)
        return ZCL_IO_FAILURE;
    /* Flushing the child alone does not make a newly created directory entry
     * durable. Also covers a retry after an earlier parent-sync failure. */
    return sync_parent(text, length);
}

static zcl_status lock_open(zcl_store *store)
{
    store->lock = openat(store->directory, ".lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600);
    if (store->lock < 0)
        return ZCL_IO_FAILURE;
    struct stat info = {0};
    if (fstat(store->lock, &info) != 0)
        return ZCL_IO_FAILURE;
    if (!S_ISREG(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 077) != 0)
        return ZCL_IO_FAILURE;
    if (info.st_size != 0 || info.st_nlink != 1)
        return ZCL_IO_FAILURE;
    if (flock(store->lock, LOCK_EX | LOCK_NB) == 0)
        return ZCL_OK;
    return errno == EWOULDBLOCK ? ZCL_BUSY : ZCL_IO_FAILURE;
}

zcl_status zcl_store_open(const uint8_t *path, size_t path_len, zcl_store *store)
{
    if (store == NULL || store->directory != -1 || store->lock != -1)
        return ZCL_INVALID_ARGUMENT;
    zcl_status status = directory_open(path, path_len, &store->directory);
    if (status == ZCL_OK)
        status = lock_open(store);
    return status;
}

zcl_status zcl_store_close(zcl_store *store, zcl_status status)
{
    if (store == NULL)
        return ZCL_INVALID_ARGUMENT;
    bool failed = false;
    if (store->lock >= 0 && close(store->lock) != 0)
        failed = true;
    if (store->directory >= 0 && close(store->directory) != 0)
        failed = true;
    store->lock = -1;
    store->directory = -1;
    /* Do not retry close after EINTR: on Linux/Android its descriptor may
     * already have been released and reused. No fd escapes this call. */
    return failed && status == ZCL_OK ? ZCL_IO_UNCERTAIN : status;
}

zcl_status zcl_store_absent(const zcl_store *store, zcl_store_slot slot)
{
    const char *name = zcl_store_name(slot);
    if (store == NULL || name == NULL)
        return ZCL_INVALID_ARGUMENT;
    struct stat info = {0};
    if (fstatat(store->directory, name, &info, AT_SYMLINK_NOFOLLOW) == 0)
        return ZCL_ALREADY_EXISTS;
    return errno == ENOENT ? ZCL_OK : ZCL_IO_FAILURE;
}
