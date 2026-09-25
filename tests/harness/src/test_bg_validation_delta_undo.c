/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * test_bg_validation_delta_undo — blocks this node connects itself must be
 * fully script-verifiable by background validation.
 *
 * The real utxo_apply stage folds a short chain of mined regtest blocks
 * (coinbase, a two-input spend of outside coins, and a spend of an output
 * created earlier in the same block). Its co-committed utxo_apply_delta row
 * is the only undo record such a block ever gets, so the group proves:
 *   - every applied height yields a block_undo naming exactly the spent
 *     outputs, and that undo survives the rev-format serialize round trip;
 *   - the background verifier checks every transparent script of those
 *     blocks with zero undo-missing skips, against the delta's scripts;
 *   - a row for another block hash, or one that does not match the block's
 *     inputs, gives no undo (fail closed: counted as skips, never trusted);
 *   - a kill -9 at any commit of the fold leaves every applied height with
 *     its undo and no unapplied height with one, and the restarted fold
 *     completes the record. */

#include "test/test_core.h"
#include "test/block_fixtures.h"

#include "bloom/merkle.h"
#include "chain/chain.h"
#include "chain/chainparams.h"
#include "coins/undo.h"
#include "core/arith_uint256.h"
#include "core/serialize.h"
#include "jobs/utxo_apply_delta.h"
#include "jobs/utxo_apply_delta_undo.h"
#include "jobs/utxo_apply_stage.h"
#include "mining/miner.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "services/bg_validation_service.h"
#include "storage/progress_store.h"
#include "storage/projection_store.h"
#include "util/blocker.h"
#include "util/safe_alloc.h"
#include "util/util.h"
#include "validation/chainstate.h"
#include "validation/main_state.h"

#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif
#include <unistd.h>

/* Sibling-private production verifier (engine/services/src). */
bool bg_validation_validate_block_proofs(const struct block *block,
                                         struct block_index *pindex,
                                         const char *datadir,
                                         const struct chain_params *params,
                                         int num_workers,
                                         size_t max_script_batch,
                                         int64_t *sigs_out,
                                         int64_t *proofs_out,
                                         int64_t *skips_out);

#define DU_BLOCKS 3
#define DU_OUTSIDE_VALUE 2000
#define DU_INNER_VALUE 1500
#define DU_OP_TRUE 0x51
#define DU_OP_FALSE 0x00

struct du_chain {
    struct block_index idx[DU_BLOCKS];
    struct uint256 hash[DU_BLOCKS];
    struct block body[DU_BLOCKS];
    uint8_t outside_script;  /* scriptPubKey byte the lookup reports */
};

static void du_outside_txid(struct uint256 *out, int h, int which)
{
    memset(out->data, 0x5a, sizeof(out->data));
    out->data[0] = (uint8_t)h;
    out->data[1] = (uint8_t)which;
}

static bool du_tx(struct transaction *tx, size_t nin, int64_t out_value)
{
    transaction_init(tx);
    if (!transaction_alloc(tx, nin, 1))
        return false;
    for (size_t i = 0; i < nin; i++)
        tx->vin[i].sequence = UINT32_MAX;
    tx->vout[0].value = out_value;
    tx->vout[0].script_pub_key.data[0] = DU_OP_TRUE;
    tx->vout[0].script_pub_key.size = 1;
    return true;
}

/* coinbase | tx1 spends two outside coins | tx2 spends tx1:0 in-block. */
static bool du_block_build(struct block *blk, int h,
                           const struct chain_params *cp)
{
    block_init(blk);
    blk->vtx = zcl_calloc(3, sizeof(*blk->vtx), "du_vtx");
    if (!blk->vtx)
        return false;
    blk->num_vtx = 3;
    if (!du_tx(&blk->vtx[0], 1, 1000 + h) ||
        !du_tx(&blk->vtx[1], 2, DU_INNER_VALUE) ||
        !du_tx(&blk->vtx[2], 1, 1000))
        return false;
    outpoint_set_null(&blk->vtx[0].vin[0].prevout);
    for (uint32_t i = 0; i < 2; i++) {
        du_outside_txid(&blk->vtx[1].vin[i].prevout.hash, h, (int)i);
        blk->vtx[1].vin[i].prevout.n = i;
    }
    transaction_compute_hash(&blk->vtx[0]);
    transaction_compute_hash(&blk->vtx[1]);
    blk->vtx[2].vin[0].prevout.hash = blk->vtx[1].hash;
    blk->vtx[2].vin[0].prevout.n = 0;
    transaction_compute_hash(&blk->vtx[2]);

    struct uint256 txids[3] = {
        blk->vtx[0].hash, blk->vtx[1].hash, blk->vtx[2].hash };
    blk->header.nVersion = 4;
    blk->header.hashMerkleRoot = compute_merkle_root(txids, 3);
    uint256_set_null(&blk->header.hashPrevBlock);
    uint256_set_null(&blk->header.hashFinalSaplingRoot);
    blk->header.nTime = 1600000000u + (uint32_t)h;
    struct arith_uint256 pow_limit;
    uint256_to_arith(&pow_limit, &cp->consensus.powLimit);
    blk->header.nBits = arith_uint256_get_compact(&pow_limit, false);
    return mine_block_pow(blk, h, cp, 0);
}

static bool du_chain_build(struct du_chain *c, const struct chain_params *cp)
{
    memset(c, 0, sizeof(*c));
    c->outside_script = DU_OP_TRUE;
    for (int h = 0; h < DU_BLOCKS; h++) {
        if (!du_block_build(&c->body[h], h, cp))
            return false;
        block_get_hash(&c->body[h], &c->hash[h]);
        struct block_index *bi = &c->idx[h];
        block_index_init(bi);
        bi->phashBlock = &c->hash[h];
        bi->hashMerkleRoot = c->body[h].header.hashMerkleRoot;
        bi->nHeight = h;
        bi->nVersion = c->body[h].header.nVersion;
        bi->nTime = c->body[h].header.nTime;
        bi->nBits = c->body[h].header.nBits;
        bi->nStatus = BLOCK_HAVE_DATA;
        if (h > 0)
            bi->pprev = &c->idx[h - 1];
    }
    return true;
}

static void du_chain_free(struct du_chain *c)
{
    for (int h = 0; h < DU_BLOCKS; h++)
        block_free(&c->body[h]);
}

static bool du_reader(struct block *out, const struct block_index *bi,
                      const char *datadir, void *user)
{
    (void)datadir;
    struct du_chain *c = user;
    if (!out || !bi || !c || bi->nHeight < 0 || bi->nHeight >= DU_BLOCKS)
        return false;
    return test_block_copy(out, &c->body[bi->nHeight], "du_read");
}

static bool du_lookup(const struct uint256 *txid, uint32_t vout,
                      struct utxo_apply_lookup *out, void *user)
{
    struct du_chain *c = user;
    memset(out, 0, sizeof(*out));
    for (int h = 0; h < DU_BLOCKS; h++) {
        for (int which = 0; which < 2; which++) {
            struct uint256 want;
            du_outside_txid(&want, h, which);
            if (uint256_eq(&want, txid) && vout == (uint32_t)which) {
                out->found = true;
                out->value = DU_OUTSIDE_VALUE;
                out->height = 0;
                out->script_len = 1;
                out->script[0] = c->outside_script;
                return true;
            }
        }
    }
    return true;
}

static bool du_exec(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) {
        printf("[du] sql failed: %s\n", err);
        sqlite3_free(err);
    }
    return rc == SQLITE_OK;
}

/* Upstream verdicts the stage requires before it folds a height. */
static bool du_seed_upstream(sqlite3 *db, const struct du_chain *c)
{
    if (!du_exec(db,
        "CREATE TABLE IF NOT EXISTS proof_validate_log ("
        "  height INTEGER PRIMARY KEY, status TEXT NOT NULL,"
        "  ok INTEGER NOT NULL, sapling_spends_total INTEGER NOT NULL,"
        "  sapling_outputs_total INTEGER NOT NULL,"
        "  sprout_joinsplits_total INTEGER NOT NULL, block_hash BLOB,"
        "  first_failure_txid BLOB, first_failure_proof_type TEXT,"
        "  validated_at INTEGER NOT NULL)"))
        return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO proof_validate_log (height, status, ok, "
            "sapling_spends_total, sapling_outputs_total, "
            "sprout_joinsplits_total, block_hash, validated_at) "
            "VALUES (?, 'verified', 1, 0, 0, 0, ?, 1)",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    bool ok = true;
    for (int h = 0; ok && h < DU_BLOCKS; h++) {
        sqlite3_bind_int(st, 1, h);
        sqlite3_bind_blob(st, 2, c->hash[h].data, 32, SQLITE_STATIC);
        ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    char sql[160];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO stage_cursor(name, cursor, updated_at) "
             "VALUES('proof_validate', %d, 1)", DU_BLOCKS);
    return ok && du_exec(db, sql);
}

static bool du_seed_script_verdicts(sqlite3 *db, const struct du_chain *c)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO script_validate_log (height, status, ok, "
            "tx_count, input_count, validated_at, block_hash) "
            "VALUES (?, 'verified', 1, 3, 3, 1, ?)",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    bool ok = true;
    for (int h = 0; ok && h < DU_BLOCKS; h++) {
        sqlite3_bind_int(st, 1, h);
        sqlite3_bind_blob(st, 2, c->hash[h].data, 32, SQLITE_STATIC);
        ok = sqlite3_step(st) == SQLITE_DONE;  // raw-sql-ok:test-fixture-seeding
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    return ok;
}

/* Open the datadir's stores and arm the real utxo_apply stage over `c`.
 * Opening an existing datadir runs the stage's own startup recovery. */
static bool du_open(const char *dir, struct main_state *ms, struct du_chain *c)
{
    SetDataDir(dir);
    if (!progress_store_open(dir) || !projection_store_open(dir))
        return false;
    memset(ms, 0, sizeof(*ms));
    active_chain_init(&ms->chain_active);
    block_map_init(&ms->map_block_index);
    active_chain_move_window_tip(&ms->chain_active, &c->idx[DU_BLOCKS - 1]);
    if (!du_seed_upstream(progress_store_db(), c) ||
        !utxo_apply_stage_init(ms) ||
        !du_seed_script_verdicts(progress_store_db(), c))
        return false;
    utxo_apply_stage_set_reader(du_reader, c);
    utxo_apply_stage_set_lookup(du_lookup, c);
    return true;
}

static void du_close(struct main_state *ms)
{
    utxo_apply_stage_shutdown();
    active_chain_free(&ms->chain_active);
    block_map_free(&ms->map_block_index);
    projection_store_close();
    progress_store_close();
    SetDataDir("");
    ClearDataDirCache();
}

static bool du_script_is(const struct script *s, uint8_t op)
{
    return s->size == 1 && s->data[0] == op;
}

/* The undo the fold must have recorded for height h of `c`. */
static bool du_undo_matches(const struct block_undo *u, int h)
{
    if (u->num_txundo != 2 || u->vtxundo[0].num_prevout != 2 ||
        u->vtxundo[1].num_prevout != 1)
        return false;
    for (size_t i = 0; i < 2; i++) {
        const struct tx_in_undo *in = &u->vtxundo[0].vprevout[i];
        if (in->txout.value != DU_OUTSIDE_VALUE || in->height != 0 ||
            in->coinbase || !du_script_is(&in->txout.script_pub_key,
                                          DU_OP_TRUE))
            return false;
    }
    const struct tx_in_undo *inner = &u->vtxundo[1].vprevout[0];
    return inner->txout.value == DU_INNER_VALUE &&
           inner->height == (unsigned)h && !inner->coinbase &&
           du_script_is(&inner->txout.script_pub_key, DU_OP_TRUE);
}

/* The rev-file encoding of the reconstructed undo must round-trip. */
static bool du_undo_roundtrips(const struct block_undo *u, int h)
{
    struct byte_stream s;
    stream_init(&s, 0);
    bool ok = block_undo_serialize(u, &s);
    struct block_undo back;
    block_undo_init(&back);
    if (ok) {
        struct byte_stream r;
        stream_init_from_data(&r, s.data, s.size);
        ok = block_undo_deserialize(&back, &r) && du_undo_matches(&back, h);
    }
    block_undo_free(&back);
    stream_free(&s);
    return ok;
}

static enum utxo_apply_delta_undo_status du_load(
    const struct du_chain *c, int h, struct block_undo *out)
{
    return utxo_apply_delta_block_undo_load(progress_store_db(), h,
                                            &c->hash[h], &c->body[h], out);
}

/* A background-validation index for height h: same identity, no parent
 * (the contextual header rules are not what this group exercises), and no
 * rev position, exactly like a block this node connected itself. */
static void du_bg_index(struct block_index *bi, struct du_chain *c, int h)
{
    block_index_init(bi);
    bi->phashBlock = &c->hash[h];
    bi->nHeight = h;
    bi->nStatus = BLOCK_VALID_SCRIPTS | BLOCK_HAVE_DATA;
    bi->nFile = 0;
    bi->nDataPos = 8;
}

/* One folded chain shared by the forward-record cases, in order. */
struct du_fixture {
    const struct chain_params *cp;
    char dir[256];
    struct du_chain c;
    struct main_state ms;
    bool built;
    bool opened;
};

static int du_case_record(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: every folded height carries an exact undo record") {
        fx->built = du_chain_build(&fx->c, fx->cp);
        ASSERT(fx->built);
        fx->opened = du_open(fx->dir, &fx->ms, &fx->c);
        ASSERT(fx->opened);
        ASSERT_EQ(utxo_apply_stage_drain(100), DU_BLOCKS);
        ASSERT_EQ(utxo_apply_stage_cursor(), (uint64_t)DU_BLOCKS);
        for (int h = 0; h < DU_BLOCKS; h++) {
            struct block_undo u;
            ASSERT_EQ(du_load(&fx->c, h, &u), UTXO_DELTA_UNDO_FOUND);
            bool exact = du_undo_matches(&u, h);
            bool trip = du_undo_roundtrips(&u, h);
            block_undo_free(&u);
            ASSERT(exact);
            ASSERT(trip);
        }
        PASS();
    } _test_next:;
    return failures;
}

static int du_case_bg_zero_skips(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: bg validation checks every script, zero skips") {
        ASSERT(fx->opened);
        uint64_t from_delta = bg_validation_undo_from_delta_blocks();
        bg_validation_reset_undo_skip_stats();
        for (int h = 0; h < DU_BLOCKS; h++) {
            struct block_index bi;
            du_bg_index(&bi, &fx->c, h);
            int64_t sigs = 0, proofs = 0, skips = 0;
            ASSERT(bg_validation_validate_block_proofs(
                &fx->c.body[h], &bi, fx->dir, fx->cp, 1, 0,
                &sigs, &proofs, &skips));
            ASSERT_EQ(skips, 0);
            ASSERT_EQ(sigs, 3);
        }
        ASSERT_EQ(bg_validation_undo_from_delta_blocks() - from_delta,
                  (uint64_t)DU_BLOCKS);
        ASSERT_EQ(bg_validation_get_undo_skip_stats().blocks, 0);
        PASS();
    } _test_next:;
    return failures;
}

static int du_case_delta_script(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: the delta's scriptPubKey is what gets verified") {
        ASSERT(fx->opened);
        /* Re-author height 0's row as if the spent coins had been locked by
         * OP_FALSE. Present undo must now FAIL the block, not skip it. */
        fx->c.outside_script = DU_OP_FALSE;
        struct delta_summary s;
        memset(&s, 0, sizeof(s));
        utxo_apply_compute_block_delta(&fx->c.body[0], 0, du_lookup, &fx->c,
                                       &s);
        bool computed = s.ok;
        bool persisted = computed &&
            utxo_apply_delta_persist(progress_store_db(), 0, &fx->c.hash[0],
                                     &s);
        if (computed)
            free_delta(&s);
        fx->c.outside_script = DU_OP_TRUE;
        ASSERT(persisted);
        struct block_index bi;
        du_bg_index(&bi, &fx->c, 0);
        int64_t sigs = 0, proofs = 0, skips = 0;
        ASSERT(!bg_validation_validate_block_proofs(
            &fx->c.body[0], &bi, fx->dir, fx->cp, 1, 0,
            &sigs, &proofs, &skips));
        ASSERT_EQ(skips, 0);
        PASS();
    } _test_next:;
    return failures;
}

static int du_case_other_branch(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: a row for another block hash gives no undo") {
        ASSERT(fx->opened);
        ASSERT(du_exec(progress_store_db(),
            "UPDATE utxo_apply_delta SET branch_hash=zeroblob(32) "
            "WHERE height=1"));
        struct block_undo u;
        ASSERT_EQ(du_load(&fx->c, 1, &u), UTXO_DELTA_UNDO_OTHER_BRANCH);
        ASSERT_EQ(u.num_txundo, 0);
        struct block_index bi;
        du_bg_index(&bi, &fx->c, 1);
        bg_validation_reset_undo_skip_stats();
        int64_t sigs = 0, proofs = 0, skips = 0;
        ASSERT(bg_validation_validate_block_proofs(
            &fx->c.body[1], &bi, fx->dir, fx->cp, 1, 0,
            &sigs, &proofs, &skips));
        ASSERT_EQ(skips, 2);
        ASSERT_EQ(sigs, 0);
        ASSERT_EQ(bg_validation_get_undo_skip_stats().blocks, 1);
        bg_validation_reset_undo_skip_stats();
        PASS();
    } _test_next:;
    return failures;
}

static int du_case_input_mismatch(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: a row that does not name the block's inputs is refused") {
        ASSERT(fx->opened);
        struct block other;
        ASSERT(test_block_copy(&other, &fx->c.body[2], "du_other"));
        other.vtx[1].vin[1].prevout.n = 7;
        struct block_undo u;
        enum utxo_apply_delta_undo_status st =
            utxo_apply_delta_block_undo_load(progress_store_db(), 2,
                                             &fx->c.hash[2], &other, &u);
        other.vtx[1].vin[1].prevout.n = 1;
        /* One tx fewer than the row records: its entry is left over. */
        other.num_vtx = 2;
        struct block_undo u2;
        enum utxo_apply_delta_undo_status st2 =
            utxo_apply_delta_block_undo_load(progress_store_db(), 2,
                                             &fx->c.hash[2], &other, &u2);
        other.num_vtx = 3;
        block_free(&other);
        struct block_undo u3;
        enum utxo_apply_delta_undo_status st3 =
            utxo_apply_delta_block_undo_load(progress_store_db(),
                                             DU_BLOCKS + 5, &fx->c.hash[0],
                                             &fx->c.body[0], &u3);
        ASSERT_EQ(st, UTXO_DELTA_UNDO_MISMATCH);
        ASSERT_EQ(u.num_txundo, 0);
        ASSERT_EQ(st2, UTXO_DELTA_UNDO_MISMATCH);
        ASSERT_EQ(u2.num_txundo, 0);
        ASSERT_EQ(st3, UTXO_DELTA_UNDO_ABSENT);
        PASS();
    } _test_next:;
    return failures;
}

static int du_case_unfolded_store(struct du_fixture *fx)
{
    int failures = 0;
    TEST("delta_undo: a store the fold never touched has no undo") {
        ASSERT(fx->opened);
        sqlite3 *mem = NULL;
        ASSERT(sqlite3_open(":memory:", &mem) == SQLITE_OK);
        struct block_undo u;
        enum utxo_apply_delta_undo_status st =
            utxo_apply_delta_block_undo_load(mem, 0, &fx->c.hash[0],
                                             &fx->c.body[0], &u);
        sqlite3_close(mem);
        uint64_t next = 7;
        bool found = false;
        ASSERT_EQ(st, UTXO_DELTA_UNDO_ABSENT);
        ASSERT(utxo_apply_delta_undo_next_unapplied(progress_store_db(),
                                                    &next, &found));
        ASSERT(found);
        ASSERT_EQ(next, (uint64_t)DU_BLOCKS);
        PASS();
    } _test_next:;
    return failures;
}

static int test_delta_undo_forward_record(const struct chain_params *cp)
{
    struct du_fixture fx;
    memset(&fx, 0, sizeof(fx));
    fx.cp = cp;
    test_make_tmpdir(fx.dir, sizeof(fx.dir), "delta_undo", "forward");
    int failures = du_case_record(&fx);
    failures += du_case_bg_zero_skips(&fx);
    failures += du_case_delta_script(&fx);
    failures += du_case_other_branch(&fx);
    failures += du_case_input_mismatch(&fx);
    failures += du_case_unfolded_store(&fx);
    if (fx.opened)
        du_close(&fx.ms);
    if (fx.built)
        du_chain_free(&fx.c);
    (void)test_rm_rf_recursive(fx.dir);
    return failures;
}

#if !defined(_WIN32)
/* Die, as kill -9 would, on the Nth commit of the store connection: the
 * hook runs before the commit record reaches the WAL, so the transaction
 * that was about to land is lost exactly as in a power cut. */
static int g_du_commit_countdown;

static int du_die_on_commit(void *user)
{
    (void)user;
    if (--g_du_commit_countdown == 0)
        raise(SIGKILL);
    return 0;
}

static void du_child_fold(const char *dir, const struct chain_params *cp,
                          int die_at_commit)
{
    struct du_chain c;
    struct main_state ms;
    if (!du_chain_build(&c, cp) || !du_open(dir, &ms, &c))
        _exit(3);
    g_du_commit_countdown = die_at_commit;
    sqlite3_commit_hook(progress_store_db(), du_die_on_commit, NULL);
    /* One height per drain: each block is its own commit, so the crash
     * points fall between blocks as well as inside one. */
    for (int i = 0; i < DU_BLOCKS; i++)
        (void)utxo_apply_stage_drain(1);
    sqlite3_commit_hook(progress_store_db(), NULL, NULL);
    _exit(utxo_apply_stage_cursor() == DU_BLOCKS ? 0 : 4);
}

/* After a crash: heights below the durable cursor have their exact undo,
 * heights at/above it have none. */
static bool du_crash_invariant(const struct du_chain *c, uint64_t *next_out)
{
    bool found = false;
    if (!utxo_apply_delta_undo_next_unapplied(progress_store_db(), next_out,
                                              &found))
        return false;
    uint64_t next = found ? *next_out : 0;
    for (int h = 0; h < DU_BLOCKS; h++) {
        struct block_undo u;
        enum utxo_apply_delta_undo_status st = du_load(c, h, &u);
        bool exact = st == UTXO_DELTA_UNDO_FOUND && du_undo_matches(&u, h);
        block_undo_free(&u);
        if ((uint64_t)h < next ? !exact : st != UTXO_DELTA_UNDO_ABSENT) {
            printf("[du] h=%d next=%llu status=%s\n", h,
                   (unsigned long long)next,
                   utxo_apply_delta_undo_status_name(st));
            return false;
        }
    }
    return true;
}

static int test_delta_undo_kill9(const struct chain_params *cp)
{
    int failures = 0;

    TEST("delta_undo: kill -9 at every fold commit keeps undo exact") {
        int killed = 0;
        unsigned crash_fronts = 0;  /* durable cursors seen after a kill */
        bool completed = false;
        for (int k = 1; k <= 64 && !completed; k++) {
            char dir[256];
            char tag[32];
            snprintf(tag, sizeof(tag), "kill9_%d", k);
            test_make_tmpdir(dir, sizeof(dir), "delta_undo", tag);
            fflush(NULL);
            pid_t pid = fork();
            ASSERT(pid >= 0);
            if (pid == 0)
                du_child_fold(dir, cp, k);
            int status = 0;
            ASSERT(waitpid(pid, &status, 0) == pid);
            bool died = WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
            completed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            ASSERT(died || completed);
            killed += died ? 1 : 0;

            struct du_chain c;
            struct main_state ms;
            ASSERT(du_chain_build(&c, cp));
            bool opened = du_open(dir, &ms, &c);
            uint64_t next = 0;
            bool before = opened && du_crash_invariant(&c, &next);
            if (died && before && next < 32)
                crash_fronts |= 1u << next;
            int advanced = opened ? utxo_apply_stage_drain(100) : -1;
            bool after = opened && du_crash_invariant(&c, &next) &&
                         next == DU_BLOCKS;
            if (opened)
                du_close(&ms);
            du_chain_free(&c);
            (void)test_rm_rf_recursive(dir);
            ASSERT(opened);
            ASSERT(before);
            ASSERT(advanced >= 0);
            ASSERT(after);
        }
        /* The loop must have crossed every commit and then run clean. */
        ASSERT(completed);
        ASSERT(killed >= DU_BLOCKS);
        /* ...and a kill landed with each of 0, 1 and 2 blocks durable. */
        ASSERT_EQ(crash_fronts & ((1u << DU_BLOCKS) - 1u),
                  (1u << DU_BLOCKS) - 1u);
        printf("(%d crash points) ", killed);
        PASS();
    } _test_next:;
    return failures;
}
#else
static int test_delta_undo_kill9(const struct chain_params *cp)
{
    (void)cp;
    printf("delta_undo: kill -9 SKIP (Windows has no fork/SIGKILL)\n");
    return 0;
}
#endif

int test_bg_validation_delta_undo(void)
{
    printf("\n=== bg_validation delta undo ===\n");
    int failures = 0;
    blocker_module_init();
    chain_params_select(CHAIN_REGTEST);
    const struct chain_params *cp = chain_params_get();
    if (!cp) {
        printf("delta_undo: regtest params unavailable... FAIL\n");
        chain_params_select(CHAIN_MAIN);
        return 1;
    }
    failures += test_delta_undo_forward_record(cp);
    failures += test_delta_undo_kill9(cp);
    chain_params_select(CHAIN_MAIN);
    return failures;
}
