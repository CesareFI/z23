/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Rhett Creighton
 *
 * Tests for the db_maintenance storage seam.
 *
 * Exercises the sqlite adapter behind db_maintenance_port (wal_checkpoint
 * TRUNCATE / ANALYZE / VACUUM plus a WAL-size probe) against isolated
 * temp-file / in-memory DBs, never the live node DB.
 *
 * Asserts each op succeeds on a WAL-mode DB, TRUNCATE shrinks the WAL to
 * zero, success clears the error buffer while NULL-conn ops fill it and
 * return false, and wal_size_bytes is false for :memory: and true once a
 * file-backed WAL exists. NULL-arg guards round it out.
 *
 * test_db_maintenance.c covers the full db_maintenance_run_now() service
 * separately. This file is hermetic.
 */

#include "test/test_core.h"

#include "adapters/outbound/persistence/db_maintenance_sqlite.h"
#include "ports/db_maintenance_port.h"
#include "util/wal_checkpoint_stats.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DBMP_CHECK(name, expr) do {                       \
    printf("db_maintenance_port: %s... ", (name));        \
    if ((expr)) { printf("OK\n"); }                       \
    else { printf("FAIL\n"); failures++; }                \
} while (0)

static bool exec_sql(sqlite3 *db, const char *sql)
{
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

/* Open a fresh file-backed WAL-mode DB with a little churn so ANALYZE /
 * VACUUM / checkpoint have real work to do. Returns the path so the
 * caller can clean up the sidecar files. */
static bool make_file_db(sqlite3 **out_db, char *path, size_t pathsz)
{
    snprintf(path, pathsz, "/tmp/zcl_dbmp_test_%d_XXXXXX", (int)getpid());
    int fd = mkstemp(path);
    if (fd >= 0) close(fd);
    unlink(path);   /* let sqlite create it fresh */

    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK || !db)
        return false;
    if (!exec_sql(db, "PRAGMA journal_mode=WAL;")) { sqlite3_close(db); return false; }
    if (!exec_sql(db,
            "CREATE TABLE kv(k INTEGER PRIMARY KEY, v BLOB);"
            "INSERT INTO kv VALUES(1, randomblob(64));"
            "INSERT INTO kv VALUES(2, randomblob(128));"
            "INSERT INTO kv VALUES(3, randomblob(256));"
            "DELETE FROM kv WHERE k=2;")) {
        sqlite3_close(db);
        return false;
    }
    *out_db = db;
    return true;
}

static void clean_file_db(sqlite3 *db, const char *path)
{
    if (db) sqlite3_close(db);
    char side[1100];
    unlink(path);
    snprintf(side, sizeof side, "%s-wal", path); unlink(side);
    snprintf(side, sizeof side, "%s-shm", path); unlink(side);
}

static int64_t wal_file_size(const char *db_path)
{
    char wal[1100];
    struct stat st;
    snprintf(wal, sizeof wal, "%s-wal", db_path);
    if (stat(wal, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

/* ── wal_ckpt_classify ─────────────────────────────────────────────────
 *
 * The outcome ranking is the invariant: busy beats completed (SQLite
 * reports contention as a successful call whose row says busy), a failed
 * call is an error, unknown frame counts refuse to claim success, and
 * only then do drained/noop/partial split on the counts. The 0/0 empty
 * WAL is a drain, not a no-op. Pure — called directly. One function per
 * TEST, as the harness's hardcoded `goto _test_next` requires. */
static int t_wal_ckpt_classify_ranking(void)
{
    int failures = 0;
    TEST("wal_ckpt_classify: busy beats completed, error otherwise") {
        ASSERT_EQ(wal_ckpt_classify(true, true, 0, 0), WAL_CKPT_BUSY);
        ASSERT_EQ(wal_ckpt_classify(false, true, -1, -1), WAL_CKPT_BUSY);
        ASSERT_EQ(wal_ckpt_classify(false, false, 5, 0), WAL_CKPT_ERROR);
        PASS();
    } _test_next:;
    return failures;
}

static int t_wal_ckpt_classify_unknown(void)
{
    int failures = 0;
    TEST("wal_ckpt_classify: unknown frame counts classify UNKNOWN, not success") {
        ASSERT_EQ(wal_ckpt_classify(true, false, -1, 0), WAL_CKPT_UNKNOWN);
        ASSERT_EQ(wal_ckpt_classify(true, false, 0, -1), WAL_CKPT_UNKNOWN);
        ASSERT_EQ(wal_ckpt_classify(true, false, -1, -1), WAL_CKPT_UNKNOWN);
        PASS();
    } _test_next:;
    return failures;
}

static int t_wal_ckpt_classify_counts(void)
{
    int failures = 0;
    TEST("wal_ckpt_classify: drained/noop/partial split on exact counts") {
        ASSERT_EQ(wal_ckpt_classify(true, false, 0, 0), WAL_CKPT_DRAINED);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 5), WAL_CKPT_DRAINED);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 7), WAL_CKPT_DRAINED);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 0), WAL_CKPT_NOOP);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 3), WAL_CKPT_PARTIAL);
        PASS();
    } _test_next:;
    return failures;
}

/* Only the main file is forwarded. Native VFS journal/WAL creation can
 * use fixed pathname buffers even when the wrapper advertises a larger
 * mxPathname. Seed the size witnesses separately; do not enter those paths. */
static sqlite3_vfs *long_path_base;

static int long_path_open(sqlite3_vfs *vfs, const char *name, sqlite3_file *file,
                          int flags, int *out_flags)
{
    (void)vfs;
    if (!(flags & SQLITE_OPEN_MAIN_DB) || !(flags & SQLITE_OPEN_READONLY))
        return SQLITE_CANTOPEN;
    return long_path_base->xOpen(long_path_base, name, file, flags, out_flags);
}

static int long_path_full(sqlite3_vfs *vfs, const char *name, int size, char *out)
{
    (void)vfs;
    size_t len = strlen(name);
    if (name[0] != '/' || size <= 0 || len >= (size_t)size)
        return SQLITE_CANTOPEN;
    memcpy(out, name, len + 1);
    return SQLITE_OK;
}

static bool long_path_seed(const char *path, const char *suffix)
{
    char name[1100];
    int n = snprintf(name, sizeof name, "%s%s", path, suffix);
    if (n < 0 || (size_t)n >= sizeof name)
        return false;
    FILE *file = fopen(name, "wb");
    if (!file)
        return false;
    bool wrote = fwrite("size witness", 1, 12, file) == 12;
    return fclose(file) == 0 && wrote;
}

static int t_wal_path_probe(sqlite3 *db, const char *path, size_t length)
{
    int failures = 0;
    const char *actual = sqlite3_db_filename(db, "main");
    DBMP_CHECK("exact returned filename", actual &&
               strlen(actual) == length && strcmp(actual, path) == 0);
    struct db_maintenance_sqlite_ctx ctx;
    struct db_maintenance_port port = {0};
    DBMP_CHECK("long-path bind", db_maintenance_sqlite_bind(&ctx, db, &port));
    int64_t bytes = 777;
    bool got = port.wal_size_bytes(port.self, &bytes);
    if (length == 1019) {
        DBMP_CHECK("fitting WAL exact size", got && bytes == 12);
    } else {
        DBMP_CHECK("oversized WAL path refuses", !got);
        DBMP_CHECK("refusal preserves sentinel", bytes == 777);
    }
    return failures;
}

static int t_wal_path_case(const char *dir, size_t length, const char *vfs_name)
{
    int failures = 0;
    char path[1100], truncated[1024];
    size_t len = strlen(dir);
    bool bounded = len < length && length <= 1023 && length - len - 1 <= 250;
    DBMP_CHECK("bounded long-path components", bounded);
    if (!bounded) return failures;
    memcpy(path, dir, len);
    path[len] = '/';
    memset(path + len + 1, 'p', length - len - 1);
    path[length] = 0;
    /* Nonempty real files make the mutant return true at every refusal
     * boundary, including 1020 where truncation leaves the suffix '-wa'.
     * sqlite3_open_v2 does not read the schema; no SQL is run on witnesses. */
    DBMP_CHECK("main size witness created", long_path_seed(path, ""));
    DBMP_CHECK("WAL size witness created", long_path_seed(path, "-wal"));
    memcpy(truncated, path, length);
    size_t suffix = 1023 - length;
    memcpy(truncated + length, "-wal", suffix);
    truncated[1023] = 0;
    if (length != 1019)
        DBMP_CHECK("truncated-path witness created", long_path_seed(truncated, ""));
    sqlite3 *db = NULL;
    bool opened = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, vfs_name) == SQLITE_OK;
    DBMP_CHECK("long-path DB opens", opened);
    if (opened) failures += t_wal_path_probe(db, path, length);
    DBMP_CHECK("long-path DB closes", !db || sqlite3_close(db) == SQLITE_OK);
    clean_file_db(NULL, path);
    if (length != 1019 && length != 1023)
        DBMP_CHECK("truncated-path witness removed", unlink(truncated) == 0);
    DBMP_CHECK("long-path files removed", access(path, F_OK) != 0 && wal_file_size(path) < 0);
    return failures;
}

static bool long_path_directory(char *dir, char *root, size_t capacity)
{
    if (!test_mkdtemp(dir, capacity, "dbmp_bounds")) return false;
    memcpy(root, dir, strlen(dir) + 1);
    size_t len = strlen(dir);
    if (len > 850) return false;
    while (len < 848) {
        size_t part = 850 - len - 1;
        if (part > 180) part = 180;
        dir[len++] = '/';
        memset(dir + len, 'd', part);
        len += part;
        dir[len] = 0;
        if (mkdir(dir, 0700) != 0) {
            *strrchr(dir, '/') = 0;
            return false;
        }
    }
    return true;
}

static int long_path_cleanup(char *dir, const char *root)
{
    int failures = 0;
    while (strlen(dir) > strlen(root)) {
        DBMP_CHECK("fixture directory removed", rmdir(dir) == 0);
        *strrchr(dir, '/') = 0;
    }
    DBMP_CHECK("fixture root removed", rmdir(root) == 0);
    return failures;
}

static int t_wal_path_bounds(void)
{
    int failures = 0;
    char dir[1100] = "", root[1100] = "";
    bool ready = long_path_directory(dir, root, sizeof dir);
    DBMP_CHECK("long-path fixture ready", ready);
    if (!root[0]) return failures;
    long_path_base = sqlite3_vfs_find(NULL);
    sqlite3_vfs vfs = {0};
    bool registered = false;
    if (ready && long_path_base) {
        vfs = *long_path_base;
        vfs.pNext = NULL;
        vfs.zName = "dbmp-long-path";
        vfs.mxPathname = 4096;
        vfs.xOpen = long_path_open;
        vfs.xFullPathname = long_path_full;
        registered = sqlite3_vfs_register(&vfs, 0) == SQLITE_OK;
    }
    DBMP_CHECK("long-path VFS registered", registered);
    const size_t lengths[] = {1019, 1020, 1023};
    for (size_t i = 0; registered && i < 3; ++i)
        failures += t_wal_path_case(dir, lengths[i], vfs.zName);
    if (registered)
        DBMP_CHECK("fixture VFS removed", sqlite3_vfs_unregister(&vfs) == SQLITE_OK);
    failures += long_path_cleanup(dir, root);
    return failures;
}

int test_db_maintenance_port(void)
{
    int failures = 0;

    /* ---- each op succeeds on a real WAL-mode file DB ---- */
    {
        char path[256];
        sqlite3 *db = NULL;
        DBMP_CHECK("file db builds", make_file_db(&db, path, sizeof path));

        struct db_maintenance_sqlite_ctx ctx;
        struct db_maintenance_port port = {0};
        DBMP_CHECK("bind ok",
                   db_maintenance_sqlite_bind(&ctx, db, &port));

        char err[256] = "sentinel";
        struct db_maintenance_wal_outcome wal = {0};
        DBMP_CHECK("wal_checkpoint ok",
                   port.wal_checkpoint(port.self, &wal, err, sizeof err));
        DBMP_CHECK("wal_checkpoint clears err on success", err[0] == 0);

        /* The TRUNCATE checkpoint flushes WAL frames into the main file
         * and truncates the WAL back to zero bytes. */
        DBMP_CHECK("wal_checkpoint truncated WAL to 0",
                   wal_file_size(path) == 0);

        /* The counts are the point: a checkpoint that drained a non-empty WAL
         * must be DISTINGUISHABLE from one that moved nothing. Both report
         * success, so only these numbers separate them. */
        DBMP_CHECK("wal_checkpoint reports frames moved",
                   wal.ckpt_frames > 0);
        DBMP_CHECK("wal_checkpoint drained the whole log",
                   wal.ckpt_frames >= wal.log_frames);
        DBMP_CHECK("wal_checkpoint was not busy", !wal.busy);
        DBMP_CHECK("wal_checkpoint records SQLITE_OK", wal.rc == SQLITE_OK);
        DBMP_CHECK("wal_checkpoint records file reset",
                   wal.truncate_rc == SQLITE_OK && wal.truncated);
        DBMP_CHECK("a drain classifies as drained",
                   wal_ckpt_classify(true, wal.busy, wal.log_frames,
                                     wal.ckpt_frames) == WAL_CKPT_DRAINED);

        /* Immediately re-checkpointing an already-drained WAL moves zero
         * frames. That is a NO-OP, and it must not read as a drain of a WAL
         * that had something in it. */
        struct db_maintenance_wal_outcome again = {0};
        DBMP_CHECK("second checkpoint also succeeds",
                   port.wal_checkpoint(port.self, &again, err, sizeof err));
        DBMP_CHECK("second checkpoint moved nothing",
                   again.ckpt_frames == 0 && again.log_frames == 0);

        strcpy(err, "sentinel");
        DBMP_CHECK("analyze ok", port.analyze(port.self, err, sizeof err));
        DBMP_CHECK("analyze clears err", err[0] == '\0');

        strcpy(err, "sentinel");
        DBMP_CHECK("vacuum ok", port.vacuum(port.self, err, sizeof err));
        DBMP_CHECK("vacuum clears err", err[0] == '\0');

        clean_file_db(db, path);
    }

    /* ---- wal_size_bytes: file-backed DB -> true and >= 0 ---- */
    {
        char path[256];
        sqlite3 *db = NULL;
        DBMP_CHECK("file db builds (walsize)",
                   make_file_db(&db, path, sizeof path));

        struct db_maintenance_sqlite_ctx ctx;
        struct db_maintenance_port port = {0};
        db_maintenance_sqlite_bind(&ctx, db, &port);

        int64_t bytes = -1;
        bool got = port.wal_size_bytes(port.self, &bytes);
        DBMP_CHECK("wal_size file-backed ok", got);
        DBMP_CHECK("wal_size >= 0", !got || bytes >= 0);

        clean_file_db(db, path);
    }

    /* ---- wal_size_bytes: in-memory has no on-disk path -> false ---- */
    {
        sqlite3 *db = NULL;
        DBMP_CHECK("memdb opens",
                   sqlite3_open(":memory:", &db) == SQLITE_OK && db);
        struct db_maintenance_sqlite_ctx ctx;
        struct db_maintenance_port port = {0};
        db_maintenance_sqlite_bind(&ctx, db, &port);
        int64_t bytes = 12345;
        DBMP_CHECK("wal_size in-memory false",
                   !port.wal_size_bytes(port.self, &bytes));
        DBMP_CHECK("wal_size untouched on false", bytes == 12345);
        sqlite3_close(db);
    }

    /* ---- ops on an in-memory DB still succeed (no WAL file) ---- */
    {
        sqlite3 *db = NULL;
        DBMP_CHECK("memdb opens (ops)",
                   sqlite3_open(":memory:", &db) == SQLITE_OK && db);
        (void)exec_sql(db,
            "CREATE TABLE kv(k INTEGER PRIMARY KEY, v BLOB);"
            "INSERT INTO kv VALUES(1, randomblob(64));");
        struct db_maintenance_sqlite_ctx ctx;
        struct db_maintenance_port port = {0};
        db_maintenance_sqlite_bind(&ctx, db, &port);
        char err[256];
        /* A non-WAL connection has no log to reclaim; the engine reports the
         * frame counts as unknown rather than as zero, and the outcome is
         * UNKNOWN — never a claimed drain. */
        struct db_maintenance_wal_outcome wal = {0};
        DBMP_CHECK("memdb wal_checkpoint ok",
                   port.wal_checkpoint(port.self, &wal, err, sizeof err));
        DBMP_CHECK("memdb wal_checkpoint reports unknown counts",
                   wal.log_frames < 0 && wal.ckpt_frames < 0 && !wal.busy);
        DBMP_CHECK("unknown counts never classify as a drain",
                   wal_ckpt_classify(true, false, wal.log_frames,
                                     wal.ckpt_frames) == WAL_CKPT_UNKNOWN);
        DBMP_CHECK("memdb analyze ok",
                   port.analyze(port.self, err, sizeof err));
        DBMP_CHECK("memdb vacuum ok",
                   port.vacuum(port.self, err, sizeof err));
        sqlite3_close(db);
    }

    /* ---- NULL / bad-arg guards ---- */
    {
        struct db_maintenance_sqlite_ctx ctx;
        struct db_maintenance_port port = {0};

        DBMP_CHECK("bind rejects NULL ctx",
                   !db_maintenance_sqlite_bind(NULL, (sqlite3 *)0x1, &port));
        DBMP_CHECK("bind rejects NULL out_port",
                   !db_maintenance_sqlite_bind(&ctx, (sqlite3 *)0x1, NULL));

        /* NULL connection is legal at bind; ops then return false with a
         * filled error buffer and wal_size returns false untouched. */
        DBMP_CHECK("bind NULL conn ok",
                   db_maintenance_sqlite_bind(&ctx, NULL, &port));
        char err[256] = "";
        struct db_maintenance_wal_outcome wal = {0};
        DBMP_CHECK("wal_checkpoint NULL db false",
                   !port.wal_checkpoint(port.self, &wal, err, sizeof err));
        DBMP_CHECK("wal_checkpoint NULL db fills err", err[0] != 0);
        DBMP_CHECK("wal_checkpoint NULL db reports unknown counts",
                   wal.log_frames == -1 && wal.ckpt_frames == -1);
        DBMP_CHECK("wal_checkpoint NULL db records misuse",
                   wal.rc == SQLITE_MISUSE && wal.truncate_rc == -1 &&
                   !wal.truncated);
        DBMP_CHECK("analyze NULL db false",
                   !port.analyze(port.self, err, sizeof err));
        DBMP_CHECK("vacuum NULL db false",
                   !port.vacuum(port.self, err, sizeof err));
        int64_t bytes = 7;
        DBMP_CHECK("wal_size NULL db false",
                   !port.wal_size_bytes(port.self, &bytes));
        DBMP_CHECK("wal_size untouched on NULL db", bytes == 7);

        /* NULL self. */
        DBMP_CHECK("wal_checkpoint NULL self false",
                   !port.wal_checkpoint(NULL, &wal, err, sizeof err));
        DBMP_CHECK("wal_size NULL self false",
                   !port.wal_size_bytes(NULL, &bytes));

        /* NULL err buffer is tolerated (op runs; error path just can't
         * report text). NULL out for wal_size returns false. */
        struct db_maintenance_sqlite_ctx ctx2;
        struct db_maintenance_port port2 = {0};
        db_maintenance_sqlite_bind(&ctx2, NULL, &port2);
        DBMP_CHECK("wal_checkpoint NULL err tolerated (still false)",
                   !port2.wal_checkpoint(port2.self, NULL, NULL, 0));
        DBMP_CHECK("wal_size NULL out false",
                   !port2.wal_size_bytes(port2.self, NULL));
    }

    /* Root-body direct calls to the pure classifier (registered-root
     * reachability binds these to the exact definition). */
    TEST("wal_ckpt_classify: direct ranking against the exact definition") {
        ASSERT_EQ(wal_ckpt_classify(true, true, 3, 3), WAL_CKPT_BUSY);
        ASSERT_EQ(wal_ckpt_classify(false, false, 3, 3), WAL_CKPT_ERROR);
        ASSERT_EQ(wal_ckpt_classify(true, false, -1, -1), WAL_CKPT_UNKNOWN);
        ASSERT_EQ(wal_ckpt_classify(true, false, 0, 0), WAL_CKPT_DRAINED);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 0), WAL_CKPT_NOOP);
        ASSERT_EQ(wal_ckpt_classify(true, false, 5, 3), WAL_CKPT_PARTIAL);
        PASS();
    } _test_next:;

    failures += t_wal_path_bounds();
    failures += t_wal_ckpt_classify_ranking();
    failures += t_wal_ckpt_classify_unknown();
    failures += t_wal_ckpt_classify_counts();

    return failures;
}
