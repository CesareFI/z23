/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * End-to-end regression for the FRESH-DATADIR SNAPSHOT BOOT seed path.
 *
 * Defect class this guards against: a snapshot import that stops at writing
 * `coins_best_block` without seeding the state the reducer folds over
 * (coins_kv, coins_applied_height, the trusted stage cursors, utxo_sha3). The
 * node then LOOKS synced (a best-block hash on disk) but H* never climbs off
 * the compiled anchor and it never reaches at_tip. The cure routes both the
 * full reindex AND the snapshot import through ONE derivation
 * (reindex_epilogue_derive_imported_snapshot); test_reindex_epilogue.c proves
 * that derivation in isolation.
 *
 * What this test adds: it drives the REAL boot entry point
 * boot_import_snapshot_db() against a genuine on-disk fixture snapshot.db —
 * exercising the whole honest path (integrity_check, _snapshot_meta height,
 * provenance gate, blocks-hash lookup, ATTACH + bulk copy, write-time SHA3,
 * coins_best_block cache write, AND the shared authority epilogue). It then
 * asserts the seeded state is SELF-CONSISTENT: coins_kv reseeded from the
 * imported UTXOs, coins_applied_height at the snapshot next-height frame,
 * coins_best_block == the snapshot tip hash, the stage cursors clamped to the
 * snapshot height, H* CLIMBED off zero (to the covered imported base), the
 * utxo_sha3 stamped, and NO stage naming a blocker. If a future edit reverts
 * boot_import_snapshot_db back to a coins_best_block-only write, this group
 * fails — it locks the "one honest code path" that seeds EVERYTHING.
 *
 * Height regime: boot_import_snapshot_db REFUSES a peer snapshot above the
 * compiled checkpoint (provenance gate) and requires a real SHA3 match AT the
 * checkpoint, so the only synthetically drivable height is BELOW the checkpoint
 * / finality anchor. A compiled checkpoint not covered by this datadir cannot
 * manufacture state above the imported tip, so the epilogue roots H* at the
 * covered imported base instead. */

#include "test/test_core.h"

#include "config/boot_snapshot_import.h"
#include "coins/utxo_commitment.h"
#include "jobs/reducer_frontier.h"
#include "jobs/tip_finalize_stage.h"
#include "models/database.h"
#include "services/seed_integrity_gate.h"
#include "storage/coins_kv.h"
#include "storage/progress_store.h"
#include "util/blocker.h"
#include "util/boot_progress.h"
#include "validation/main_state.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SB_CHECK(name, expr) do {                              \
    printf("  snapshot_boot_seed: %s... ", (name));            \
    if (expr) printf("OK\n");                                  \
    else { printf("FAIL\n"); failures++; }                     \
} while (0)

/* Snapshot height BELOW the compiled checkpoint/anchor (3,056,758): the only
 * height a synthetic fixture can drive through boot_import_snapshot_db's
 * provenance + write-time-SHA3 gates. */
#define SB_SNAP_HEIGHT 1000000
#define SB_UTXO_COUNT  1200          /* > the importer's 1000-row plausibility floor */

static int sb_mkdir(const char *p)
{
    if (mkdir(p, 0700) == 0) return 0;
    return (errno == EEXIST) ? 0 : -1;
}

/* The deterministic 32-byte block hash the fixture records at SB_SNAP_HEIGHT;
 * boot_import_snapshot_db must copy it verbatim into coins_best_block. */
static void sb_tip_hash(uint8_t out[32])
{
    for (int i = 0; i < 32; i++) out[i] = (uint8_t)(0xa0 ^ (i * 7 + 3));
}

/* Build a real consensus_snapshot.db fixture: the public tables the importer
 * reads (_snapshot_meta, blocks, utxos) + a padding blob so the file clears the
 * importer's 10 MiB "likely-truncated" size floor. utxos MUST carry the exact
 * 8-column node.db schema/order because the importer bulk-copies with
 * `INSERT INTO main.utxos SELECT * FROM snapsrc.utxos`. */
static bool sb_build_fixture(const char *path, const uint8_t tip_hash[32])
{
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) return false;
    bool ok = true;
    char *err = NULL;

    ok = ok && sqlite3_exec(db,
        "CREATE TABLE _snapshot_meta(key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE blocks(hash BLOB PRIMARY KEY, height INTEGER NOT NULL);"
        "CREATE TABLE utxos("
        "  txid BLOB NOT NULL, vout INTEGER NOT NULL,"
        "  value INTEGER NOT NULL, script BLOB NOT NULL,"
        "  script_type INTEGER NOT NULL DEFAULT 0,"
        "  address_hash BLOB, height INTEGER NOT NULL,"
        "  is_coinbase INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY(txid,vout));"
        "CREATE TABLE _pad(id INTEGER PRIMARY KEY, blob BLOB);",
        NULL, NULL, &err) == SQLITE_OK;
    if (err) { sqlite3_free(err); err = NULL; }

    /* _snapshot_meta.height */
    if (ok) {
        sqlite3_stmt *st = NULL;
        ok = sqlite3_prepare_v2(db,
            "INSERT INTO _snapshot_meta(key,value) VALUES('height',?)",
            -1, &st, NULL) == SQLITE_OK;
        if (ok) {
            char h[32]; snprintf(h, sizeof(h), "%d", SB_SNAP_HEIGHT);
            sqlite3_bind_text(st, 1, h, -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        }
        sqlite3_finalize(st);
    }

    /* blocks row at the snapshot tip height */
    if (ok) {
        sqlite3_stmt *st = NULL;
        ok = sqlite3_prepare_v2(db,
            "INSERT INTO blocks(hash,height) VALUES(?,?)",
            -1, &st, NULL) == SQLITE_OK;
        if (ok) {
            sqlite3_bind_blob(st, 1, tip_hash, 32, SQLITE_STATIC);
            sqlite3_bind_int(st, 2, SB_SNAP_HEIGHT);
            ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        }
        sqlite3_finalize(st);
    }

    /* SB_UTXO_COUNT deterministic UTXOs (all at heights <= snapshot height). */
    if (ok) ok = sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK;
    if (ok) {
        sqlite3_stmt *st = NULL;
        ok = sqlite3_prepare_v2(db,
            "INSERT INTO utxos"
            "(txid,vout,value,script,script_type,address_hash,height,is_coinbase)"
            " VALUES(?,?,?,?,0,NULL,?,?)", -1, &st, NULL) == SQLITE_OK;
        for (int i = 0; ok && i < SB_UTXO_COUNT; i++) {
            uint8_t txid[32]; memset(txid, 0, 32);
            txid[0] = (uint8_t)(i & 0xFF);
            txid[1] = (uint8_t)((i >> 8) & 0xFF);
            txid[31] = 0x5a;
            uint8_t script[6] = {0x76, 0xa9, (uint8_t)i, 0x14, 0x88, 0xac};
            sqlite3_reset(st);
            sqlite3_bind_blob(st, 1, txid, 32, SQLITE_STATIC);
            sqlite3_bind_int(st, 2, 0);
            sqlite3_bind_int64(st, 3, (int64_t)1000 + i);
            sqlite3_bind_blob(st, 4, script, sizeof(script), SQLITE_STATIC);
            sqlite3_bind_int(st, 5, (i % SB_SNAP_HEIGHT));   /* height */
            sqlite3_bind_int(st, 6, i == 0 ? 1 : 0);         /* is_coinbase */
            ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        }
        sqlite3_finalize(st);
    }
    if (ok) ok = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;

    /* Padding: push the file past the importer's 10 MiB size floor with a
     * single ~11 MiB zeroblob (read-only-attached, never copied). */
    if (ok) {
        sqlite3_stmt *st = NULL;
        ok = sqlite3_prepare_v2(db,
            "INSERT INTO _pad(blob) VALUES(zeroblob(?))", -1, &st, NULL)
            == SQLITE_OK;
        if (ok) {
            sqlite3_bind_int(st, 1, 11 * 1024 * 1024);
            ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        }
        sqlite3_finalize(st);
    }

    sqlite3_close(db);
    return ok;
}

static int32_t sb_trusted_base(sqlite3 *db)
{
    uint8_t blob[8] = {0}; size_t n = 0; bool found = false;
    if (!progress_meta_get(db, REDUCER_TRUSTED_BASE_HEIGHT_KEY,
                           blob, sizeof(blob), &n, &found) || !found || n != 8)
        return -1;
    int64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | blob[i];
    return (int32_t)v;
}

static int64_t sb_cursor(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;
    int64_t out = -1;
    if (sqlite3_prepare_v2(db, "SELECT cursor FROM stage_cursor WHERE name=?",
                           -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW)  // raw-sql-ok:test-fixture-verify
        out = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return out;
}

static bool sb_set_snapshot_height(const char *path, const char *height)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK)
        goto done;
    if (sqlite3_prepare_v2(db,
            "UPDATE _snapshot_meta SET value=? WHERE key='height'",
            -1, &st, NULL) != SQLITE_OK)
        goto done;
    if (sqlite3_bind_text(st, 1, height, -1, SQLITE_STATIC) != SQLITE_OK)
        goto done;
    if (sqlite3_step(st) != SQLITE_DONE) // raw-sql-ok:test-fixture-seeding
        goto done;
    sqlite3_finalize(st);
    return sqlite3_close(db) == SQLITE_OK;
done:
    sqlite3_finalize(st);
    if (db) sqlite3_close(db);
    return false;
}

static int sb_test_malformed_height(struct node_db *ndb, const char *path)
{
    int failures = 0;
    SB_CHECK("malformed-height fixture planted",
             sb_set_snapshot_height(path, "1000000junk"));

    int64_t count = -1, height = -1;
    uint8_t best[32] = {0};
    uint8_t zero_hash[32] = {0};
    SB_CHECK("malformed snapshot height is rejected",
             !boot_import_snapshot_db(ndb, path, &count, &height, best));
    SB_CHECK("malformed snapshot leaves outputs untouched",
             count == -1 && height == -1 &&
             memcmp(best, zero_hash, sizeof(best)) == 0);
    SB_CHECK("overflow-height fixture planted",
             sb_set_snapshot_height(path, "2147483647"));
    SB_CHECK("height whose next cursor would overflow is rejected",
             !boot_import_snapshot_db(ndb, path, NULL, NULL, NULL));
    return failures;
}

struct sb_copy_fault {
    bool denied_rollback;
    bool denied_detach;
};

static int sb_deny_copy_and_first_cleanup(void *ctx, int action,
                                          const char *target,
                                          const char *detail,
                                          const char *db_name,
                                          const char *trigger)
{
    (void)detail;
    (void)trigger;
    struct sb_copy_fault *fault = ctx;
    if (action == SQLITE_INSERT && target && db_name &&
        strcmp(target, "utxos") == 0 && strcmp(db_name, "main") == 0)
        return SQLITE_DENY;
    if (action == SQLITE_TRANSACTION && target &&
        strcmp(target, "ROLLBACK") == 0 && !fault->denied_rollback) {
        fault->denied_rollback = true;
        return SQLITE_DENY;
    }
    if (action == SQLITE_DETACH && !fault->denied_detach) {
        fault->denied_detach = true;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static bool sb_has_attached_snapshot(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    bool found = false;
    if (sqlite3_prepare_v2(db, "PRAGMA database_list", -1, &st, NULL) !=
        SQLITE_OK)
        return true;
    while (sqlite3_step(st) == SQLITE_ROW) { // raw-sql-ok:test-fixture-verify
        const unsigned char *name = sqlite3_column_text(st, 1);
        if (name && strcmp((const char *)name, "snapsrc") == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(st);
    return found;
}

static int sb_test_copy_rollback(struct node_db *ndb, const char *path)
{
    int failures = 0;
    const char *seed_sql =
        "INSERT INTO utxos"
        "(txid,vout,value,script,script_type,address_hash,height,is_coinbase)"
        " VALUES(zeroblob(32),7,99,x'51',0,NULL,1,0)";
    SB_CHECK("rollback fixture: prior UTXO planted",
             sqlite3_exec(ndb->db, seed_sql, NULL, NULL, NULL) == SQLITE_OK);

    struct sb_copy_fault fault = {0};
    SB_CHECK("rollback fixture: fault authorizer installed",
             sqlite3_set_authorizer(ndb->db,
                                    sb_deny_copy_and_first_cleanup,
                                    &fault) == SQLITE_OK);
    int64_t count = -1, height = -1;
    uint8_t best[32] = {0};
    uint8_t zero_hash[32] = {0};
    SB_CHECK("failed copy is rejected",
             !boot_import_snapshot_db(ndb, path, &count, &height, best));
    SB_CHECK("rollback denial was injected", fault.denied_rollback);
    SB_CHECK("detach denial was injected", fault.denied_detach);
    SB_CHECK("failed copy closes its transaction",
             sqlite3_get_autocommit(ndb->db) != 0);
    SB_CHECK("failed copy detaches snapshot",
             !sb_has_attached_snapshot(ndb->db));
    SB_CHECK("failed copy preserves prior UTXO",
             node_db_utxo_count(ndb) == 1);
    SB_CHECK("failed copy leaves outputs untouched",
             count == -1 && height == -1 &&
             memcmp(best, zero_hash, sizeof(best)) == 0);

    sqlite3_set_authorizer(ndb->db, NULL, NULL);
    sqlite3_exec(ndb->db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_exec(ndb->db, "DETACH DATABASE snapsrc", NULL, NULL, NULL);
    sqlite3_exec(ndb->db, "DELETE FROM utxos", NULL, NULL, NULL);
    return failures;
}

static int sb_deny_anchor_write(void *ctx, int action, const char *target,
                                const char *detail, const char *db_name,
                                const char *trigger)
{
    (void)ctx;
    (void)detail;
    (void)trigger;
    return action == SQLITE_INSERT && target && db_name &&
           strcmp(target, "node_state") == 0 &&
           strcmp(db_name, "main") == 0 ? SQLITE_DENY : SQLITE_OK;
}

static int sb_test_anchor_atomicity(struct node_db *ndb, const char *path)
{
    int failures = 0;
    uint8_t prior_hash[32];
    memset(prior_hash, 0x6b, sizeof(prior_hash));
    const char *seed_sql =
        "INSERT INTO utxos"
        "(txid,vout,value,script,script_type,address_hash,height,is_coinbase)"
        " VALUES(zeroblob(32),9,101,x'52',0,NULL,2,0)";
    SB_CHECK("anchor fixture: prior UTXO planted",
             sqlite3_exec(ndb->db, seed_sql, NULL, NULL, NULL) == SQLITE_OK);
    SB_CHECK("anchor fixture: prior hash planted",
             node_db_state_set(ndb, "coins_best_block",
                               prior_hash, sizeof(prior_hash)));
    SB_CHECK("anchor fixture: write fault installed",
             sqlite3_set_authorizer(ndb->db, sb_deny_anchor_write, NULL) ==
             SQLITE_OK);

    int64_t count = -1, height = -1;
    uint8_t best[32] = {0};
    SB_CHECK("failed anchor write rejects import",
             !boot_import_snapshot_db(ndb, path, &count, &height, best));
    SB_CHECK("failed anchor write preserves prior UTXO",
             node_db_utxo_count(ndb) == 1);
    uint8_t actual_hash[32] = {0};
    size_t actual_len = 0;
    SB_CHECK("failed anchor write preserves prior hash",
             node_db_state_get(ndb, "coins_best_block", actual_hash,
                               sizeof(actual_hash), &actual_len) &&
             actual_len == sizeof(actual_hash) &&
             memcmp(actual_hash, prior_hash, sizeof(actual_hash)) == 0);

    sqlite3_set_authorizer(ndb->db, NULL, NULL);
    sqlite3_exec(ndb->db, "DELETE FROM utxos", NULL, NULL, NULL);
    sqlite3_exec(ndb->db,
                 "DELETE FROM node_state WHERE key='coins_best_block'",
                 NULL, NULL, NULL);
    return failures;
}

static int sb_deny_coins_reset(void *ctx, int action, const char *target,
                               const char *detail, const char *db_name,
                               const char *trigger)
{
    (void)ctx;
    (void)detail;
    (void)db_name;
    (void)trigger;
    return action == SQLITE_DELETE && target &&
           strcmp(target, "coins") == 0 ? SQLITE_DENY : SQLITE_OK;
}

static int sb_test_epilogue_resume(struct node_db *ndb, sqlite3 *pdb,
                                   const char *path)
{
    int failures = 0;
    SB_CHECK("resume fixture: epilogue fault installed",
             sqlite3_set_authorizer(pdb, sb_deny_coins_reset, NULL) ==
             SQLITE_OK);
    SB_CHECK("failed authority epilogue rejects import",
             !boot_import_snapshot_db(ndb, path, NULL, NULL, NULL));

    bool pending = false;
    SB_CHECK("failed authority epilogue leaves pending receipt",
             boot_snapshot_import_pending(ndb, &pending) && pending);
    SB_CHECK("pending receipt prevents existing-UTXO skip",
             !boot_snapshot_import_can_skip(ndb, SB_UTXO_COUNT));
    SB_CHECK("resume fixture: epilogue fault removed",
             sqlite3_set_authorizer(pdb, NULL, NULL) == SQLITE_OK);
    SB_CHECK("resume fixture: snapshot artifact removed",
             unlink(path) == 0);
    int64_t resumed_utxos = 0, resumed_height = 0;
    uint8_t resumed_hash[32] = {0};
    uint8_t expected_hash[32] = {0};
    sb_tip_hash(expected_hash);
    SB_CHECK("pending snapshot import resumes",
             boot_snapshot_import_resume(ndb, &resumed_utxos,
                                         &resumed_height, resumed_hash));
    SB_CHECK("artifact-free resume reports installed UTXOs",
             resumed_utxos == SB_UTXO_COUNT);
    SB_CHECK("artifact-free resume reports snapshot height",
             resumed_height == SB_SNAP_HEIGHT);
    SB_CHECK("artifact-free resume reports snapshot hash",
             memcmp(resumed_hash, expected_hash, sizeof(resumed_hash)) == 0);
    SB_CHECK("successful resume clears pending receipt",
             boot_snapshot_import_pending(ndb, &pending) && !pending);
    SB_CHECK("completed receipt allows existing-UTXO skip",
             boot_snapshot_import_can_skip(ndb, SB_UTXO_COUNT));
    const uint8_t malformed_receipt = 1;
    SB_CHECK("malformed receipt fixture planted",
             node_db_state_set(ndb, "snapshot_import_pending_v1",
                               &malformed_receipt,
                               sizeof(malformed_receipt)));
    SB_CHECK("malformed receipt fails closed",
             !boot_snapshot_import_resume(ndb, NULL, NULL, NULL));
    SB_CHECK("malformed receipt remains pending",
             boot_snapshot_import_pending(ndb, &pending) && pending);
    SB_CHECK("malformed receipt fixture cleared",
             node_db_state_delete(ndb, "snapshot_import_pending_v1"));
    return failures;
}

int test_snapshot_boot_seed(void);
int test_snapshot_boot_seed(void)
{
    test_reset_shared_globals();
    printf("\n=== snapshot_boot_seed tests ===\n");
    int failures = 0;

    blocker_module_init();
    blocker_reset_for_testing();
    seed_integrity_gate_reset_for_testing();

    char dir[256];
    test_fmt_tmpdir(dir, sizeof(dir), "snapshot_boot_seed", "main");
    sb_mkdir("./test-tmp");
    sb_mkdir(dir);

    /* ── Build the on-disk fixture snapshot. */
    char snap_path[600];
    /* Exercise ATTACH path handling with a valid filename that must never be
     * interpolated into SQL text. */
    snprintf(snap_path, sizeof(snap_path), "%s/consensus'_snapshot.db", dir);
    uint8_t tip_hash[32];
    sb_tip_hash(tip_hash);
    SB_CHECK("fixture: snapshot.db built", sb_build_fixture(snap_path, tip_hash));
    {
        struct stat st;
        SB_CHECK("fixture: snapshot > 10 MiB size floor",
                 stat(snap_path, &st) == 0 &&
                 st.st_size >= (off_t)(10 * 1024 * 1024));
    }

    /* ── Fresh node.db (the running node's DB the snapshot imports INTO). */
    char ndb_path[600];
    snprintf(ndb_path, sizeof(ndb_path), "%s/node.db", dir);
    struct node_db ndb;
    bool db_ok = node_db_open(&ndb, ndb_path);
    SB_CHECK("fixture: node.db opens", db_ok);
    if (!db_ok) { blocker_reset_for_testing(); return failures; }
    seed_integrity_gate_set_node_db_for_testing(&ndb);

    /* progress store + tip_finalize stage (the seed anchor writes cursors here). */
    SB_CHECK("fixture: progress_store opens", progress_store_open(dir));
    sqlite3 *pdb = progress_store_db();
    SB_CHECK("fixture: pdb handle", pdb != NULL);

    struct main_state ms;
    memset(&ms, 0, sizeof(ms));
    main_state_init(&ms);
    SB_CHECK("fixture: tip_finalize stage init", tip_finalize_stage_init(&ms));

    /* Metadata is an untrusted TEXT field. A numeric prefix with trailing
     * bytes used to parse as the prefix and drive the complete import. */
    failures += sb_test_malformed_height(&ndb, snap_path);
    char valid_height[32];
    snprintf(valid_height, sizeof(valid_height), "%d", SB_SNAP_HEIGHT);
    SB_CHECK("valid snapshot height restored",
             sb_set_snapshot_height(snap_path, valid_height));
    failures += sb_test_copy_rollback(&ndb, snap_path);
    failures += sb_test_anchor_atomicity(&ndb, snap_path);

    /* PRECONDITION (the "looks-synced-but-isn't" defect state we must escape):
     * a bare fresh datadir has NO reducer seed authority — coins_kv is NOT a
     * proven authority (no migration stamp) and coins_applied_height is ABSENT.
     * Prove that starting point so the post-state deltas are meaningful. */
    SB_CHECK("precond: coins_kv NOT proven-authority (no seed yet)",
             !coins_kv_is_proven_authority(pdb, NULL));
    {
        int32_t ca = 0; bool found = true;
        coins_kv_get_applied_height(pdb, &ca, &found);
        SB_CHECK("precond: coins_applied_height ABSENT", !found);
    }

    /* ── DRIVE THE REAL BOOT ENTRY POINT. */
    int64_t out_count = -1, out_height = -1;
    uint8_t out_best[32] = {0};
    int64_t progress_before = boot_progress_last_us();
    bool imported = boot_import_snapshot_db(&ndb, snap_path,
                                            &out_count, &out_height, out_best);
    SB_CHECK("import: boot_import_snapshot_db returns true", imported);
    SB_CHECK("import: SQLite work advances boot progress",
             boot_progress_last_us() > progress_before &&
             boot_progress_last_label() != NULL &&
             strcmp(boot_progress_last_label(),
                    "snapshot_import_bulk_insert") == 0);
    SB_CHECK("import: out_utxo_count == fixture count",
             out_count == SB_UTXO_COUNT);
    SB_CHECK("import: out_snap_height == snapshot height",
             out_height == SB_SNAP_HEIGHT);
    SB_CHECK("import: out_best_hash == fixture tip hash",
             memcmp(out_best, tip_hash, 32) == 0);

    /* ── POST-STATE: the seeded authority must be SELF-CONSISTENT. */

    /* coins_kv reseeded from the imported UTXOs + proven-authority stamp. */
    SB_CHECK("post: coins_kv count == imported UTXO count",
             coins_kv_count(pdb) == SB_UTXO_COUNT);
    SB_CHECK("post: coins_kv proven-authority (migration stamp set)",
             coins_kv_is_proven_authority(pdb, NULL));

    /* coins_applied_height at the snapshot next-height frame (NOT absent, NOT
     * stale) — the reducer's applied cursor now sits exactly one past tip. */
    {
        int32_t ca = -1; bool found = false;
        bool ok = coins_kv_get_applied_height(pdb, &ca, &found);
        SB_CHECK("post: coins_applied_height == snapshot height + 1",
                 ok && found && ca == SB_SNAP_HEIGHT + 1);
    }

    /* coins_best_block == the snapshot tip hash (height agreement:
     * coins_applied frame and the best-block cache describe the SAME tip). */
    {
        uint8_t cb[32] = {0}; size_t n = 0;
        bool ok = node_db_state_get(&ndb, "coins_best_block", cb, sizeof(cb), &n);
        SB_CHECK("post: coins_best_block == snapshot tip hash",
                 ok && n == 32 && memcmp(cb, tip_hash, 32) == 0);
    }

    /* Stage cursors clamped to the snapshot height (tip_finalize == H, the 8
     * upstream stages at H+1) — the reducer frontier now roots at the seed. */
    SB_CHECK("post: tip_finalize cursor == snapshot height (NOT H+1)",
             sb_cursor(pdb, "tip_finalize") == SB_SNAP_HEIGHT);
    SB_CHECK("post: utxo_apply cursor == snapshot height + 1",
             sb_cursor(pdb, "utxo_apply") == SB_SNAP_HEIGHT + 1);
    SB_CHECK("post: validate_headers cursor == snapshot height + 1",
             sb_cursor(pdb, "validate_headers") == SB_SNAP_HEIGHT + 1);
    SB_CHECK("post: reducer_trusted_base_height == snapshot height",
             sb_trusted_base(pdb) == SB_SNAP_HEIGHT);

    /* utxo_sha3 stamped over the imported set at the snapshot height. */
    {
        uint8_t expect_root[32]; uint64_t expect_count = 0;
        utxo_commitment_sha3_compute(ndb.db, expect_root, &expect_count);
        uint8_t got[32]; int32_t gh = -1; uint64_t gc = 0;
        bool ld = utxo_commitment_sha3_load(ndb.db, got, &gh, &gc);
        SB_CHECK("post: utxo_sha3 stamp present", ld);
        SB_CHECK("post: utxo_sha3 == recompute over imported set",
                 ld && memcmp(got, expect_root, 32) == 0 &&
                 gc == expect_count && gc == SB_UTXO_COUNT &&
                 gh == SB_SNAP_HEIGHT);
    }

    /* H* CLIMBED off the "pinned / looks-synced-but-isn't" state to the exact
     * imported base. The higher compiled checkpoint is not a floor here because
     * this short fixture's coin authority does not cover it. */
    {
        int32_t hs = 0, sf = 0;
        progress_store_tx_lock();
        bool ok = reducer_frontier_compute_hstar(pdb, &hs, &sf);
        progress_store_tx_unlock();
        SB_CHECK("post: H* climbed to the covered imported base",
                 ok && hs == SB_SNAP_HEIGHT && hs > 0);
    }

    /* NO stage named a blocker while seeding — the import is a clean advance,
     * never a silent stall or a named halt. */
    SB_CHECK("post: no active blocker after seed",
             blocker_count_active() == 0);
    failures += sb_test_epilogue_resume(&ndb, pdb, snap_path);

    tip_finalize_stage_shutdown();
    main_state_free(&ms);
    progress_store_close();
    seed_integrity_gate_reset_for_testing();
    node_db_close(&ndb);
    test_cleanup_tmpdir(dir);

    blocker_reset_for_testing();
    return failures;
}
