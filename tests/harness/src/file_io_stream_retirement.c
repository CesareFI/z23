/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Purpose: prove whole-file readers acquire a no-inherit stream and retire a
 * partially filled owned allocation before returning a short-read failure. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform/file_stream.h"
#include "test/test_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>

static unsigned file_io_observed_opens;
static bool file_io_observed_cloexec;
static bool file_io_inject_short_read;
static size_t file_io_short_read_bytes;
static size_t file_io_expected_allocation;
static bool file_io_freed_allocation_zero;

static FILE *file_io_observe_open(const char *path)
{
    FILE *stream = platform_file_stream_open_read(path);
    ++file_io_observed_opens;
    int fd = stream ? fileno(stream) : -1;
    file_io_observed_cloexec = file_io_observed_cloexec && fd >= 0 &&
        (fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0;
    return stream;
}

static size_t file_io_observe_fread(void *ptr, size_t size, size_t count,
                                    FILE *stream)
{
    if (!file_io_inject_short_read)
        return fread(ptr, size, count, stream);
    (void)stream;
    if (size != 1 || count == 0)
        return 0;
    file_io_short_read_bytes = count - 1;
    memset(ptr, 0xa5, file_io_short_read_bytes);
    return file_io_short_read_bytes;
}

static void file_io_observe_free(void *ptr)
{
    if (file_io_inject_short_read && ptr) {
        const uint8_t *bytes = ptr;
        file_io_freed_allocation_zero = true;
        for (size_t i = 0; i < file_io_expected_allocation; i++)
            file_io_freed_allocation_zero =
                file_io_freed_allocation_zero && bytes[i] == 0;
    }
    free(ptr);
}

static void file_io_short_read_reset(size_t allocation_len)
{
    file_io_inject_short_read = true;
    file_io_short_read_bytes = 0;
    file_io_expected_allocation = allocation_len;
    file_io_freed_allocation_zero = false;
}

#define platform_file_stream_open_read file_io_observe_open
#define fread file_io_observe_fread
#define free file_io_observe_free
#define zcl_read_whole_file file_io_observed_read
#define zcl_read_whole_file_text file_io_observed_read_text
#include "../../../platform/modules/util/src/file_io.c"
#undef zcl_read_whole_file_text
#undef zcl_read_whole_file
#undef free
#undef fread
#undef platform_file_stream_open_read
#endif

#ifndef _WIN32
static bool file_io_regular_reads_ok(const char *path, bool prepared)
{
    file_io_observed_opens = 0;
    file_io_observed_cloexec = true;
    uint8_t *bytes = NULL;
    size_t length = 0;
    bool read_ok = prepared && file_io_observed_read(
        path, 4, &bytes, &length, "file_io_stream_test");
    bool binary_ok = read_ok && length == 4 &&
        memcmp(bytes, "read", 4) == 0;
    free(bytes);

    char *text = NULL;
    length = 0;
    bool text_ok = prepared && file_io_observed_read_text(
        path, 4, &text, &length, "file_io_stream_test") && length == 4 &&
        strcmp(text, "read") == 0;
    free(text);
    return binary_ok && text_ok && file_io_observed_opens == 2 &&
        file_io_observed_cloexec;
}

static bool file_io_binary_short_read_retired(const char *path)
{
    file_io_short_read_reset(4);
    uint8_t *bytes = NULL;
    size_t length = 0;
    bool okay = !file_io_observed_read(
        path, 4, &bytes, &length, "file_io_stream_test") && !bytes &&
        length == 0 && file_io_short_read_bytes == 3 &&
        file_io_freed_allocation_zero;
    file_io_inject_short_read = false;
    return okay;
}

static bool file_io_text_short_read_retired(const char *path)
{
    file_io_short_read_reset(5);
    char *text = NULL;
    size_t length = 0;
    bool okay = !file_io_observed_read_text(
        path, 4, &text, &length, "file_io_stream_test") && !text &&
        length == 0 && file_io_short_read_bytes == 3 &&
        file_io_freed_allocation_zero;
    file_io_inject_short_read = false;
    return okay;
}
#endif

int file_io_stream_retirement_cases(void);
int file_io_stream_retirement_cases(void)
{
#ifdef _WIN32
    puts("path_check: whole-file stream refuses inheritance... UNOBSERVED (Windows runtime required)");
    return 0;
#else
    char path[PATH_MAX];
    int fd = test_mkstemp(path, sizeof(path), "file_io_stream");
    bool prepared = false;
    if (fd >= 0) {
        bool wrote = write(fd, "read", 4) == 4;
        bool closed = close(fd) == 0;
        prepared = wrote && closed;
    }
    bool stream_ok = file_io_regular_reads_ok(path, prepared);

    bool binary_short = file_io_binary_short_read_retired(path);
    bool text_short = file_io_text_short_read_retired(path);

    bool retirement_ok = binary_short && text_short;
    bool okay = stream_ok && retirement_ok;
    if (fd >= 0)
        (void)unlink(path);
    printf("path_check: whole-file stream refuses inheritance... %s\n",
        stream_ok ? "OK" : "FAIL");
    printf("path_check: short whole-file reads retire owned buffers... %s\n",
        retirement_ok ? "OK" : "FAIL");
    return okay ? 0 : 1;
#endif
}
