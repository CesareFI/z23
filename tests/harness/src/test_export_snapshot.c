/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 *
 * test_export_snapshot — regressions for the consensus snapshot exporter
 * (tools/export_snapshot.c -> build/bin/export_snapshot via `make
 * export_snapshot`).
 *
 * The exporter copies only the public consensus tables out of node.db
 * into consensus_snapshot.db. These cases pin its contract:
 *   - a normal datadir exports all seven public tables with their rows;
 *   - a datadir path containing an apostrophe does not break the ATTACH
 *     statement (the path is SQL-quoted);
 *   - a read-only node.db still exports (the source attaches mode=ro);
 *   - a datadir without node.db refuses with exit 1. */

#include "test/test_core.h"
#include "util/spawn.h"

#include "platform/directory_compat.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define EXPORT_SNAPSHOT_BIN "build/bin/export_snapshot"

static int snapshot_run(const char *datadir, char *buf, size_t cap)
{
    char cmd[4608];
    int n = snprintf(cmd, sizeof(cmd), "%s \"%s\"", EXPORT_SNAPSHOT_BIN,
                     datadir);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) return -1;
    const char *argv[] = {"sh", "-c", cmd, NULL};
    return zcl_spawn_capture(argv, buf, cap, 30000);
}

/* Create a node.db holding the seven public tables, one row each. */
static bool fixture_node_db(const char *datadir)
{
    char path[4608];
    int n = snprintf(path, sizeof(path), "%s/node.db", datadir);
    if (n <= 0 || (size_t)n >= sizeof(path)) return false;
    static const char *const tables[] = {
        "blocks", "transactions", "utxos", "addresses",
        "chain_stats", "zslp_tokens", "zslp_balances", NULL};
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    bool ok = true;
    for (size_t i = 0; ok && tables[i]; i++) {
        char sql[128];
        (void)snprintf(sql, sizeof(sql),
                       "CREATE TABLE %s(x); INSERT INTO %s VALUES(1);",
                       tables[i], tables[i]);
        ok = sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
    }
    sqlite3_close(db);
    return ok;
}

/* Verify the snapshot beside node.db holds every public table with its row. */
static bool snapshot_has_tables(const char *datadir)
{
    char path[4608];
    int n = snprintf(path, sizeof(path), "%s/consensus_snapshot.db", datadir);
    if (n <= 0 || (size_t)n >= sizeof(path)) return false;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    static const char *const tables[] = {
        "blocks", "transactions", "utxos", "addresses",
        "chain_stats", "zslp_tokens", "zslp_balances", NULL};
    bool ok = true;
    for (size_t i = 0; ok && tables[i]; i++) {
        char sql[128];
        (void)snprintf(sql, sizeof(sql), "SELECT count(*) FROM %s",
                       tables[i]);
        sqlite3_stmt *st = NULL;
        ok = sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
             sqlite3_step(st) == SQLITE_ROW &&
             sqlite3_column_int(st, 0) == 1;
        if (st) sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

static int test_export_snapshot_paths(void)
{
    int failures = 0;
    char dir[4096], quoted[4608], buf[4096] = {0};
    test_make_tmpdir(dir, sizeof(dir), "export_snapshot", "run");
    TEST("export_snapshot: normal, quoted, and read-only datadirs export") {
        ASSERT(fixture_node_db(dir));
        ASSERT(snapshot_run(dir, buf, sizeof(buf)) == 0);
        ASSERT(strstr(buf, "Exported 7 tables") != NULL);
        ASSERT(snapshot_has_tables(dir));

        /* An apostrophe in the datadir path must not break the ATTACH. */
        int n = snprintf(quoted, sizeof(quoted), "%s/it's-here", dir);
        ASSERT(n > 0 && (size_t)n < sizeof(quoted));
        ASSERT(platform_directory_ensure(quoted, 0700));
        ASSERT(fixture_node_db(quoted));
        buf[0] = 0;
        ASSERT(snapshot_run(quoted, buf, sizeof(buf)) == 0);
        ASSERT(strstr(buf, "Exported 7 tables") != NULL);
        ASSERT(snapshot_has_tables(quoted));

        /* URI-reserved characters (? # %) must survive as literal path
         * bytes, not be read as URI syntax. */
        n = snprintf(quoted, sizeof(quoted), "%s/uri?#%%chars", dir);
        ASSERT(n > 0 && (size_t)n < sizeof(quoted));
        ASSERT(platform_directory_ensure(quoted, 0700));
        ASSERT(fixture_node_db(quoted));
        buf[0] = 0;
        ASSERT(snapshot_run(quoted, buf, sizeof(buf)) == 0);
        ASSERT(strstr(buf, "Exported 7 tables") != NULL);
        ASSERT(snapshot_has_tables(quoted));

        /* A read-only source still exports: the ATTACH is mode=ro. */
        char db_path[4608];
        (void)snprintf(db_path, sizeof(db_path), "%s/node.db", dir);
        ASSERT(chmod(db_path, 0444) == 0);
        char snap_path[4608];
        (void)snprintf(snap_path, sizeof(snap_path),
                       "%s/consensus_snapshot.db", dir);
        ASSERT(unlink(snap_path) == 0);
        buf[0] = 0;
        ASSERT(snapshot_run(dir, buf, sizeof(buf)) == 0);
        ASSERT(snapshot_has_tables(dir));
        PASS();
    } _test_next:;
    (void)test_rm_rf_recursive(dir);
    return failures;
}

static int test_export_snapshot_refusals(void)
{
    int failures = 0;
    char dir[4096], buf[1024] = {0};
    test_make_tmpdir(dir, sizeof(dir), "export_snapshot", "refuse");
    TEST("export_snapshot: a datadir without node.db refuses with exit 1") {
        ASSERT(snapshot_run(dir, buf, sizeof(buf)) == 1);
        ASSERT(strstr(buf, "Exported") == NULL);
        PASS();
    } _test_next:;
    (void)test_rm_rf_recursive(dir);
    return failures;
}

int test_export_snapshot(void)
{
    int failures = 0;
    struct stat st;
    if (stat(EXPORT_SNAPSHOT_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("export_snapshot: build/bin/export_snapshot missing "
               "(run: make export_snapshot) — skipped\n");
        return 0;
    }
    failures += test_export_snapshot_paths();
    failures += test_export_snapshot_refusals();
    return failures;
}
