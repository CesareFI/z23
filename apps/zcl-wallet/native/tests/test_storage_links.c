/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Real descriptor metadata except for a controlled record link-count field.
 * Directories and the empty lock retain their real metadata. No syscall fault
 * or filesystem operation is installed in a production library. */
static bool armed;
static nlink_t links;
static size_t observed;
int __real_fstat(int fd, struct stat *info);
int __wrap_fstat(int fd, struct stat *info);

int __wrap_fstat(int fd, struct stat *info)
{
    if (info == NULL) abort();
    const int status = __real_fstat(fd, info);
    if (status == 0 && armed && S_ISREG(info->st_mode) && info->st_size >= 124) {
        info->st_nlink = links;
        ++observed;
    }
    return status;
}

static int read_links(const storage_fixture *fixture, const uint8_t *record, size_t length,
    bool expected_pending, nlink_t count)
{
    uint8_t actual[142], before[142];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    size_t actual_len = SIZE_MAX;
    bool pending = !expected_pending;
    links = count; observed = 0; armed = true;
    const zcl_status status = fixture_read(fixture, actual + 1, 140, &actual_len, &pending);
    armed = false;
    CHECK(observed == 1);
    if (count != 1) {
        CHECK(status == ZCL_IO_FAILURE);
        CHECK(actual_len == SIZE_MAX && pending == !expected_pending);
        CHECK(memcmp(actual, before, sizeof(actual)) == 0);
    } else {
        CHECK(status == ZCL_OK && actual_len == length && pending == expected_pending);
        CHECK(memcmp(actual + 1, record, length) == 0);
        CHECK(actual[0] == 0xa5 && actual[141] == 0xa5);
    }
    return 0;
}

static int profile(const uint8_t *record, size_t length, bool pending, nlink_t count)
{
    storage_fixture fixture;
    CHECK(fixture_open(&fixture) == 0);
    CHECK(fixture_write(&fixture, pending ? ".wallet.pending" : "wallet.zcl", record, length) == 0);
    CHECK(read_links(&fixture, record, length, pending, count) == 0);
    links = count; observed = 0; armed = true;
    const zcl_status status = fixture_promote(&fixture, record, length);
    armed = false;
    CHECK(observed == 1);
    CHECK(status == (count == 1 ? ZCL_OK : ZCL_IO_FAILURE));
    // A metadata refusal leaves the real record readable and its name intact.
    CHECK(read_links(&fixture, record, length, pending && count != 1, 1) == 0);
    CHECK(fixture_promote(&fixture, record, length) == ZCL_OK);
    return fixture_close(&fixture);
}

int main(void)
{
    uint8_t record[140] = {0};
    size_t length = 0;
    CHECK(fixture_record(record, sizeof(record), &length) == 0);
    const nlink_t counts[] = {0, 1, 2, (nlink_t)-1};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        CHECK(profile(record, length, false, counts[i]) == 0);
        CHECK(profile(record, length, true, counts[i]) == 0);
    }
    puts("storage links: detached/aliased metadata, exact output preservation and normal retry passed");
    return 0;
}
