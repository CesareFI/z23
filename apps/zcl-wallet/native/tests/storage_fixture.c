/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "storage_fixture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __ANDROID__
static const char fixture_template[] = "/data/local/tmp/zcl-XXXXXX";
#else
static const char fixture_template[] = "/tmp/zcl-storage-test-XXXXXX";
#endif
_Static_assert(sizeof(fixture_template) <= sizeof(((storage_fixture *)0)->path), "fixture path fits");

size_t fixture_path_len(void) { return sizeof(fixture_template) - 1; }

int fixture_open(storage_fixture *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->directory = -1;
    memcpy(fixture->path, fixture_template, sizeof(fixture_template));
    CHECK(mkdtemp(fixture->path) != NULL);
    fixture->directory = open(fixture->path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fixture->directory < 0) {
        const int cause = errno;
        /* mkdtemp transferred this fresh empty directory before open failed.
         * No descriptor exists for fixture_close; retire only this path now. */
        if (rmdir(fixture->path) != 0) perror("storage fixture directory cleanup");
        fprintf(stderr, "storage fixture directory open failed: %d\n", cause);
        errno = cause;
        return 1;
    }
    return 0;
}

int fixture_close(storage_fixture *fixture)
{
    static const char *const names[] = {"wallet.zcl", ".wallet.pending", ".change.index", ".lock", "target", "child"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        int result = unlinkat(fixture->directory, names[i], 0);
        CHECK(result == 0 || errno == ENOENT);
    }
    CHECK(close(fixture->directory) == 0);
    fixture->directory = -1;
    CHECK(rmdir(fixture->path) == 0);
    return 0;
}

int fixture_record(uint8_t *record, size_t capacity, size_t *length)
{
    /* Published all-zero BIP39 entropy. Ciphertext is deliberately inert test
     * data; these storage tests make no GCM authentication claim. */
    uint8_t entropy[16] = {0}, blind[32] = {1}, header[80] = {0};
    uint8_t iv[12] = {0}, cipher[32] = {0};
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_TESTNET, blind, sizeof(blind),
                                   header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, sizeof(header), iv, sizeof(iv), cipher, sizeof(cipher),
                                 record, capacity, length) == ZCL_OK);
    return 0;
}

int fixture_write(const storage_fixture *fixture, const char *name, const uint8_t *bytes, size_t length)
{
    int fd = openat(fixture->directory, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    ssize_t count = write(fd, bytes, length);
    int closed = close(fd);
    CHECK(count >= 0 && (size_t)count == length && closed == 0);
    return 0;
}

zcl_status fixture_read(const storage_fixture *fixture, uint8_t *record, size_t capacity,
                        size_t *length, bool *pending)
{
    return zcl_storage_read((const uint8_t *)fixture->path, fixture_path_len(), record, capacity, length, pending);
}

zcl_status fixture_create(const storage_fixture *fixture, const uint8_t *record, size_t length)
{
    return zcl_storage_create((const uint8_t *)fixture->path, fixture_path_len(), record, length);
}

zcl_status fixture_promote(const storage_fixture *fixture, const uint8_t *record, size_t length)
{
    return zcl_storage_promote((const uint8_t *)fixture->path, fixture_path_len(), record, length);
}
