/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 *
 * test_sqlq — regressions for the sqlq read-only sqlite query CLI
 * (tools/sqlq.c, built as build/bin/sqlq by `make sqlq`).
 *
 * Python and the sqlite3 CLI are both absent on a stock host; sqlq is how
 * an operator or script inspects the node's sqlite stores (the kernel
 * store's stage cursors, node.db tables, copied fixture datadirs). These
 * cases pin the exact output contract — rows tab-separated, NULL as the
 * literal "NULL", BLOBs as lowercase hex — and the read-only refusal:
 * anything that is not SELECT/PRAGMA, an unreadable path, or a SQL error
 * exits 1 and never writes. */

#include "test/test_core.h"
#include "util/spawn.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define SQLQ_BIN "build/bin/sqlq"

static int sqlq_run(const char *db_path, const char *sql,
                    char *buf, size_t cap)
{
    const char *argv[] = {SQLQ_BIN, db_path, sql, NULL};
    return zcl_spawn_capture(argv, buf, cap, 5000);
}

/* One small fixture database: an integer, a text, a blob, and a NULL,
 * plus a second row, so column typing and row ordering are both pinned. */
static bool sqlq_fixture(const char *dir, char *db_path, size_t db_path_cap)
{
    int n = snprintf(db_path, db_path_cap, "%s/fixture.db", dir);
    if (n <= 0 || (size_t)n >= db_path_cap) return false;
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    const char *ddl =
        "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, payload BLOB, "
        "note TEXT);"
        "INSERT INTO t VALUES(1, 'alpha', x'00ff10', NULL);"
        "INSERT INTO t VALUES(2, 'bravo', x'7f', 'done');";
    bool ok = sqlite3_exec(db, ddl, NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_close(db);
    return ok;
}

static int test_sqlq_rows_and_typing(void)
{
    int failures = 0;
    char dir[4096], db[4608], buf[1024] = {0};
    test_make_tmpdir(dir, sizeof(dir), "sqlq", "rows");
    TEST("sqlq: rows are tab-separated; NULL and BLOB keep their contract") {
        ASSERT(sqlq_fixture(dir, db, sizeof(db)));
        ASSERT(sqlq_run(db, "SELECT id, name, payload, note FROM t ORDER BY id",
                        buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "1\talpha\t00ff10\tNULL\n2\tbravo\t7f\tdone\n") ==
               0);
        /* Empty results are a clean exit with no output. */
        buf[0] = 0;
        ASSERT(sqlq_run(db, "SELECT id FROM t WHERE id > 99",
                        buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "") == 0);
        /* PRAGMA is the second permitted read-only statement. */
        ASSERT(sqlq_run(db, "PRAGMA user_version", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "0\n") == 0);
        PASS();
    } _test_next:;
    (void)test_rm_rf_recursive(dir);
    return failures;
}

static int test_sqlq_refusals(void)
{
    int failures = 0;
    char dir[4096], db[4608], buf[256] = {0};
    test_make_tmpdir(dir, sizeof(dir), "sqlq", "refusals");
    TEST("sqlq: writes, missing databases, and bad SQL all refuse with exit 1") {
        ASSERT(sqlq_fixture(dir, db, sizeof(db)));
        /* The read-only prefix guard refuses statements before sqlite sees
         * them; a SELECT-qualified mutation shape still cannot write. */
        ASSERT(sqlq_run(db, "DELETE FROM t", buf, sizeof(buf)) == 1);
        ASSERT(sqlq_run(db, "INSERT INTO t VALUES(3, 'x', x'00', 'y')",
                        buf, sizeof(buf)) == 1);
        /* Syntax errors refuse after prepare, not as a crash. */
        ASSERT(sqlq_run(db, "SELECT FROM WHERE", buf, sizeof(buf)) == 1);
        /* A missing database file is exit 1, never creation. */
        ASSERT(sqlq_run("build/bin/sqlq-no-such-database", "SELECT 1",
                        buf, sizeof(buf)) == 1);
        /* The fixture survived every refusal attempt. */
        ASSERT(sqlq_run(db, "SELECT COUNT(*) FROM t", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "2\n") == 0);
        PASS();
    } _test_next:;
    (void)test_rm_rf_recursive(dir);
    return failures;
}

int test_sqlq(void)
{
    int failures = 0;
    struct stat st;
    if (stat(SQLQ_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("sqlq: build/bin/sqlq missing (run: make sqlq) — skipped\n");
        return 0;
    }
    failures += test_sqlq_rows_and_typing();
    failures += test_sqlq_refusals();
    return failures;
}
