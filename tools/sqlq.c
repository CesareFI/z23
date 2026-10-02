/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * sqlq — read-only SQL query CLI over any sqlite database file.
 *
 * The operator/diagnosis story: python is banned in this repo and the
 * sqlite3 CLI is not installed on the host, so ad-hoc inspection of the
 * node's sqlite stores (the `consensus.db` kernel store's stage cursors —
 * `progress.kv` on a pre-flip datadir — node.db tables, or a copied fixture
 * datadir) from a shell needs a vendored-sqlite C tool (same precedent as
 * tools/p2_invariant_check.c). `core storage query` covers node.db but
 * cannot reach the kernel store or a copied fixture datadir.
 *
 *   build/bin/sqlq <db-path> <SELECT ...>
 *
 * Opens SQLITE_OPEN_READONLY (never creates, never writes, WAL-reader
 * safe against a live node). Prints rows tab-separated, NULL as "NULL",
 * BLOBs as lowercase hex. Exit 0 on success, 1 on usage/open/SQL error.
 */

#include "base/hex.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* SQLite prepares one statement at a time. Parse the remainder, but never
 * step it: comments and whitespace are fine; a second statement is not. */
static int sqlq_tail_is_empty(sqlite3 *db, const char *tail)
{
    sqlite3_stmt *extra = NULL;
    int rc = sqlite3_prepare_v2(db, tail, -1, &extra, NULL);
    if (rc != SQLITE_OK)
        fprintf(stderr, "sqlq: trailing SQL invalid: %s\n", sqlite3_errmsg(db));
    else if (extra)
        fprintf(stderr, "sqlq: trailing statement refused\n");
    int empty = rc == SQLITE_OK && extra == NULL;
    int finalize_rc = sqlite3_finalize(extra);
    if (finalize_rc != SQLITE_OK) {
        fprintf(stderr, "sqlq: trailing statement cleanup failed: %s\n",
                sqlite3_errstr(finalize_rc));
        empty = 0;
    }
    return empty;
}

/* Borrow SQLite's BLOB only until the next step/finalize. The fixed stack
 * buffer bounds formatting work regardless of the column length. */
static int sqlq_print_blob_hex(const unsigned char *blob, int len)
{
    char out[4097];
    if (len < 0 || (len > 0 && !blob)) {
        fprintf(stderr, "sqlq: BLOB column unavailable\n");
        return 0;
    }
    size_t remaining = (size_t)len;
    while (remaining) {
        size_t capacity = (sizeof(out) - 1) / 2;
        size_t count = remaining < capacity ? remaining : capacity;
        zcl_hex_encode(blob, count, out);
        if (fwrite(out, 1, 2 * count, stdout) != 2 * count) {
            fprintf(stderr, "sqlq: BLOB output failed\n");
            return 0;
        }
        blob += count;
        remaining -= count;
    }
    return 1;
}

static int sqlq_print_row(sqlite3_stmt *st)
{
    int n = sqlite3_column_count(st);
    for (int i = 0; i < n; i++) {
        if (i) fputc('\t', stdout);
        switch (sqlite3_column_type(st, i)) {
        case SQLITE_NULL:
            fputs("NULL", stdout);
            break;
        case SQLITE_BLOB: {
            const unsigned char *b = sqlite3_column_blob(st, i);
            int len = sqlite3_column_bytes(st, i);
            if (!sqlq_print_blob_hex(b, len)) return 0;
            break;
        }
        default:
            fputs((const char *)sqlite3_column_text(st, i), stdout);
        }
    }
    fputc('\n', stdout);
    return 1;
}

static int sqlq_print_rows(sqlite3 *db, sqlite3_stmt *st)
{
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) { // raw-sql-ok:read-only-diagnostic-cli
        if (!sqlq_print_row(st)) return 0;
    }
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "sqlq: step failed: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: sqlq <db-path> <SELECT ...>\n");
        return 1;
    }
    const char *sql = argv[2];
    if (strncasecmp(sql, "SELECT", 6) != 0 &&
        strncasecmp(sql, "PRAGMA", 6) != 0) {
        fprintf(stderr, "sqlq: read-only — SELECT/PRAGMA only\n");
        return 1;
    }

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READONLY, NULL) !=
        SQLITE_OK) {
        fprintf(stderr, "sqlq: open failed: %s\n",
                db ? sqlite3_errmsg(db) : "out of memory");
        sqlite3_close(db);
        return 1;
    }
    sqlite3_busy_timeout(db, 2000);

    sqlite3_stmt *st = NULL;
    const char *tail = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, &tail) != SQLITE_OK) {
        fprintf(stderr, "sqlq: prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    if (!sqlq_tail_is_empty(db, tail)) {
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 1;
    }

    int ok = sqlq_print_rows(db, st);
    sqlite3_finalize(st);
    sqlite3_close(db);
    return ok ? 0 : 1;
}
