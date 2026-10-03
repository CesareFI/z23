/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

static bool fail_open, fail_cleanup;
static unsigned open_calls, cleanup_calls;

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
    ++open_calls;
    if (fail_open) { errno = EMFILE; return -1; }
    return __real_open(path, flags);
}

int __real_rmdir(const char *path);
int __wrap_rmdir(const char *path);
int __wrap_rmdir(const char *path)
{
    ++cleanup_calls;
    if (fail_cleanup) { errno = EACCES; return -1; }
    return __real_rmdir(path);
}

static int failed_descriptor_retires_directory(bool cleanup_failure)
{
    storage_fixture fixture;
    fail_open = true;
    fail_cleanup = cleanup_failure;
    open_calls = cleanup_calls = 0;
    const int status = fixture_open(&fixture);
    const int cause = errno;
    fail_open = fail_cleanup = false;
    const unsigned cleanups = cleanup_calls;
    struct stat info = {0};
    const int found = lstat(fixture.path, &info);
    const int lookup_error = errno;
    /* Also clean the RED candidate's invocation-owned directory before failing
     * its assertion. Never leave expected regression failures as temp debris. */
    if (found == 0) CHECK(rmdir(fixture.path) == 0);
    CHECK(status == 1 && open_calls == 1 && fixture.directory == -1);
    CHECK(cause == EMFILE && cleanups == 1);
    if (cleanup_failure) CHECK(found == 0 && S_ISDIR(info.st_mode));
    else CHECK(found == -1 && lookup_error == ENOENT);
    return 0;
}

int main(void)
{
    storage_fixture control;
    CHECK(fixture_open(&control) == 0);
    const int failed = failed_descriptor_retires_directory(false)
        || failed_descriptor_retires_directory(true);
    struct stat info = {0};
    const int intact = fstat(control.directory, &info);
    const int closed = fixture_close(&control);
    CHECK(intact == 0 && S_ISDIR(info.st_mode) && closed == 0);
    CHECK(failed == 0);
    puts("Storage fixture descriptor failure retires only its own directory");
    return 0;
}
