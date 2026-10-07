/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Shared snapshot import implementation for the pre-restore probe in boot.c
 * and the post-services receive path in boot_services.c. The function takes
 * `struct node_db *` directly (not boot_svc_ctx) so it can run before
 * services have been composed. See engine/composition/include/config/boot_snapshot_import.h.
 */

#include "config/boot_snapshot_import.h"
#include "base/serialize_le.h"
#include "chain/checkpoints.h"
#include "coins/utxo_commitment.h"
#include "models/database.h"
#include "services/chain_restore_boot_snapshot.h"
#include "services/reindex_epilogue.h"
#include "event/event.h"
#include "util/ar_step_readonly.h"
#include "util/boot_progress.h"
#include "util/log_macros.h"
#include "util/parse_num.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#define SNAPSHOT_IMPORT_PENDING_KEY "snapshot_import_pending_v1"
#define SNAPSHOT_IMPORT_RECEIPT_VERSION 1u
#define SNAPSHOT_IMPORT_RECEIPT_LEN (1u + 8u + 32u)

/* SQLite calls this only after executing more virtual-machine instructions.
 * Unlike a timer thread, it cannot claim progress while a disk operation is
 * hung. Returning zero preserves the statement; the callback records only a
 * cheap lock-free liveness timestamp. */
static int snapshot_import_progress(void *unused)
{
    (void)unused;
    boot_progress_tick("snapshot_import_bulk_insert");
    return 0;
}

static bool snapshot_read_height(sqlite3 *src, int64_t *out_height)
{
    sqlite3_stmt *q = NULL;
    bool ok = false;

    if (sqlite3_prepare_v2(src,
            "SELECT value FROM _snapshot_meta WHERE key='height'",
            -1, &q, NULL) == SQLITE_OK && q) {
        if (sqlite3_step(q) == SQLITE_ROW) {  // raw-sql-ok:read-only-snapshot
            const unsigned char *v = sqlite3_column_text(q, 0);
            ok = v && zcl_parse_i64((const char *)v, out_height) &&
                 *out_height >= 1 && *out_height < INT32_MAX;
        }
    }
    sqlite3_finalize(q);
    return ok;
}

static int snapshot_attach(sqlite3 *db, const char *path)
{
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db,
                               "ATTACH DATABASE ? AS snapsrc",
                               -1, &st, NULL);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
    if (rc == SQLITE_OK)
        rc = sqlite3_step(st); // raw-sql-ok:snapshot-attach

    int finalize_rc = sqlite3_finalize(st);
    if (rc == SQLITE_DONE)
        rc = finalize_rc;
    return rc;
}

static bool snapshot_rollback(sqlite3 *db)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        if (sqlite3_get_autocommit(db) != 0)
            return true;
        if (sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK)
            return sqlite3_get_autocommit(db) != 0;
        LOG_WARN("boot_snapshot_import",
                 "ROLLBACK attempt %d failed: %s",
                 attempt + 1, sqlite3_errmsg(db));
    }
    return sqlite3_get_autocommit(db) != 0;
}

static bool snapshot_detach(sqlite3 *db)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        if (sqlite3_exec(db, "DETACH DATABASE snapsrc",
                         NULL, NULL, NULL) == SQLITE_OK)
            return true;
        LOG_WARN("boot_snapshot_import",
                 "DETACH attempt %d failed: %s",
                 attempt + 1, sqlite3_errmsg(db));
    }
    return false;
}

static void snapshot_detach_after_begin_failure(sqlite3 *db)
{
    if (!snapshot_detach(db))
        LOG_WARN("boot_snapshot_import",
                 "could not detach snapshot after BEGIN failure");
}

static bool snapshot_finish_copy(struct node_db *ndb, bool copy_ok,
                                 int64_t snap_height,
                                 const uint8_t best_hash[32])
{
    bool ok = copy_ok;
    uint8_t receipt[SNAPSHOT_IMPORT_RECEIPT_LEN] = {
        SNAPSHOT_IMPORT_RECEIPT_VERSION
    };
    zcl_write_i64_le(receipt + 1, snap_height);
    memcpy(receipt + 9, best_hash, 32);
    if (ok)
        ok = node_db_state_set(ndb, SNAPSHOT_IMPORT_PENDING_KEY,
                               receipt, sizeof(receipt));
    if (ok && sqlite3_exec(ndb->db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        LOG_WARN("boot_snapshot_import",
                 "COMMIT failed (%s) — rolling back snapshot install",
                 sqlite3_errmsg(ndb->db));
        ok = false;
    }
    if (!ok && !snapshot_rollback(ndb->db))
        LOG_WARN("boot_snapshot_import",
                 "snapshot transaction remains open after rollback retries");

    sqlite3_progress_handler(ndb->db, 0, NULL, NULL);
    if (!snapshot_detach(ndb->db))
        ok = false;
    return ok;
}

bool boot_snapshot_import_pending(struct node_db *ndb, bool *pending)
{
    if (pending)
        *pending = true;
    if (!ndb || !ndb->open || !ndb->db || !pending)
        return false;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(ndb->db,
            "SELECT 1 FROM node_state WHERE key=?",
            -1, &stmt, NULL);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_text(stmt, 1, SNAPSHOT_IMPORT_PENDING_KEY,
                               -1, SQLITE_STATIC);
    if (rc == SQLITE_OK)
        rc = sqlite3_step(stmt); // raw-sql-ok:read-only-snapshot-receipt
    if (rc == SQLITE_ROW)
        *pending = true;
    else if (rc == SQLITE_DONE)
        *pending = false;
    int finalize_rc = sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) &&
           finalize_rc == SQLITE_OK;
}

bool boot_snapshot_import_can_skip(struct node_db *ndb,
                                   int64_t existing_utxos)
{
    bool pending = true;
    if (!boot_snapshot_import_pending(ndb, &pending)) {
        LOG_WARN("boot_snapshot_import",
                 "snapshot import receipt unreadable; forcing retry");
        return false;
    }
    return existing_utxos > 1000 && !pending;
}

static bool snapshot_finish_authority(struct node_db *ndb,
                                      const char *main_db_path,
                                      int snap_height,
                                      const uint8_t best_hash[32])
{
    if (!reindex_epilogue_derive_imported_snapshot(
            ndb, main_db_path, snap_height, best_hash)) {
        LOG_WARN("boot_snapshot_import",
                 "snapshot imported into node.db but authority epilogue failed "
                 "at h=%d; refusing fast-rebuild success", snap_height);
        return false;
    }
    if (!node_db_state_delete(ndb, SNAPSHOT_IMPORT_PENDING_KEY)) {
        LOG_WARN("boot_snapshot_import",
                 "authority complete but pending receipt clear failed");
        return false;
    }
    return true;
}

struct snapshot_pending_receipt {
    int height;
    int64_t utxo_count;
    uint8_t best_hash[32];
};

static bool snapshot_pending_receipt_load(
    struct node_db *ndb, struct snapshot_pending_receipt *out)
{
    if (!ndb || !ndb->open || !ndb->db || !out)
        return false;

    uint8_t receipt[SNAPSHOT_IMPORT_RECEIPT_LEN] = {0};
    size_t receipt_len = 0;
    if (!node_db_state_get(ndb, SNAPSHOT_IMPORT_PENDING_KEY,
                           receipt, sizeof(receipt), &receipt_len) ||
        receipt_len != sizeof(receipt) ||
        receipt[0] != SNAPSHOT_IMPORT_RECEIPT_VERSION) {
        LOG_WARN("boot_snapshot_import",
                 "pending snapshot receipt missing or malformed");
        return false;
    }

    int64_t snap_height = zcl_read_i64_le(receipt + 1);
    int64_t utxo_count = 0;
    uint8_t stored_hash[32] = {0};
    size_t stored_hash_len = 0;
    if (!node_db_utxo_count_checked(ndb, &utxo_count) ||
        snap_height < 1 || snap_height >= INT32_MAX || utxo_count < 1000 ||
        !node_db_state_get(ndb, "coins_best_block", stored_hash,
                           sizeof(stored_hash), &stored_hash_len) ||
        stored_hash_len != sizeof(stored_hash) ||
        memcmp(stored_hash, receipt + 9, sizeof(stored_hash)) != 0) {
        LOG_WARN("boot_snapshot_import",
                 "pending snapshot receipt does not match installed anchor");
        return false;
    }

    out->height = (int)snap_height;
    out->utxo_count = utxo_count;
    memcpy(out->best_hash, receipt + 9, sizeof(out->best_hash));
    return true;
}

bool boot_snapshot_import_resume(struct node_db *ndb,
                                 int64_t *out_utxo_count,
                                 int64_t *out_snap_height,
                                 uint8_t out_best_hash[32])
{
    struct snapshot_pending_receipt receipt = {0};
    if (!snapshot_pending_receipt_load(ndb, &receipt))
        return false;

    const char *main_db_path = sqlite3_db_filename(ndb->db, "main");
    if (!snapshot_finish_authority(ndb, main_db_path,
                                   receipt.height, receipt.best_hash))
        return false;

    if (out_utxo_count)
        *out_utxo_count = receipt.utxo_count;
    if (out_snap_height)
        *out_snap_height = receipt.height;
    if (out_best_hash)
        memcpy(out_best_hash, receipt.best_hash, 32);
    return true;
}

bool boot_snapshot_import_resume_pending(struct node_db *ndb)
{
    bool pending = false;
    if (!boot_snapshot_import_pending(ndb, &pending)) {
        LOG_WARN("boot_snapshot_import", "snapshot recovery receipt unreadable");
        return false;
    }
    if (!pending)
        return true;

    int64_t utxo_count = 0, snap_height = 0;
    uint8_t best_hash[32] = {0};
    if (!boot_snapshot_import_resume(ndb, &utxo_count,
                                     &snap_height, best_hash)) {
        LOG_WARN("boot_snapshot_import",
                 "pending snapshot authority recovery failed");
        return false;
    }
    chain_restore_record_snapshot_import(true, utxo_count, snap_height);
    event_emitf(EV_BOOT_UTXO_IMPORT, 0,
                "phase=pre-restore-resume ok=1 utxos=%lld height=%lld",
                (long long)utxo_count, (long long)snap_height);
    printf("[boot] pending snapshot authority recovery OK: "
           "%lld UTXOs at h=%lld\n",
           (long long)utxo_count, (long long)snap_height);
    return true;
}

bool boot_import_snapshot_db(struct node_db *ndb,
                              const char *snapshot_path,
                              int64_t *out_utxo_count,
                              int64_t *out_snap_height,
                              uint8_t out_best_hash[32])
{
    if (!ndb || !ndb->open || !ndb->db || !snapshot_path)
        LOG_FAIL("boot_snapshot_import", "null inputs");

    struct stat st;
    if (stat(snapshot_path, &st) != 0)
        LOG_FAIL("boot_snapshot_import", "stat %s: %s",
                 snapshot_path, strerror(errno));
    if (st.st_size < (off_t)(10 * 1024 * 1024))
        LOG_FAIL("boot_snapshot_import",
                 "snapshot too small (%lld bytes) — likely truncated",
                 (long long)st.st_size);

    sqlite3 *src = NULL;
    if (sqlite3_open_v2(snapshot_path, &src,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        const char *m = src ? sqlite3_errmsg(src) : "n/a";
        if (src) sqlite3_close(src);
        LOG_FAIL("boot_snapshot_import", "open ro %s: %s",
                 snapshot_path, m);
    }

    bool integrity_ok = false;
    {
        sqlite3_stmt *ck = NULL;
        if (sqlite3_prepare_v2(src, "PRAGMA integrity_check",
                               -1, &ck, NULL) == SQLITE_OK && ck) {
            if (sqlite3_step(ck) == SQLITE_ROW) {  // raw-sql-ok:integrity-pragma
                const unsigned char *r = sqlite3_column_text(ck, 0);
                integrity_ok = r && strcmp((const char *)r, "ok") == 0;
            }
            sqlite3_finalize(ck);
        }
    }
    if (!integrity_ok) {
        sqlite3_close(src);
        LOG_FAIL("boot_snapshot_import",
                 "integrity_check failed for %s", snapshot_path);
    }

    int64_t snap_height = 0;
    if (!snapshot_read_height(src, &snap_height)) {
        sqlite3_close(src);
        LOG_FAIL("boot_snapshot_import",
                 "missing/invalid _snapshot_meta.height");
    }

    /* PROVENANCE GATE (defense-in-depth for the peer-served path):
     * this importer installs a `consensus_snapshot.db` that arrived over the
     * unauthenticated file_service (file_index=254) — the per-chunk SHA3 only
     * proves the bytes match the SERVING PEER's manifest, not that the coin set
     * is the real consensus set. We only have an in-binary cryptographic ground
     * truth AT the single compiled checkpoint (the WRITE-TIME SHA3 reject
     * below). ABOVE the checkpoint there is no in-binary root and no anchor
     * binding here, so a forged snapshot at an arbitrary height would otherwise
     * be installed as ground truth (forged-money / consensus divergence).
     * REFUSE to promote an above-checkpoint peer snapshot — fall back to safe
     * P2P IBD. The assisted operator bundle path
     * (boot_load_snapshot_at_own_height_reset, which checks that the snapshot
     * anchor names a validated block_index location) is the supported way to
     * seed above the checkpoint; that location check does not authenticate
     * UTXO/shielded contents, and the path does NOT use this function. */
    {
        const struct sha3_utxo_checkpoint *cp = get_sha3_utxo_checkpoint();
        if (cp && snap_height > (int64_t)cp->height) {
            sqlite3_close(src);
            LOG_FAIL("boot_snapshot_import",
                     "REFUSING peer snapshot at h=%lld (above compiled "
                     "checkpoint h=%llu): no in-binary root to consensus-verify "
                     "it against — falling back to P2P IBD / operator bundle",
                     (long long)snap_height,
                     (unsigned long long)cp->height);
        }
    }

    uint8_t best_hash[32] = {0};
    bool best_found = false;
    {
        sqlite3_stmt *q = NULL;
        if (sqlite3_prepare_v2(src,
                "SELECT hash FROM blocks WHERE height=?",
                -1, &q, NULL) == SQLITE_OK && q) {
            sqlite3_bind_int64(q, 1, snap_height);
            if (sqlite3_step(q) == SQLITE_ROW) {  // raw-sql-ok:read-only-snapshot
                const void *b = sqlite3_column_blob(q, 0);
                int n = sqlite3_column_bytes(q, 0);
                if (b && n == 32) {
                    memcpy(best_hash, b, 32);
                    best_found = true;
                }
            }
            sqlite3_finalize(q);
        }
    }
    if (!best_found) {
        sqlite3_close(src);
        LOG_FAIL("boot_snapshot_import",
                 "no blocks row at h=%lld", (long long)snap_height);
    }

    int64_t snap_utxos = 0;
    {
        sqlite3_stmt *q = NULL;
        if (sqlite3_prepare_v2(src,
                "SELECT COUNT(*) FROM utxos",
                -1, &q, NULL) == SQLITE_OK && q) {
            if (sqlite3_step(q) == SQLITE_ROW)  // raw-sql-ok:read-only-snapshot
                snap_utxos = sqlite3_column_int64(q, 0);
            sqlite3_finalize(q);
        }
    }
    sqlite3_close(src);
    if (snap_utxos < 1000)
        LOG_FAIL("boot_snapshot_import",
                 "implausible utxo count %lld", (long long)snap_utxos);

    char *err = NULL;
    if (snapshot_attach(ndb->db, snapshot_path) != SQLITE_OK)
        LOG_FAIL("boot_snapshot_import", "ATTACH failed: %s",
                 sqlite3_errmsg(ndb->db));

    bool ok = true;
    if (sqlite3_exec(ndb->db, "BEGIN IMMEDIATE", NULL, NULL, &err)
        != SQLITE_OK) {
        char msg[256] = "?";
        if (err) { snprintf(msg, sizeof(msg), "%s", err); sqlite3_free(err); }
        snapshot_detach_after_begin_failure(ndb->db);
        LOG_FAIL("boot_snapshot_import", "BEGIN failed: %s", msg);
    }
    /* Report only measured SQLite VM progress while the bulk copy and
     * commitment scan run. A timer-based pump falsely kept a genuinely hung
     * INSERT alive forever once watchdog progress exemptions became active. */
    sqlite3_progress_handler(ndb->db, 50000,
                             snapshot_import_progress, NULL);
    if (ok && ar_exec_write_sql(ndb->db, "DELETE FROM main.utxos")
                  != SQLITE_OK) {
        fprintf(stderr, "[boot_snapshot_import] clear utxos: %s\n",  // obs-ok:bulk-import-failure
                sqlite3_errmsg(ndb->db));
        ok = false;
    }
    if (ok && ar_exec_write_sql(ndb->db,
            "INSERT INTO main.utxos SELECT * FROM snapsrc.utxos")
                  != SQLITE_OK) {
        fprintf(stderr, "[boot_snapshot_import] copy utxos: %s\n",  // obs-ok:bulk-import-failure
                sqlite3_errmsg(ndb->db));
        ok = false;
    }
    /* WRITE-TIME VERIFICATION (mirrors utxo_recovery_restore.c):
     * before COMMIT, recompute the SHA3 over the just-installed set. At the
     * compiled checkpoint height there is a cryptographic ground truth, so
     * REJECT unless (root,count) byte-match it — a peer's per-chunk transport
     * SHA3 only proves the file matches the SERVING PEER's manifest, NOT that
     * the coin set is the real consensus set. Runs while the SQLite progress
     * handler remains active (the walk is O(set)). Above the checkpoint there
     * is no
     * compiled root to verify against; that non-checkpoint provenance gap (the
     * snapshot path writes no cold-import seed, so the boot torn-gate cannot
     * see it) is a documented follow-up — the checkpoint reject is the
     * load-bearing half and closes the fabricate-at-the-checkpoint hole. */
    if (ok) {
        const struct sha3_utxo_checkpoint *cp = get_sha3_utxo_checkpoint();
        if (cp && snap_height == (int64_t)cp->height) {
            uint8_t root[32]; uint64_t cnt = 0;
            utxo_commitment_sha3_compute(ndb->db, root, &cnt);
            if (cnt != cp->utxo_count || memcmp(root, cp->sha3_hash, 32) != 0) {
                LOG_WARN("boot_snapshot_import",
                         "checkpoint SHA3 MISMATCH at h=%lld (count=%llu "
                         "want=%llu) — snapshot REJECTED, not installed as "
                         "ground truth",
                         (long long)snap_height, (unsigned long long)cnt,
                         (unsigned long long)cp->utxo_count);
                ok = false;
            } else {
                printf("[boot_snapshot_import] checkpoint SHA3 verified at "
                       "h=%lld (%llu UTXOs)\n", (long long)snap_height,
                       (unsigned long long)cnt);
            }
        }
    }
    /* coins_best_block is a projection cache, but it must describe the same
     * UTXO generation that this transaction publishes. A failed cache write
     * therefore rejects and rolls back the replacement set atomically. */
    if (ok && !node_db_state_set(ndb, "coins_best_block",
                                 best_hash, sizeof(best_hash))) {
        LOG_WARN("boot_snapshot_import", "set coins_best_block failed");
        ok = false;
    }
    /* A failed COMMIT (SQLITE_FULL / I/O error) leaves the bulk-copy
     * transaction uncommitted. Never stamp coins_best_block until checked,
     * bounded rollback and detach cleanup has finished. */
    ok = snapshot_finish_copy(ndb, ok, snap_height, best_hash);

    if (!ok) {
        LOG_FAIL("boot_snapshot_import",
                 "snapshot install failed; cleanup attempted");
        return false;
    }

    /* Verified-install epilogue: a snapshot import is only a fast rebuild if
     * it seeds the same durable authority surface as a full reindex. Reuse the
     * reindex epilogue so both paths derive coins_kv, coins_applied_height,
     * utxo_sha3, trusted cursors, and H* with one implementation. */
    const char *main_db_path = sqlite3_db_filename(ndb->db, "main");
    if (!snapshot_finish_authority(ndb, main_db_path,
                                   (int)snap_height, best_hash))
        return false;

    if (out_utxo_count)  *out_utxo_count  = snap_utxos;
    if (out_snap_height) *out_snap_height = snap_height;
    if (out_best_hash)   memcpy(out_best_hash, best_hash, 32);
    return true;
}
