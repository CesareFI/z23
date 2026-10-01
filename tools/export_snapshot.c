/* Minimal consensus snapshot exporter.
 * Copies only public tables from node.db to consensus_snapshot.db.
 * Usage: build/bin/export_snapshot [datadir] */

#include "platform/time_compat.h"
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

/* Build "ATTACH DATABASE 'file:<path>?mode=ro' AS src" with every single
 * quote in the path doubled, so an apostrophe in the datadir cannot break
 * the statement. False only when the buffer cannot hold the result. */
static bool attach_sql_build(const char *src_path, char *out, size_t cap)
{
    int n = snprintf(out, cap, "ATTACH DATABASE 'file:");
    if (n < 0 || (size_t)n >= cap) return false;
    size_t an = (size_t)n;
    for (const char *p = src_path; *p; p++) {
        size_t need = 1u + (*p == '\'' ? 1u : 0u) + sizeof("?mode=ro' AS src");
        if (an + need > cap) return false;
        if (*p == '\'') out[an++] = '\'';
        out[an++] = *p;
    }
    int t = snprintf(out + an, cap - an, "?mode=ro' AS src");
    return t > 0 && (size_t)t < cap - an;
}

/* Copy each public consensus table from the attached src schema; missing
 * tables are skipped, not fatal. Returns the number copied. */
static int snapshot_copy_tables(sqlite3 *dst)
{
    static const char *tables[] = {
        "blocks", "transactions", "utxos", "addresses",
        "chain_stats", "zslp_tokens", "zslp_balances",
        NULL
    };
    int copied = 0;
    for (int i = 0; tables[i]; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "CREATE TABLE %s AS SELECT * FROM src.%s",
            tables[i], tables[i]);
        char *err = NULL;
        int rc = sqlite3_exec(dst, sql, NULL, NULL, &err);
        if (rc == SQLITE_OK) {
            char cnt[128];
            snprintf(cnt, sizeof(cnt), "SELECT count(*) FROM %s", tables[i]);
            sqlite3_stmt *s = NULL;
            sqlite3_prepare_v2(dst, cnt, -1, &s, NULL);
            int rows = 0;
            if (s && sqlite3_step(s) == SQLITE_ROW)  // raw-sql-ok:standalone-dev-tool
                rows = sqlite3_column_int(s, 0);
            if (s) sqlite3_finalize(s);
            printf("  %-20s %d rows\n", tables[i], rows);
            copied++;
        } else {
            printf("  %-20s skipped (%s)\n", tables[i],
                   err ? err : "not found");
            if (err) sqlite3_free(err);
        }
    }
    return copied;
}

int main(int argc, char *argv[])
{
    char default_dd[256] = "./.zclassic-c23";
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(default_dd, sizeof(default_dd), "%s/.zclassic-c23", home);
    }
    const char *datadir = default_dd;
    if (argc >= 2) datadir = argv[1];

    char src_path[576], dst_path[576];
    snprintf(src_path, sizeof(src_path), "%s/node.db", datadir);
    snprintf(dst_path, sizeof(dst_path), "%s/consensus_snapshot.db", datadir);

    struct stat src_st;
    if (stat(src_path, &src_st) != 0) {
        fprintf(stderr, "No node.db at %s\n", src_path);
        return 1;
    }
    printf("Source: %s (%.0f MB)\n", src_path,
           (double)src_st.st_size / (1024.0*1024.0));

    unlink(dst_path);

    /* The connection is opened with SQLITE_OPEN_URI so the ATTACH below can
     * carry mode=ro: this tool's contract is READ-ONLY against the source
     * node.db (it must never hold a write-capable handle on the node's
     * database), and a read-only attach never needs -shm/-wal creation in
     * the source directory. */
    sqlite3 *dst = NULL;
    if (sqlite3_open_v2(dst_path, &dst,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
        NULL) != SQLITE_OK) {
        fprintf(stderr, "Can't create %s\n", dst_path);
        return 1;
    }

    sqlite3_exec(dst, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    sqlite3_exec(dst, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
    sqlite3_exec(dst, "PRAGMA cache_size=-262144", NULL, NULL, NULL);

    char attach[sizeof(src_path) * 2 + 96];
    if (!attach_sql_build(src_path, attach, sizeof(attach))) {
        fprintf(stderr, "ATTACH path too long\n");
        sqlite3_close(dst);
        return 1;
    }
    if (sqlite3_exec(dst, attach, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "ATTACH failed: %s\n", sqlite3_errmsg(dst));
        sqlite3_close(dst);
        return 1;
    }

    int64_t t0 = (int64_t)platform_time_wall_time_t();
    sqlite3_exec(dst, "BEGIN", NULL, NULL, NULL);

    int copied = snapshot_copy_tables(dst);

    sqlite3_exec(dst, "COMMIT", NULL, NULL, NULL);
    sqlite3_exec(dst, "DETACH DATABASE src", NULL, NULL, NULL);

    sqlite3_exec(dst, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    printf("Compacting...\n");
    sqlite3_exec(dst, "VACUUM", NULL, NULL, NULL);
    sqlite3_close(dst);

    int64_t elapsed = (int64_t)platform_time_wall_time_t() - t0;
    struct stat dst_st;
    stat(dst_path, &dst_st);
    printf("\nExported %d tables to %s (%.0f MB) in %llds\n",
           copied, dst_path, (double)dst_st.st_size / (1024.0*1024.0),
           (long long)elapsed);

    /* Delete stale manifest so it rebuilds with the new snapshot */
    char manifest_path[576];
    snprintf(manifest_path, sizeof(manifest_path),
             "%s/file_manifest.bin", datadir);
    if (unlink(manifest_path) == 0)
        printf("Deleted stale file_manifest.bin (will rebuild)\n");

    return 0;
}
