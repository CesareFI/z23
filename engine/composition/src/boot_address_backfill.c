/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Background explorer address-table backfill.
 *
 * Part of the boot composition root (engine/composition/src/), extracted from
 * boot_index.c. Owns exactly one thing: the one-shot background thread
 * that aggregates the explorer `addresses` table from the SQLite UTXO
 * set. It opens its own read/write sqlite handle, shares no file-scope
 * state with any other boot unit, and writes only the explorer
 * `addresses` table + the `addresses_backfilled` node_state marker — no
 * consensus state is touched. */

#include "platform/time_compat.h"
#include "config/boot_internal.h"
#include "util/log_macros.h"
#include "util/ar_step_readonly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sqlite3.h>

/* ── Background address backfill from UTXOs ────────────────── */

static bool address_backfill_exec(sqlite3 *db, const char *sql,
                                  const char *operation)
{
    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc == SQLITE_OK)
        return true;
    LOG_ERROR("store", "address backfill %s failed: rc=%d error=%s",
              operation, rc, sqlite3_errmsg(db));
    return false;
}

static bool address_backfill_finalize(sqlite3 *db, sqlite3_stmt **stmt,
                                      const char *operation)
{
    if (!stmt || !*stmt)
        return true;
    int rc = sqlite3_finalize(*stmt);
    *stmt = NULL;
    if (rc == SQLITE_OK)
        return true;
    LOG_ERROR("store", "address backfill %s finalize failed: rc=%d error=%s",
              operation, rc, sqlite3_errmsg(db));
    return false;
}

static void address_backfill_rollback(sqlite3 *db)
{
    if (!address_backfill_exec(db, "ROLLBACK", "rollback"))
        LOG_ERROR("store", "address backfill cleanup could not roll back");
}

static bool address_backfill_configure(sqlite3 *db)
{
    if (!address_backfill_exec(db, "PRAGMA mmap_size=0", "disable mmap"))
        return false;
    int rc = sqlite3_busy_timeout(db, 60000);
    if (rc != SQLITE_OK) {
        LOG_ERROR("store", "address backfill busy timeout failed: rc=%d error=%s",
                  rc, sqlite3_errmsg(db));
        return false;
    }
    if (!address_backfill_exec(db, "PRAGMA temp_store=FILE",
                               "configure temp store"))
        return false;
    return address_backfill_exec(db, "PRAGMA cache_size=-32768",
                                 "configure cache");
}

static bool address_backfill_schema(sqlite3 *db)
{
    if (!address_backfill_exec(db,
            "CREATE TABLE IF NOT EXISTS addresses ("
            "address_hash BLOB PRIMARY KEY,"
            "script_type INTEGER NOT NULL DEFAULT 0,"
            "balance INTEGER NOT NULL DEFAULT 0,"
            "utxo_count INTEGER NOT NULL DEFAULT 0,"
            "first_seen_height INTEGER NOT NULL DEFAULT 0,"
            "last_seen_height INTEGER NOT NULL DEFAULT 0"
            ")", "create addresses table"))
        return false;
    return address_backfill_exec(db,
        "CREATE INDEX IF NOT EXISTS idx_utxo_address"
        " ON utxos(address_hash) WHERE address_hash IS NOT NULL",
        "create UTXO address index");
}

static bool address_backfill_prepare(sqlite3 *db, sqlite3_stmt **cursor,
                                     sqlite3_stmt **upsert)
{
    int rc = sqlite3_prepare_v2(db,
        "SELECT DISTINCT address_hash FROM utxos "
        "WHERE address_hash IS NOT NULL "
        "ORDER BY address_hash", -1, cursor, NULL);
    if (rc != SQLITE_OK || !*cursor) {
        LOG_ERROR("store", "address backfill cursor prepare failed: rc=%d error=%s",
                  rc, sqlite3_errmsg(db));
        return false;
    }
    rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO addresses "
        "(address_hash, script_type, balance, utxo_count, "
        "first_seen_height, last_seen_height) "
        "SELECT address_hash, MAX(script_type), SUM(value), count(*), "
        "MIN(height), MAX(height) "
        "FROM utxos WHERE address_hash = ?1", -1, upsert, NULL);
    if (rc == SQLITE_OK && *upsert)
        return true;
    LOG_ERROR("store", "address backfill upsert prepare failed: rc=%d error=%s",
              rc, sqlite3_errmsg(db));
    return false;
}

static bool address_backfill_one(sqlite3 *db, sqlite3_stmt *upsert,
                                 const void *address, int address_len)
{
    int rc = sqlite3_reset(upsert);
    if (rc == SQLITE_OK)
        rc = sqlite3_clear_bindings(upsert);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_blob(upsert, 1, address, address_len, SQLITE_STATIC);
    if (rc == SQLITE_OK)
        rc = AR_STEP_WRITE(upsert);
    if (rc == SQLITE_DONE)
        return true;
    LOG_ERROR("store", "address backfill row write failed: rc=%d error=%s",
              rc, sqlite3_errmsg(db));
    return false;
}

static bool address_backfill_rows(sqlite3 *db, sqlite3_stmt *cursor,
                                  sqlite3_stmt *upsert, int64_t *processed,
                                  bool *transaction_open)
{
    static const int64_t batch_size = 10000;
    int rc;
    while ((rc = AR_STEP_ROW_READONLY(cursor)) == SQLITE_ROW) {
        const void *address = sqlite3_column_blob(cursor, 0);
        int address_len = sqlite3_column_bytes(cursor, 0);
        if (!address || address_len <= 0)
            continue;
        if (*processed == INT64_MAX) {
            LOG_ERROR("store", "address backfill row count overflow");
            return false;
        }
        if (!address_backfill_one(db, upsert, address, address_len))
            return false;
        (*processed)++;
        if (*processed % batch_size != 0)
            continue;
        if (!address_backfill_exec(db, "COMMIT", "batch commit"))
            return false;
        *transaction_open = false;
        if (!address_backfill_exec(db, "BEGIN", "next batch begin"))
            return false;
        *transaction_open = true;
        if (*processed % 100000 == 0) {
            printf("Address backfill: %lld addresses processed...\n",
                   (long long)*processed);
            fflush(stdout);
        }
    }
    if (rc == SQLITE_DONE)
        return true;
    LOG_ERROR("store", "address backfill cursor failed: rc=%d error=%s",
              rc, sqlite3_errmsg(db));
    return false;
}

struct address_backfill_context {
    sqlite3 *db;
    sqlite3_stmt *cursor;
    sqlite3_stmt *upsert;
    int64_t processed;
    bool transaction_open;
};

static bool address_backfill_open(struct address_backfill_context *ctx,
                                  const char *db_path)
{
    int rc = sqlite3_open_v2(db_path, &ctx->db, SQLITE_OPEN_READWRITE, NULL);
    if (rc == SQLITE_OK)
        return true;
    LOG_ERROR("store", "address backfill database open failed: rc=%d error=%s",
              rc, ctx->db ? sqlite3_errmsg(ctx->db) : "no handle");
    if (ctx->db && sqlite3_close(ctx->db) != SQLITE_OK)
        LOG_ERROR("store", "address backfill failed-open handle did not close");
    ctx->db = NULL;
    return false;
}

static bool address_backfill_transaction(struct address_backfill_context *ctx)
{
    if (!address_backfill_exec(ctx->db, "BEGIN", "transaction begin"))
        return false;
    ctx->transaction_open = true;
    if (!address_backfill_prepare(ctx->db, &ctx->cursor, &ctx->upsert))
        return false;
    if (!address_backfill_rows(ctx->db, ctx->cursor, ctx->upsert,
                               &ctx->processed, &ctx->transaction_open))
        return false;
    if (!address_backfill_finalize(ctx->db, &ctx->cursor, "cursor"))
        return false;
    if (!address_backfill_finalize(ctx->db, &ctx->upsert, "upsert"))
        return false;
    if (!address_backfill_exec(ctx->db,
            "INSERT OR REPLACE INTO node_state(key,value) "
            "VALUES('addresses_backfilled', X'01')",
            "publish completion marker"))
        return false;
    if (!address_backfill_exec(ctx->db, "COMMIT", "final commit"))
        return false;
    ctx->transaction_open = false;
    return true;
}

static bool address_backfill_cleanup(struct address_backfill_context *ctx,
                                     bool ok)
{
    if (!address_backfill_finalize(ctx->db, &ctx->cursor, "cursor cleanup"))
        ok = false;
    if (!address_backfill_finalize(ctx->db, &ctx->upsert, "upsert cleanup"))
        ok = false;
    if (ctx->transaction_open)
        address_backfill_rollback(ctx->db);
    int rc = sqlite3_close(ctx->db);
    ctx->db = NULL;
    if (rc == SQLITE_OK)
        return ok;
    LOG_ERROR("store", "address backfill database close failed: rc=%d", rc);
    return false;
}

/* One-shot background worker: aggregate the explorer `addresses` table
 * from the SQLite UTXO set. `arg` is a `const char *` SQLite db_path
 * whose backing string must outlive this thread. Returns NULL. */
bool boot_address_backfill_run(const char *db_path)
{
    struct address_backfill_context ctx = {0};
    int64_t t0 = (int64_t)platform_time_wall_time_t();
    if (!address_backfill_open(&ctx, db_path))
        return false;

    /* Disable mmap entirely for this background thread.
     * The previous mmap_size=64MB caused SIGSEGV after ~64K addresses:
     * when the main thread writes via WAL, the kernel may invalidate
     * mmap pages that this thread's sort cursor is scanning, triggering
     * a fault in SQLite's mmap read path. With mmap_size=0, SQLite
     * falls back to read() which is safe under concurrent WAL writes.
     * Performance is irrelevant — this is a one-time background job. */
    bool ok = address_backfill_configure(ctx.db) &&
              address_backfill_schema(ctx.db);
    if (ok) {
        printf("Address backfill: aggregating UTXOs...\n");
        fflush(stdout);
        /* Process in batches using a cursor over distinct address_hash values.
         * The old single-query approach caused SQLite sort/mmap pressure.
         * Batching keeps peak memory bounded. */
        ok = address_backfill_transaction(&ctx);
    }
    ok = address_backfill_cleanup(&ctx, ok);
    if (ok) {
        int64_t elapsed = (int64_t)platform_time_wall_time_t() - t0;
        printf("Address backfill: %lld addresses in %llds\n",
               (long long)ctx.processed, (long long)elapsed);
        fflush(stdout);
    }
    return ok;
}

void *backfill_addresses_thread(void *arg)
{
    (void)boot_address_backfill_run((const char *)arg);
    return NULL;
}
