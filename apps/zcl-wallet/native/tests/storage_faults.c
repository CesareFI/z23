/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_faults.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

io_fault storage_read_fault, storage_write_fault, storage_sync_fault;
io_fault storage_rename_fault, storage_close_fault;
io_fault storage_pread_fault;

ssize_t __real_read(int fd, void *buffer, size_t length);
ssize_t __real_pread(int fd, void *buffer, size_t length, off_t offset);
ssize_t __real_write(int fd, const void *buffer, size_t length);
int __real_fsync(int fd);
int __real_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);
int __real_close(int fd);
ssize_t __wrap_read(int fd, void *buffer, size_t length);
ssize_t __wrap_pread(int fd, void *buffer, size_t length, off_t offset);
ssize_t __wrap_write(int fd, const void *buffer, size_t length);
int __wrap_fsync(int fd);
int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags);
int __wrap_close(int fd);

void storage_faults_reset(void)
{
    storage_read_fault = (io_fault){0};
    storage_write_fault = (io_fault){0};
    storage_sync_fault = (io_fault){0};
    storage_rename_fault = (io_fault){0};
    storage_close_fault = (io_fault){0};
    storage_pread_fault = (io_fault){0};
}

static io_mode next_fault(io_fault *fault)
{
    ++fault->calls;
    /* A selected partial-write fault begins at that call and persists as an
     * error afterward; at0 begins on the first call, as before. */
    if (fault->mode == IO_PARTIAL_ERROR && fault->at != 0 && fault->calls >= fault->at)
        return fault->mode;
    return fault->at == 0 || fault->calls == fault->at ? fault->mode : IO_NORMAL;
}

static ssize_t failed_io(io_mode mode, size_t length)
{
    if (mode == IO_ZERO)
        return 0;
    if (mode == IO_OVERSIZE)
        return (ssize_t)(length + 1); /* Only bounded test calls (<=140). */
    errno = mode == IO_INTERRUPT ? EINTR : ENOSPC;
    return -1;
}

ssize_t __wrap_read(int fd, void *buffer, size_t length)
{
    io_mode mode = next_fault(&storage_read_fault);
    if (mode == IO_NORMAL)
        return __real_read(fd, buffer, length);
    if (mode == IO_SHORT)
        return __real_read(fd, buffer, length > 1 ? 1 : length);
    return failed_io(mode, length);
}

#if defined(__GLIBC__) || defined(__ANDROID__)
/* Optimized glibc/Bionic calls can use fortified entry points. Keep their original
 * object bound even when injecting a short read; never bypass a bounds trap. */
ssize_t __real___read_chk(int fd, void *buffer, size_t length, size_t capacity);
ssize_t __wrap___read_chk(int fd, void *buffer, size_t length, size_t capacity);
ssize_t __real___pread_chk(int fd, void *buffer, size_t length, off_t offset, size_t capacity);
ssize_t __wrap___pread_chk(int fd, void *buffer, size_t length, off_t offset, size_t capacity);

ssize_t __wrap___read_chk(int fd, void *buffer, size_t length, size_t capacity)
{
    if (length > capacity) return __real___read_chk(fd, buffer, length, capacity);
    io_mode mode = next_fault(&storage_read_fault);
    if (mode == IO_NORMAL) return __real___read_chk(fd, buffer, length, capacity);
    if (mode == IO_SHORT)
        return __real___read_chk(fd, buffer, length > 1 ? 1 : length, capacity);
    return failed_io(mode, length);
}

ssize_t __wrap___pread_chk(int fd, void *buffer, size_t length, off_t offset, size_t capacity)
{
    if (length > capacity) return __real___pread_chk(fd, buffer, length, offset, capacity);
    io_mode mode = next_fault(&storage_pread_fault);
    if (mode == IO_NORMAL) return __real___pread_chk(fd, buffer, length, offset, capacity);
    if (mode == IO_SHORT)
        return __real___pread_chk(fd, buffer, length > 1 ? 1 : length, offset, capacity);
    return failed_io(mode, length);
}
#endif

ssize_t __wrap_write(int fd, const void *buffer, size_t length)
{
    io_mode mode = next_fault(&storage_write_fault);
    if (mode == IO_NORMAL)
        return __real_write(fd, buffer, length);
    if (mode == IO_SHORT)
        return __real_write(fd, buffer, length > 1 ? 1 : length);
    if (mode == IO_PARTIAL_ERROR && storage_write_fault.calls ==
        (storage_write_fault.at == 0 ? 1 : storage_write_fault.at))
        return __real_write(fd, buffer, length / 2);
    return failed_io(mode, length);
}

#if defined(__ANDROID__)
ssize_t __real___write_chk(int fd, const void *buffer, size_t length, size_t capacity);
ssize_t __wrap___write_chk(int fd, const void *buffer, size_t length, size_t capacity);

ssize_t __wrap___write_chk(int fd, const void *buffer, size_t length, size_t capacity)
{
    if (length > capacity) return __real___write_chk(fd, buffer, length, capacity);
    io_mode mode = next_fault(&storage_write_fault);
    if (mode == IO_NORMAL) return __real___write_chk(fd, buffer, length, capacity);
    if (mode == IO_SHORT)
        return __real___write_chk(fd, buffer, length > 1 ? 1 : length, capacity);
    if (mode == IO_PARTIAL_ERROR && storage_write_fault.calls ==
        (storage_write_fault.at == 0 ? 1 : storage_write_fault.at))
        return __real___write_chk(fd, buffer, length / 2, capacity);
    return failed_io(mode, length);
}
#endif

ssize_t __wrap_pread(int fd, void *buffer, size_t length, off_t offset)
{
    io_mode mode = next_fault(&storage_pread_fault);
    if (mode == IO_NORMAL) return __real_pread(fd, buffer, length, offset);
    if (mode == IO_SHORT) return __real_pread(fd, buffer, length > 1 ? 1 : length, offset);
    return failed_io(mode, length);
}

int __wrap_fsync(int fd)
{
    io_mode mode = next_fault(&storage_sync_fault);
    if (mode == IO_NORMAL)
        return __real_fsync(fd);
    errno = mode == IO_INTERRUPT ? EINTR : EIO;
    return -1;
}

int __wrap_renameat2(int olddir, const char *oldpath, int newdir, const char *newpath, unsigned flags)
{
    io_mode mode = next_fault(&storage_rename_fault);
    if (mode == IO_COLLISION) {
        /* An uncooperative writer creates the destination after our absence
         * check. Exercise the real kernel's no-replace behavior. */
        int fd = openat(newdir, newpath, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0)
            return -1;
        ssize_t count = __real_write(fd, "older", 5);
        int closed = __real_close(fd);
        if (count != 5 || closed != 0) {
            errno = EIO;
            return -1;
        }
    }
    if (mode == IO_NORMAL || mode == IO_COLLISION)
        return __real_renameat2(olddir, oldpath, newdir, newpath, flags);
    errno = ENOSPC;
    return -1;
}

int __wrap_close(int fd)
{
    int status = __real_close(fd);
    io_mode mode = next_fault(&storage_close_fault);
    if (status != 0 || mode == IO_NORMAL)
        return status;
    /* Linux releases the descriptor even when close reports a late failure.
     * The implementation must not retry and close a subsequently reused fd. */
    errno = mode == IO_INTERRUPT ? EINTR : EIO;
    return -1;
}
