/* Copyright 2026 Rhett Creighton - Apache License 2.0 */

#include "test/test_core.h"
#include "net/file_service.h"
#include "net/file_manifest.h"
#include "platform/time_compat.h"
#include "util/log_macros.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

enum resume_damage {
    RESUME_FRESH,
    RESUME_INTACT,
    RESUME_CORRUPT,
    RESUME_HOLE,
    RESUME_TRUNCATED,
    RESUME_MISSING,
};

static bool resume_write_chunk(const char *dir, int index)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/blocks/blk%05d.dat", dir, index);
    unsigned char bytes[4096];
    memset(bytes, 0x31 + index, sizeof(bytes));
    FILE *f = fopen(path, "wb");
    if (!f)
        LOG_FAIL("test_file_service_resume", "create fixture chunk %d", index);
    bool ok = fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes);
    ok = fclose(f) == 0 && ok;
    /* The real server advertises only files stable for at least an hour. */
    struct utimbuf times = { .actime = 1, .modtime = 1 };
    if (!ok || utime(path, &times) != 0)
        LOG_FAIL("test_file_service_resume", "persist fixture chunk %d", index);
    return true;
}

static bool resume_make_files(const char *dir, bool populate)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/blocks", dir);
    if (mkdir(path, 0700) != 0)
        LOG_FAIL("test_file_service_resume", "create fixture blocks directory");
    for (int i = 0; populate && i < 3; i++)
        if (!resume_write_chunk(dir, i))
            return false;
    return true;
}

static bool resume_damage_chunk(const char *dir, enum resume_damage damage)
{
    if (damage == RESUME_FRESH || damage == RESUME_INTACT)
        return true;
    char path[512];
    snprintf(path, sizeof(path), "%s/blocks/blk00001.dat", dir);
    if (damage == RESUME_MISSING) {
        if (unlink(path) != 0)
            LOG_FAIL("test_file_service_resume", "remove fixture chunk");
        return true;
    }
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        LOG_FAIL("test_file_service_resume", "open damaged fixture chunk");
    bool ok;
    if (damage == RESUME_CORRUPT) {
        unsigned char bad = 0xff;
        ok = pwrite(fd, &bad, 1, 2048) == 1;
    } else if (damage == RESUME_HOLE) {
        /* A later positioned write can extend a file past an unwritten
         * chunk after interruption. Preserve length with sparse zero bytes. */
        ok = ftruncate(fd, 0) == 0 && ftruncate(fd, 4096) == 0;
    } else {
        ok = ftruncate(fd, 2048) == 0;
    }
    ok = close(fd) == 0 && ok;
    if (!ok)
        LOG_FAIL("test_file_service_resume", "damage fixture chunk");
    return true;
}

static bool resume_verify_files(const char *dir,
                                const struct file_manifest *manifest)
{
    for (uint32_t i = 0; i < manifest->num_chunks; i++) {
        uint8_t *data = NULL;
        uint32_t size = 0;
        bool ok = file_chunk_read(&manifest->chunks[i], dir, &data, &size);
        free(data);
        if (!ok || size != manifest->chunks[i].size)
            LOG_FAIL("test_file_service_resume",
                     "successful sync left chunk %u unreadable or corrupt", i);
    }
    return true;
}

static bool resume_run_case(uint16_t port,
                            const struct file_manifest *manifest,
                            enum resume_damage damage)
{
    char dir[256];
    if (!test_mkdtemp(dir, sizeof(dir), "zcl_fs_resume_client"))
        LOG_FAIL("test_file_service_resume", "create fresh receiver datadir");
    bool ok = resume_make_files(dir, damage != RESUME_FRESH) &&
              resume_damage_chunk(dir, damage);
    uint8_t root[32] = {0};
    if (ok)
        ok = fs_client_sync("127.0.0.1", port, dir, root);
    if (ok)
        ok = resume_verify_files(dir, manifest);
    test_rm_rf_recursive(dir);
    return ok;
}

int test_file_service_resume(void)
{
    int failures = 0;
    char server[256] = {0};
    fs_server_stop();
    test_reset_shared_globals();
    TEST("file-service successful fresh/resumed downloads verify every chunk") {
        ASSERT(test_mkdtemp(server, sizeof(server), "zcl_fs_resume_server"));
        ASSERT(resume_make_files(server, true));
        struct file_manifest manifest = {0};
        ASSERT(file_manifest_build(&manifest, server));
        ASSERT(manifest.num_chunks == 3);
        fs_server_start(server, 0);
        for (int i = 0; i < 40 && !fs_server_is_running(); i++)
            platform_sleep_ms(50);
        uint16_t port = fs_server_get_port();
        ASSERT(port != 0);
        ASSERT(fs_server_refresh_manifest());
        static const char *names[] = {
            "fresh", "intact", "corrupt unchecked chunk", "sparse hole",
            "truncated chunk", "missing chunk",
        };
        for (int i = RESUME_FRESH; i <= RESUME_MISSING; i++) {
            bool ok = resume_run_case(port, &manifest, (enum resume_damage)i);
            printf("file_service_resume: %s: %s\n", names[i], ok ? "PASS" : "FAIL");
            if (!ok)
                failures++;
        }
        ASSERT(failures == 0);
        PASS();
    } _test_next:;
    fs_server_stop();
    if (server[0])
        test_rm_rf_recursive(server);
    return failures;
}
