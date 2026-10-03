/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: round-trip the REAL snapshot serializer against the REAL fetch
 * parser. db_utxo_serialize_snapshot() writes each record's scriptPubKey
 * clamped to 520 bytes, but the per-export SHA3 commitment it also writes
 * absorbs the FULL script (engine/models/src/utxo.c snap_write_utxo) — and
 * the canonical commitment (core coins, no 520 clamp) recomputed on import
 * covers the full script too. A UTXO whose scriptPubKey is 521..10000 bytes
 * (consensus-valid; the model layer admits up to 10000) therefore produced
 * a snapshot whose commitment can never verify on import: poisoned
 * serve/import for that node and every peer. Before this group no test
 * constructed the serializer output at all (see test_snapshot_serve_loopback
 * file header). */

#include "test/test_core.h"

#include "models/utxo.h"
#include "models/database.h"
#include "coins/utxo_commitment.h"
#include "net/snapshot_sync_contract.h"
#include "script/standard.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

/* Consensus imposes no 520-byte cap on scriptPubKey (520 is the bare-
 * multisig standardness rule). Pick 600: over the export clamp, under the
 * model ceiling, and not mistaken for any special length. */
#define SSR_BIG_SCRIPT_LEN 600u

static void ssr_fill_utxo(struct db_utxo *u, uint8_t *script)
{
    memset(u, 0, sizeof(*u));
    memset(u->txid, 0x5A, sizeof(u->txid));
    u->vout = 1;
    u->value = 1000;
    script[0] = 0x76; /* plausible-ish leading op, rest deterministic */
    for (size_t i = 1; i < SSR_BIG_SCRIPT_LEN; i++)
        script[i] = (uint8_t)(0x30 + (i & 0x3F));
    u->script = script;
    u->script_len = SSR_BIG_SCRIPT_LEN;
    u->script_type = SCRIPT_OTHER;
    u->height = 100;
    u->is_coinbase = false;
}

static int ssr_staging_script_len(struct node_db *ndb)
{
    sqlite3_stmt *st = NULL;
    int len = -1;
    if (sqlite3_prepare_v2(ndb->db,
            "SELECT length(script) FROM snapshot_staging_utxos", -1, &st,
            NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        len = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return len;
}

static int test_ssr_big_script_roundtrips(void)
{
    int failures = 0;
    TEST("snapshot: a 600-byte scriptPubKey survives serialize-apply-"
         "commitment") {
        char dir[256], path[512];
        test_make_tmpdir(dir, sizeof(dir), "snap520", "roundtrip");
        int n = snprintf(path, sizeof(path), "%s/utxos.snap", dir);
        ASSERT(n > 0 && (size_t)n < sizeof(path));

        struct node_db src;
        memset(&src, 0, sizeof(src));
        ASSERT(node_db_open(&src, ":memory:"));
        struct db_utxo u;
        uint8_t script[SSR_BIG_SCRIPT_LEN];
        ssr_fill_utxo(&u, script);
        ASSERT(db_utxo_save(&src, &u));

        uint8_t exported_root[32];
        ASSERT(db_utxo_serialize_snapshot(&src, path, 500, exported_root) == 1);

        /* Feed the file's chunks through the production fetch parser. */
        FILE *fp = fopen(path, "rb");
        ASSERT(fp != NULL);
        uint8_t chunk[4096];
        size_t got = fread(chunk, 1, sizeof(chunk), fp);
        fclose(fp);
        ASSERT(got > 4);

        struct node_db dst;
        memset(&dst, 0, sizeof(dst));
        ASSERT(node_db_open(&dst, ":memory:"));
        struct snapshot_sync_service svc;
        snapsync_init(&svc, &dst);
        svc.state = SNAPSYNC_RECEIVING;
        ASSERT(snapsync_apply_chunk(&svc, chunk, got) == 1);

        /* RED pre-fix: the record carried only 520 bytes. */
        ASSERT_EQ(ssr_staging_script_len(&dst), (int)SSR_BIG_SCRIPT_LEN);

        /* The import-time recomputation must reproduce the export
         * commitment — this is the divergence that poisoned snapshots. */
        uint8_t src_root[32], dst_root[32];
        uint64_t src_cnt = 0, dst_cnt = 0;
        utxo_commitment_sha3_compute(src.db, src_root, &src_cnt);
        utxo_commitment_sha3_compute_table(dst.db,
                                           "snapshot_staging_utxos",
                                           dst_root, &dst_cnt);
        ASSERT(src_cnt == 1 && dst_cnt == 1);
        ASSERT(memcmp(src_root, exported_root, 32) == 0);
        ASSERT(memcmp(src_root, dst_root, 32) == 0);

        node_db_close(&dst);
        node_db_close(&src);
        test_rm_rf_recursive(dir);
        PASS();
    } _test_next:;
    return failures;
}

int test_snapshot_script_roundtrip(void)
{
    int failures = 0;
    failures += test_ssr_big_script_roundtrips();
    printf("=== snapshot_script_roundtrip: %d failures ===\n", failures);
    return failures;
}
