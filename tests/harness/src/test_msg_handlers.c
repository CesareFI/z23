/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Tests for the split message handler files:
 *   msg_version.c, msg_headers.c, msg_blocks.c, msg_tx.c, msg_compact.c
 *
 * These tests exercise the public/testable functions without requiring
 * a full msg_processor or live P2P connection. Coverage:
 *   1. msg_headers_get_stats — NULL safety, initial zeroed state
 *   2. block_already_seen / block_mark_seen / block_clear_seen — dedup ring
 *   3. tx_already_seen / tx_mark_seen — tx dedup ring
 *   4. Dandelion globals — initial state
 */

#include "test/test_core.h"
#include "chain/chain.h"
#include "chain/chainparams.h"
#include "core/hash.h"
#include "mining/miner.h"
#include "net/compact_blocks.h"
#include "net/msgprocessor.h"
#include "net/msg_internal.h"
#include "net/download.h"
#include "net/peer_scoring.h"
#include "platform/socket_compat.h"
#include "consensus/validation.h"
#include "core/uint256.h"
#include "primitives/block.h"
#include "sync/sync_state.h"
#include "util/safe_alloc.h"
#include "util/blocker.h"
#include "util/thread_registry.h"
#include "validation/main_state.h"
#include "validation/txmempool.h"

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void test_msg_sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void test_msg_sync_to_idle(void)
{
    enum sync_state cur = sync_get_state();
    if (cur == SYNC_IDLE)
        return;
    if (cur == SYNC_AT_TIP) {
        (void)sync_set_state(SYNC_IDLE, "msg_handlers cleanup");
        return;
    }
    if (cur == SYNC_REORG) {
        (void)sync_set_state(SYNC_AT_TIP, "msg_handlers cleanup");
        (void)sync_set_state(SYNC_IDLE, "msg_handlers cleanup");
        return;
    }
    (void)sync_set_state(SYNC_IDLE, "msg_handlers cleanup");
}

static void test_msg_sync_to_blocks_download(void)
{
    test_msg_sync_to_idle();
    if (sync_get_state() != SYNC_IDLE)
        return;
    (void)sync_set_state(SYNC_FINDING_PEERS, "msg_handlers setup");
    (void)sync_set_state(SYNC_HEADERS_DOWNLOAD, "msg_handlers setup");
    (void)sync_set_state(SYNC_BLOCKS_DOWNLOAD, "msg_handlers setup");
}

/* ── msg_headers.c tests ───────────────────────────────────────── */

static int test_headers_stats_null_safe(void)
{
    int failures = 0;
    TEST("msg_handlers: msg_headers_get_stats(NULL) does not crash") {
        msg_headers_get_stats(NULL);
        ASSERT(true);  /* survived NULL arg without crashing */
        PASS();
    } _test_next:;
    return failures;
}

static int test_headers_stats_initial(void)
{
    int failures = 0;
    TEST("msg_handlers: msg_headers_get_stats returns zeroed counters initially") {
        struct msg_headers_stats st;
        memset(&st, 0xFF, sizeof(st));
        msg_headers_get_stats(&st);
        /* Counters start at zero (atomics initialized to 0). */
        ASSERT(st.batches_received == 0);
        ASSERT(st.total_accepted == 0);
        ASSERT(st.total_rejected == 0);
        ASSERT(st.already_known == 0);
        PASS();
    } _test_next:;
    return failures;
}

/* ── msgprocessor.c dedup ring buffer tests ─────────────────────── */

static struct uint256 make_test_hash(uint8_t seed)
{
    struct uint256 h;
    memset(h.data, seed, 32);
    return h;
}

static int test_block_dedup_basic(void)
{
    int failures = 0;
    TEST("msg_handlers: block dedup — unseen hash returns false") {
        struct uint256 h = make_test_hash(0xAA);
        ASSERT(!block_already_seen(&h));
        PASS();
    } _test_next:;
    return failures;
}

static int test_block_dedup_mark_and_check(void)
{
    int failures = 0;
    TEST("msg_handlers: block dedup — mark then check returns true") {
        struct uint256 h = make_test_hash(0xBB);
        block_mark_seen(&h);
        ASSERT(block_already_seen(&h));
        PASS();
    } _test_next:;
    return failures;
}

static int test_block_dedup_clear(void)
{
    int failures = 0;
    TEST("msg_handlers: block dedup — clear removes hash") {
        struct uint256 h = make_test_hash(0xCC);
        block_mark_seen(&h);
        ASSERT(block_already_seen(&h));
        block_clear_seen(&h);
        ASSERT(!block_already_seen(&h));
        PASS();
    } _test_next:;
    return failures;
}

static int test_block_dedup_multiple(void)
{
    int failures = 0;
    TEST("msg_handlers: block dedup — multiple hashes tracked independently") {
        struct uint256 h1 = make_test_hash(0xD1);
        struct uint256 h2 = make_test_hash(0xD2);
        struct uint256 h3 = make_test_hash(0xD3);
        block_mark_seen(&h1);
        block_mark_seen(&h2);
        ASSERT(block_already_seen(&h1));
        ASSERT(block_already_seen(&h2));
        ASSERT(!block_already_seen(&h3));
        PASS();
    } _test_next:;
    return failures;
}

static int test_tx_dedup_basic(void)
{
    int failures = 0;
    TEST("msg_handlers: tx dedup — unseen tx returns false") {
        struct uint256 h = make_test_hash(0xE1);
        ASSERT(!tx_already_seen(&h));
        PASS();
    } _test_next:;
    return failures;
}

static int test_tx_dedup_mark_and_check(void)
{
    int failures = 0;
    TEST("msg_handlers: tx dedup — mark then check returns true") {
        struct uint256 h = make_test_hash(0xE2);
        tx_mark_seen(&h);
        ASSERT(tx_already_seen(&h));
        PASS();
    } _test_next:;
    return failures;
}

/* ── Dandelion state tests ─────────────────────────────────────── */

static int test_dandelion_initial_state(void)
{
    int failures = 0;
    TEST("msg_handlers: dandelion not initialized at start") {
        /* g_dandelion_init should be false before any peer handshake */
        ASSERT(!g_dandelion_init);
        PASS();
    } _test_next:;
    return failures;
}

/* ── msg_blocks_should_mark_seen tests ───────────────────────
 *
 * mark_seen is gated on "block reached active chain" via
 * msg_blocks_should_mark_seen(), a pure function: a block that was indexed
 * but not activated (e.g. ACTIVATION_SKIP_ALREADY_RUNNING) must stay
 * retryable. */

static int test_p148_should_mark_seen_rejects_null(void)
{
    int failures = 0;
    TEST("should_mark_seen rejects NULL chain or pindex") {
        struct active_chain ac;
        active_chain_init(&ac);
        struct block_index bi;
        block_index_init(&bi);

        ASSERT(!msg_blocks_should_mark_seen(NULL, &bi));
        ASSERT(!msg_blocks_should_mark_seen(&ac, NULL));
        ASSERT(!msg_blocks_should_mark_seen(NULL, NULL));

        active_chain_free(&ac);
        PASS();
    } _test_next:;
    return failures;
}

static int test_p148_should_mark_seen_rejects_orphan(void)
{
    int failures = 0;
    TEST("should_mark_seen rejects block NOT in active chain") {
        /* Block was indexed but activation SKIP'd, so it is not in the active
         * chain and must NOT be marked seen (the dedup ring would hide retries). */
        struct active_chain ac;
        active_chain_init(&ac);

        struct block_index tip;
        block_index_init(&tip);
        tip.nHeight = 100;
        active_chain_move_window_tip(&ac, &tip);

        struct block_index orphan;
        block_index_init(&orphan);
        orphan.nHeight = 101; /* indexed above tip, not connected */

        ASSERT(!msg_blocks_should_mark_seen(&ac, &orphan));

        active_chain_free(&ac);
        PASS();
    } _test_next:;
    return failures;
}

static int test_p148_should_mark_seen_accepts_active(void)
{
    int failures = 0;
    TEST("should_mark_seen accepts block that IS in active chain") {
        struct active_chain ac;
        active_chain_init(&ac);

        struct block_index tip;
        block_index_init(&tip);
        tip.nHeight = 42;
        active_chain_move_window_tip(&ac, &tip);

        ASSERT(msg_blocks_should_mark_seen(&ac, &tip));

        active_chain_free(&ac);
        PASS();
    } _test_next:;
    return failures;
}

static int test_should_announce_getblocks(void)
{
    int failures = 0;
    TEST("getblocks announce requires HAVE_DATA and a hash") {
        struct block_index bi;
        struct uint256 hash;
        uint256_set_null(&hash);
        hash.data[0] = 1;
        block_index_init(&bi);
        ASSERT(!msg_blocks_should_announce_getblocks(NULL));
        ASSERT(!msg_blocks_should_announce_getblocks(&bi));
        bi.phashBlock = &hash;
        ASSERT(!msg_blocks_should_announce_getblocks(&bi));
        bi.nStatus = BLOCK_HAVE_DATA;
        ASSERT(msg_blocks_should_announce_getblocks(&bi));
        bi.nStatus = BLOCK_VALID_TREE;
        ASSERT(!msg_blocks_should_announce_getblocks(&bi));
        PASS();
    } _test_next:;
    return failures;
}

static int test_source_header_echo_policy(void)
{
    int failures = 0;
    TEST("msg_handlers: source gets only its negotiated header proof") {
        struct p2p_node peer;
        memset(&peer, 0, sizeof(peer));
        peer.id = 42;
        peer.state = PEER_HANDSHAKE_COMPLETE;
        peer.prefer_headers = true;

        ASSERT(msg_blocks_should_echo_source_header(&peer, 42));
        ASSERT(!msg_blocks_should_echo_source_header(&peer, 41));
        peer.prefer_headers = false;
        ASSERT(!msg_blocks_should_echo_source_header(&peer, 42));
        peer.prefer_headers = true;
        peer.disconnect = true;
        ASSERT(!msg_blocks_should_echo_source_header(&peer, 42));
        peer.disconnect = false;
        peer.state = PEER_CONNECTED;
        ASSERT(!msg_blocks_should_echo_source_header(&peer, 42));
        ASSERT(!msg_blocks_should_echo_source_header(NULL, 42));
        PASS();
    } _test_next:;
    return failures;
}

static int test_block_validation_retryable_classifier(void)
{
    int failures = 0;
    TEST("msg_handlers: reducer-pending block verdict is retryable") {
        struct validation_state st;
        validation_state_init(&st);
        validation_state_invalid(&st, false, REJECT_INVALID,
                                 "block-not-finalized-by-reducer",
                                 "h=7 tf_cursor=6 ua_ok=1");
        ASSERT(msg_block_validation_is_retryable(&st));
        ASSERT(strstr(st.debug_message, "tf_cursor=6") != NULL);

        const char *const intake_reasons[] = {
            "p2p-block-queued-for-reducer",
            "p2p-block-staged-for-reducer",
            "p2p-block-header-missing",
            "header-admit-inbox-full",
            "reducer-body-header-missing",
            "reducer-body-runtime-unwired",
            "reducer-body-write-failed",
            "reducer-body-verify-failed",
            "p2p-block-intake-unavailable",
            "p2p-block-intake-stopped",
            "p2p-block-intake-full",
            "p2p-block-clone-failed",
        };
        for (size_t i = 0; i < sizeof(intake_reasons) /
                               sizeof(intake_reasons[0]); i++) {
            validation_state_init(&st);
            validation_state_error(&st, intake_reasons[i]);
            ASSERT(msg_block_validation_is_retryable(&st));
        }

        validation_state_init(&st);
        validation_state_invalid(&st, false, REJECT_INVALID,
                                 "bad-txns-inputs-missingorspent", NULL);
        ASSERT(!msg_block_validation_is_retryable(&st));

        validation_state_init(&st);
        ASSERT(!msg_block_validation_is_retryable(&st));
        PASS();
    } _test_next:;
    return failures;
}

static bool submit_reducer_pending_block(struct block *block,
                                         struct validation_state *out,
                                         void *ctx)
{
    (void)block;
    int *calls = (int *)ctx;
    if (calls)
        (*calls)++;
    validation_state_invalid(out, false, REJECT_INVALID,
                             "block-not-finalized-by-reducer", NULL);
    return false;
}

static int test_process_block_msg_reducer_pending_stays_retryable(void)
{
    int failures = 0;
    TEST("msg_handlers: reducer-pending process_block_msg does not mark seen") {
        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000000u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 7;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;

        struct p2p_node node;
        memset(&node, 0, sizeof(node));
        node.id = 77;
        snprintf(node.addr_name, sizeof(node.addr_name), "test-peer");

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(submit_calls == 1);
        ASSERT(!block_already_seen(&hash));

        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    return failures;
}

/* ── PEER_OFFENCE_UNREQUESTED wiring ─────────────────────────────────
 *
 * process_block_msg() (msg_blocks.c) scores PEER_OFFENCE_UNREQUESTED when
 * dl_mark_received() returns UINT32_MAX (no in-flight slot) for the delivered
 * hash from any peer: a "block" message is only a getdata response here.
 * Pinned: scored when never requested, not scored when requested, not scored
 * inside the drain/timeout grace window (see
 * tests/harness/src/test_download.c::test_dl_last_forced_settle_time). */

static void unreq_setup_node(struct p2p_node *node, uint32_t id)
{
    memset(node, 0, sizeof(*node));
    node->id = id;
    snprintf(node->addr_name, sizeof(node->addr_name), "unreq-peer-%u", id);
    /* Non-localhost, non-whitelisted so is_trusted_peer() doesn't exempt
     * it from scoring (mirrors test_peer_scoring.c's setup_node()). */
    node->addr.svc.addr.ip[10] = 0xff;
    node->addr.svc.addr.ip[11] = 0xff;
    node->addr.svc.addr.ip[12] = 198;
    node->addr.svc.addr.ip[13] = 51;
    node->addr.svc.addr.ip[14] = 100;
    node->addr.svc.addr.ip[15] = (unsigned char)id;
}

static int test_process_block_msg_scores_unrequested(void)
{
    int failures = 0;
    TEST("msg_handlers: process_block_msg scores PEER_OFFENCE_UNREQUESTED "
         "when the block was never requested from anyone") {
        peer_scoring_init();
        /* Hermetic: a prior test in this forked process may have left the
         * process-wide download manager in a drain/timeout grace window. */
        dl_init(get_download_mgr());

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000010u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 21;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 501);

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(atomic_load(&node.misbehavior) ==
              peer_offence_weight(PEER_OFFENCE_UNREQUESTED));

        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_block_msg_no_score_when_requested(void)
{
    int failures = 0;
    TEST("msg_handlers: process_block_msg does NOT score a block we "
         "actually asked this peer for") {
        peer_scoring_init();
        dl_init(get_download_mgr());

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000011u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 22;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct p2p_node node;
        unreq_setup_node(&node, 502);

        /* We DID ask this peer for it before it arrived. */
        ASSERT(dl_mark_requested(get_download_mgr(), &hash, 1, node.id));

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;
        mp.net_mgr = &nm;

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(atomic_load(&node.misbehavior) == 0);

        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_block_msg_malformed_releases_owned_requests(void)
{
    int failures = 0;
    TEST("msg_handlers: malformed block disconnects its request owner for "
         "immediate takeover") {
        peer_scoring_init();
        struct download_manager *dm = get_download_mgr();
        dl_init(dm);

        struct p2p_node node;
        unreq_setup_node(&node, 505);
        node.endpoint_generation = 17;

        struct uint256 h1 = make_test_hash(0x51);
        struct uint256 h2 = make_test_hash(0x52);
        ASSERT(dl_mark_requested(dm, &h1, 51, (uint32_t)node.id));
        ASSERT(dl_mark_requested(dm, &h2, 52, (uint32_t)node.id));

        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        /* A complete wire frame can still carry a truncated block payload.
         * The handler cannot name either owned hash from this one byte, so
         * it must enter the normal peer-disconnect lifecycle; that sweep is
         * what releases every request owned by this failed source. */
        const uint8_t truncated_payload = 0;
        struct byte_stream s;
        stream_init_from_data(&s, &truncated_payload, 1);

        ASSERT(!process_block_msg(&mp, &node, &s));
        ASSERT(node.disconnect);
        ASSERT(atomic_load(&node.disconnect_reason) ==
               P2P_DISCONNECT_MESSAGE_PARSE);
        ASSERT(atomic_load(&node.disconnect_source) ==
               P2P_DISCONNECT_SOURCE_MESSAGE_HANDLER);

        /* Model the already-covered connman disconnect sweep and prove the
         * two bodies are immediately available to a healthy peer. */
        ASSERT(dl_peer_disconnected(dm, (uint32_t)node.id) == 2);
        struct uint256 reassigned[2];
        ASSERT(dl_assign_to_peer(dm, 506, reassigned, 2) == 2);
        (void)dl_drain_for_backpressure(dm);

        stream_free(&s);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_blocktxn_malformed_retries_full_body(void)
{
    int failures = 0;
    TEST("msg_handlers: malformed blocktxn abandons partial state and "
         "requeues the full body") {
        peer_scoring_init();
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);

        struct p2p_node node;
        unreq_setup_node(&node, 507);
        struct uint256 hash = make_test_hash(0x71);
        ASSERT(dl_mark_requested(dm, &hash, 71, (uint32_t)node.id));

        node.compact_pending_block = calloc(1, sizeof(struct block));
        ASSERT(node.compact_pending_block != NULL);
        block_init(node.compact_pending_block);
        node.compact_pending_hash = hash;
        node.compact_missing_indices = calloc(1, sizeof(uint64_t));
        ASSERT(node.compact_missing_indices != NULL);
        node.compact_num_missing = 1;
        node.compact_request_time = 1;

        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        const uint8_t truncated_payload = 0;
        struct byte_stream s;
        stream_init_from_data(&s, &truncated_payload, 1);
        ASSERT(!process_blocktxn(&mp, &node, &s));
        ASSERT(node.compact_pending_block == NULL);
        ASSERT(node.compact_missing_indices == NULL);
        ASSERT(!dl_is_in_flight(dm, &hash));

        struct uint256 reassigned;
        ASSERT(dl_assign_to_peer(dm, 508, &reassigned, 1) == 1);
        ASSERT(uint256_eq(&reassigned, &hash));

        (void)dl_drain_for_backpressure(dm);
        stream_free(&s);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_blocktxn_bad_fill_retries_full_body(void)
{
    int failures = 0;
    TEST("msg_handlers: unusable blocktxn completion requeues the full body") {
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);

        struct p2p_node node;
        unreq_setup_node(&node, 509);
        struct uint256 hash = make_test_hash(0x72);
        ASSERT(dl_mark_requested(dm, &hash, 72, (uint32_t)node.id));

        node.compact_pending_block = calloc(1, sizeof(struct block));
        ASSERT(node.compact_pending_block != NULL);
        block_init(node.compact_pending_block);
        node.compact_pending_block->num_vtx = 1;
        node.compact_pending_block->vtx =
            calloc(1, sizeof(struct transaction));
        ASSERT(node.compact_pending_block->vtx != NULL);
        transaction_init(&node.compact_pending_block->vtx[0]);
        node.compact_pending_hash = hash;
        node.compact_missing_indices = calloc(1, sizeof(uint64_t));
        ASSERT(node.compact_missing_indices != NULL);
        node.compact_num_missing = 1;
        node.compact_request_time = (int64_t)time(NULL);

        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        /* Valid blocktxn envelope for the pending hash, but zero txs cannot
         * fill the one missing slot. */
        struct byte_stream s;
        stream_init(&s, 64);
        ASSERT(stream_write_bytes(&s, hash.data, sizeof(hash.data)));
        ASSERT(stream_write_compact_size(&s, 0));
        ASSERT(process_blocktxn(&mp, &node, &s));
        ASSERT(node.compact_pending_block == NULL);
        ASSERT(!dl_is_in_flight(dm, &hash));

        struct uint256 reassigned;
        ASSERT(dl_assign_to_peer(dm, 510, &reassigned, 1) == 1);
        ASSERT(uint256_eq(&reassigned, &hash));

        (void)dl_drain_for_backpressure(dm);
        stream_free(&s);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_blocktxn_stale_retries_full_body(void)
{
    int failures = 0;
    TEST("msg_handlers: stale blocktxn response requeues the full body") {
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);

        struct p2p_node node;
        unreq_setup_node(&node, 511);
        struct uint256 hash = make_test_hash(0x73);
        ASSERT(dl_mark_requested(dm, &hash, 73, (uint32_t)node.id));

        node.compact_pending_block = calloc(1, sizeof(struct block));
        ASSERT(node.compact_pending_block != NULL);
        block_init(node.compact_pending_block);
        node.compact_pending_hash = hash;
        node.compact_request_time = (int64_t)time(NULL) - 31;

        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        struct byte_stream s;
        stream_init(&s, 64);
        ASSERT(stream_write_bytes(&s, hash.data, sizeof(hash.data)));
        ASSERT(stream_write_compact_size(&s, 0));
        ASSERT(process_blocktxn(&mp, &node, &s));
        ASSERT(node.compact_pending_block == NULL);
        ASSERT(!dl_is_in_flight(dm, &hash));

        struct uint256 reassigned;
        ASSERT(dl_assign_to_peer(dm, 512, &reassigned, 1) == 1);
        ASSERT(uint256_eq(&reassigned, &hash));

        (void)dl_drain_for_backpressure(dm);
        stream_free(&s);
        PASS();
    } _test_next:;
    return failures;
}

static bool make_missing_compact_test_block(struct compact_block_msg *cb,
                                            const struct chain_params *cp)
{
    struct block blk;
    block_init(&blk);
    blk.header.nVersion = 4;
    blk.header.nTime = 1700000513u;
    struct arith_uint256 pow_limit;
    uint256_to_arith(&pow_limit, &cp->consensus.powLimit);
    blk.header.nBits = arith_uint256_get_compact(&pow_limit, false);
    blk.num_vtx = 2;
    blk.vtx = calloc(blk.num_vtx, sizeof(*blk.vtx));
    if (!blk.vtx)
        return false;

    for (size_t i = 0; i < blk.num_vtx; i++) {
        transaction_init(&blk.vtx[i]);
        blk.vtx[i].version = 4;
        blk.vtx[i].overwintered = true;
        blk.vtx[i].version_group_id = SAPLING_VERSION_GROUP_ID;
        memset(blk.vtx[i].hash.data, (int)(0x80 + i),
               sizeof(blk.vtx[i].hash.data));
    }
    blk.vtx[0].num_vin = 1;
    blk.vtx[0].vin = calloc(1, sizeof(*blk.vtx[0].vin));
    if (!blk.vtx[0].vin) {
        block_free(&blk);
        return false;
    }
    tx_in_init(&blk.vtx[0].vin[0]);

    bool ok = mine_block_pow(&blk, 1, cp, 0) &&
              compact_block_from_block(cb, &blk, 513);
    block_free(&blk);
    return ok;
}

static int test_process_cmpctblock_replacement_retries_old_body(void)
{
    int failures = 0;
    TEST("msg_handlers: newer cmpctblock requeues replaced partial body") {
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_REGTEST);
        const struct chain_params *cp = chain_params_get();

        struct compact_block_msg cb;
        compact_block_msg_init(&cb);
        bool setup_ok = make_missing_compact_test_block(&cb, cp);
        struct byte_stream s;
        stream_init(&s, 4096);
        setup_ok = setup_ok && compact_block_msg_serialize(&cb, &s);

        struct tx_mempool pool;
        tx_mempool_init(&pool, 0);
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.params = cp;
        mp.mempool = &pool;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 513);
        node.socket = PLATFORM_SOCKET_INVALID;
        zcl_mutex_init(&node.cs_send);
        struct uint256 old_hash = make_test_hash(0x74);
        setup_ok = setup_ok &&
                   dl_mark_requested(dm, &old_hash, 74, (uint32_t)node.id);
        node.compact_pending_block = calloc(1, sizeof(struct block));
        setup_ok = setup_ok && node.compact_pending_block != NULL;
        if (node.compact_pending_block) {
            block_init(node.compact_pending_block);
            node.compact_pending_hash = old_hash;
            node.compact_request_time = (int64_t)time(NULL);
        }

        bool handled = setup_ok && process_cmpctblock(&mp, &node, &s);
        bool old_released = !dl_is_in_flight(dm, &old_hash);
        struct uint256 reassigned_hash;
        bool reassigned =
            dl_assign_to_peer(dm, 514, &reassigned_hash, 1) == 1 &&
            uint256_eq(&reassigned_hash, &old_hash);

        if (node.compact_pending_block) {
            block_free(node.compact_pending_block);
            free(node.compact_pending_block);
        }
        free(node.compact_missing_indices);
        while (node.send_head) {
            struct send_segment *seg = node.send_head;
            node.send_head = seg->next;
            send_segment_free(seg);
        }
        zcl_mutex_destroy(&node.cs_send);
        tx_mempool_free(&pool);
        stream_free(&s);
        compact_block_msg_free(&cb);
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_MAIN);

        ASSERT(setup_ok);
        ASSERT(handled);
        ASSERT(old_released);
        ASSERT(reassigned);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_cmpctblock_pending_alloc_failure_retries_body(void)
{
    int failures = 0;
    TEST("msg_handlers: compact pending allocation failure requeues body") {
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_REGTEST);
        const struct chain_params *cp = chain_params_get();

        struct compact_block_msg cb;
        compact_block_msg_init(&cb);
        bool setup_ok = make_missing_compact_test_block(&cb, cp);
        struct uint256 hash;
        block_header_get_hash(&cb.header, &hash);
        struct byte_stream s;
        stream_init(&s, 4096);
        setup_ok = setup_ok && compact_block_msg_serialize(&cb, &s);

        struct tx_mempool pool;
        tx_mempool_init(&pool, 0);
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.params = cp;
        mp.mempool = &pool;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 515);
        node.socket = PLATFORM_SOCKET_INVALID;
        zcl_mutex_init(&node.cs_send);
        setup_ok = setup_ok &&
                   dl_mark_requested(dm, &hash, 75, (uint32_t)node.id);

        zcl_alloc_fault_fail_next("compact_pending_block");
        bool handled = setup_ok && process_cmpctblock(&mp, &node, &s);
        bool fault_consumed = zcl_alloc_fault_armed_label() == NULL;
        bool released = !dl_is_in_flight(dm, &hash);
        struct uint256 reassigned_hash;
        bool reassigned =
            dl_assign_to_peer(dm, 516, &reassigned_hash, 1) == 1 &&
            uint256_eq(&reassigned_hash, &hash);

        zcl_alloc_fault_clear();
        if (node.compact_pending_block) {
            block_free(node.compact_pending_block);
            free(node.compact_pending_block);
        }
        free(node.compact_missing_indices);
        while (node.send_head) {
            struct send_segment *seg = node.send_head;
            node.send_head = seg->next;
            send_segment_free(seg);
        }
        zcl_mutex_destroy(&node.cs_send);
        tx_mempool_free(&pool);
        stream_free(&s);
        compact_block_msg_free(&cb);
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_MAIN);

        ASSERT(setup_ok);
        ASSERT(handled);
        ASSERT(fault_consumed);
        ASSERT(released);
        ASSERT(reassigned);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_cmpctblock_request_serialize_failure_retries_body(void)
{
    int failures = 0;
    TEST("msg_handlers: getblocktxn serialization failure requeues body") {
        struct download_manager *dm = get_download_mgr();
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_REGTEST);
        const struct chain_params *cp = chain_params_get();

        struct compact_block_msg cb;
        compact_block_msg_init(&cb);
        bool setup_ok = make_missing_compact_test_block(&cb, cp);
        struct uint256 hash;
        block_header_get_hash(&cb.header, &hash);
        struct byte_stream s;
        stream_init(&s, 4096);
        setup_ok = setup_ok && compact_block_msg_serialize(&cb, &s);

        struct tx_mempool pool;
        tx_mempool_init(&pool, 0);
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.params = cp;
        mp.mempool = &pool;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 517);
        node.socket = PLATFORM_SOCKET_INVALID;
        zcl_mutex_init(&node.cs_send);
        setup_ok = setup_ok &&
                   dl_mark_requested(dm, &hash, 76, (uint32_t)node.id);

        /* Processing first allocates streams for SipHash-key derivation,
         * prefilled-transaction hashing, and block-header hashing. The fourth
         * matching allocation is the outbound getblocktxn request stream. */
        zcl_alloc_fault_fail_nth("stream_data", 4);
        bool handled = setup_ok && process_cmpctblock(&mp, &node, &s);
        bool fault_consumed = zcl_alloc_fault_armed_label() == NULL;
        bool pending_cleared = node.compact_pending_block == NULL;
        bool released = !dl_is_in_flight(dm, &hash);
        struct uint256 reassigned_hash;
        bool reassigned =
            dl_assign_to_peer(dm, 518, &reassigned_hash, 1) == 1 &&
            uint256_eq(&reassigned_hash, &hash);

        zcl_alloc_fault_clear();
        if (node.compact_pending_block) {
            block_free(node.compact_pending_block);
            free(node.compact_pending_block);
        }
        free(node.compact_missing_indices);
        while (node.send_head) {
            struct send_segment *seg = node.send_head;
            node.send_head = seg->next;
            send_segment_free(seg);
        }
        zcl_mutex_destroy(&node.cs_send);
        tx_mempool_free(&pool);
        stream_free(&s);
        compact_block_msg_free(&cb);
        (void)dl_drain_for_backpressure(dm);
        chain_params_select(CHAIN_MAIN);

        ASSERT(setup_ok);
        ASSERT(handled);
        ASSERT(fault_consumed);
        ASSERT(pending_cleared);
        ASSERT(released);
        ASSERT(reassigned);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_block_msg_no_score_when_requested_from_peer_zero(void)
{
    int failures = 0;
    TEST("msg_handlers: peer id zero remains a requested block, not the "
         "not-found sentinel") {
        peer_scoring_init();
        dl_init(get_download_mgr());

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000014u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 25;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct p2p_node node;
        unreq_setup_node(&node, 0);
        ASSERT(dl_mark_requested(get_download_mgr(), &hash, 1, node.id));

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;
        mp.net_mgr = &nm;

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(atomic_load(&node.misbehavior) == 0);

        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_block_msg_no_score_within_settle_grace(void)
{
    int failures = 0;
    TEST("msg_handlers: process_block_msg withholds scoring inside the "
         "post-drain/timeout grace window (honest-but-late delivery)") {
        peer_scoring_init();
        struct download_manager *dm = get_download_mgr();
        dl_init(dm);
        /* A drain or timeout reassignment force-clears in-flight state, so a
         * requested body can arrive with no trace it was asked for. */
        (void)dl_drain_for_backpressure(dm);

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000012u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 23;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 503);

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(atomic_load(&node.misbehavior) == 0);

        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    return failures;
}

static int test_process_block_msg_no_score_during_shutdown(void)
{
    int failures = 0;
    TEST("msg_handlers: process_block_msg withholds unrequested scoring "
         "during orderly shutdown") {
        peer_scoring_init();
        dl_init(get_download_mgr());
        thread_registry_reset_for_test();
        thread_registry_request_shutdown();

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000013u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 24;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        int submit_calls = 0;
        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.block_submit = submit_reducer_pending_block;
        mp.block_submit_ctx = &submit_calls;
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 504);

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(atomic_load(&node.misbehavior) == 0);

        thread_registry_reset_for_test();
        stream_free(&s);
        block_free(&blk);
        PASS();
    } _test_next:;
    /* ASSERT can jump past the in-body cleanup.  Never leak the process-wide
     * shutdown bit into later tests in this forked group. */
    thread_registry_reset_for_test();
    return failures;
}

struct async_block_submit_ctx {
    atomic_int entered;
    atomic_int release;
    atomic_int drains;
    atomic_int batch_begins;
    atomic_int batch_ends;
};

static void count_async_batch_begin(void *ctx)
{
    struct async_block_submit_ctx *submit_ctx = ctx;
    atomic_fetch_add_explicit(&submit_ctx->batch_begins, 1,
                              memory_order_relaxed);
}

static void count_async_batch_end(void *ctx)
{
    struct async_block_submit_ctx *submit_ctx = ctx;
    atomic_fetch_add_explicit(&submit_ctx->batch_ends, 1,
                              memory_order_relaxed);
}

static int count_async_catchup_drain(void *ctx)
{
    struct async_block_submit_ctx *submit_ctx = ctx;
    atomic_fetch_add_explicit(&submit_ctx->drains, 1,
                              memory_order_relaxed);
    return 0;
}

static bool submit_async_blocking_pending(struct block *block,
                                          struct validation_state *out,
                                          void *ctx)
{
    (void)block;
    struct async_block_submit_ctx *submit_ctx = ctx;
    atomic_fetch_add_explicit(&submit_ctx->entered, 1,
                              memory_order_relaxed);
    while (!atomic_load_explicit(&submit_ctx->release,
                                 memory_order_acquire)) {
        test_msg_sleep_ms(1);
    }
    validation_state_invalid(out, false, REJECT_INVALID,
                             "block-not-finalized-by-reducer", NULL);
    return false;
}

static int test_process_block_msg_queues_reducer_during_catchup(void)
{
    int failures = 0;
    TEST("msg_handlers: catch-up block intake does not block message thread") {
        test_msg_sync_to_blocks_download();
        ASSERT(sync_get_state() == SYNC_BLOCKS_DOWNLOAD);

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000001u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 9;

        struct uint256 hash;
        block_get_hash(&blk, &hash);
        block_clear_seen(&hash);

        struct byte_stream s;
        stream_init(&s, 256);
        ASSERT(block_serialize(&blk, &s));

        struct main_state ms;
        main_state_init(&ms);
        struct async_block_submit_ctx submit_ctx = {0};
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.main_state = &ms;
        mp.params = chain_params_get();
        mp.block_submit = submit_async_blocking_pending;
        mp.block_submit_ctx = &submit_ctx;
        mp.catchup_drain = count_async_catchup_drain;
        mp.catchup_drain_ctx = &submit_ctx;
        msg_processor_set_catchup_batch_scope(
            &mp, count_async_batch_begin, count_async_batch_end, &submit_ctx);

        struct p2p_node node;
        memset(&node, 0, sizeof(node));
        node.id = 88;
        snprintf(node.addr_name, sizeof(node.addr_name), "test-peer");

        ASSERT(process_block_msg(&mp, &node, &s));
        ASSERT(!atomic_load_explicit(&submit_ctx.release,
                                     memory_order_acquire));

        for (int i = 0; i < 200 &&
             atomic_load_explicit(&submit_ctx.entered,
                                  memory_order_acquire) == 0; i++) {
            test_msg_sleep_ms(1);
        }
        ASSERT(atomic_load_explicit(&submit_ctx.entered,
                                    memory_order_acquire) == 1);

        struct msg_block_intake_stats stats;
        msg_processor_get_block_intake_stats(&mp, &stats);
        ASSERT(stats.running);
        ASSERT(stats.capacity > 0);
        ASSERT(stats.enqueued == 1);
        ASSERT(stats.processed == 0);
        ASSERT(!block_already_seen(&hash));

        /* The periodic evaluator may commit while this worker owns the final
         * historical body; its post-submit reducer drain must survive. */
        ASSERT(sync_try_transition(SYNC_BLOCKS_DOWNLOAD, SYNC_AT_TIP,
                                   "unit periodic edge"));
        atomic_store_explicit(&submit_ctx.release, 1,
                              memory_order_release);
        msg_processor_stop_block_intake(&mp);
        ASSERT(atomic_load_explicit(&submit_ctx.drains,
                                    memory_order_acquire) == 1);
        ASSERT(atomic_load_explicit(&submit_ctx.batch_begins,
                                    memory_order_acquire) == 1);
        ASSERT(atomic_load_explicit(&submit_ctx.batch_ends,
                                    memory_order_acquire) == 1);
        stream_free(&s);
        block_free(&blk);
        main_state_free(&ms);
        test_msg_sync_to_idle();
        PASS();
    } _test_next:;
    return failures;
}

static int test_msg_block_intake_full_stays_retryable(void)
{
    int failures = 0;
    TEST("msg_handlers: full catch-up block intake stays retryable") {
        test_msg_sync_to_blocks_download();
        ASSERT(sync_get_state() == SYNC_BLOCKS_DOWNLOAD);

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000002u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 10;

        struct uint256 hash;
        block_get_hash(&blk, &hash);

        struct main_state ms;
        main_state_init(&ms);
        struct async_block_submit_ctx submit_ctx = {0};
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.main_state = &ms;
        mp.params = chain_params_get();
        mp.block_submit = submit_async_blocking_pending;
        mp.block_submit_ctx = &submit_ctx;

        bool saw_full = false;
        struct validation_state state;
        validation_state_init(&state);
        ASSERT(msg_processor_enqueue_p2p_block(&mp, &blk, &hash,
                                               89, &state));

        struct msg_block_intake_stats stats;
        msg_processor_get_block_intake_stats(&mp, &stats);
        ASSERT(stats.capacity > 0);
        /* Fill with distinct hashes: re-deliveries of a queued hash reuse its
         * slot (p2p-block-already-queued) and never reach the full arm, which
         * is only for first-seen bodies. */
        for (uint64_t i = 0; i <= stats.capacity && !saw_full; i++) {
            struct block other;
            block_init(&other);
            other.header.nVersion = 4;
            other.header.nTime = 1700000002u;
            other.header.nBits = 0x1f00ffffu;
            memset(&other.header.nNonce, 0, sizeof(other.header.nNonce));
            other.header.nNonce.data[0] = 10;
            uint64_t tag = i + 1;
            memcpy(&other.header.nNonce.data[8], &tag, sizeof(tag));
            struct uint256 other_hash;
            block_get_hash(&other, &other_hash);
            validation_state_init(&state);
            ASSERT(msg_processor_enqueue_p2p_block(&mp, &other, &other_hash,
                                                   89, &state));
            saw_full = strcmp(state.reject_reason,
                              "p2p-block-intake-full") == 0;
            block_free(&other);
        }

        ASSERT(saw_full);
        ASSERT(msg_block_validation_is_retryable(&state));

        msg_processor_get_block_intake_stats(&mp, &stats);
        ASSERT(stats.capacity > 0);
        ASSERT(stats.current_depth <= stats.capacity);
        ASSERT(stats.dropped > 0);
        ASSERT(stats.enqueued > 0);
        ASSERT(stats.duplicates == 0);

        atomic_store_explicit(&submit_ctx.release, 1,
                              memory_order_release);
        msg_processor_stop_block_intake(&mp);
        block_free(&blk);
        main_state_free(&ms);
        test_msg_sync_to_idle();
        PASS();
    } _test_next:;
    return failures;
}

static int test_msg_block_intake_duplicate_reuses_slot(void)
{
    int failures = 0;
    TEST("msg_handlers: duplicate block body reuses its intake slot") {
        test_msg_sync_to_blocks_download();
        ASSERT(sync_get_state() == SYNC_BLOCKS_DOWNLOAD);

        struct block blk;
        block_init(&blk);
        blk.header.nVersion = 4;
        blk.header.nTime = 1700000002u;
        blk.header.nBits = 0x1f00ffffu;
        blk.header.nNonce.data[0] = 10;

        struct uint256 hash;
        block_get_hash(&blk, &hash);

        struct main_state ms;
        main_state_init(&ms);
        struct async_block_submit_ctx submit_ctx = {0};
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.main_state = &ms;
        mp.params = chain_params_get();
        mp.block_submit = submit_async_blocking_pending;
        mp.block_submit_ctx = &submit_ctx;

        struct validation_state state;
        struct msg_block_intake_stats stats;

        /* Re-delivered bodies (timeout reassignment / grace) must reuse the
         * existing slot: capacity+1 copies of one hash take at most two slots
         * (ring + worker), so a first-seen body is still admitted. The worker
         * may dequeue the first copy early, so assert only invariants that
         * hold either way. */
        validation_state_init(&state);
        ASSERT(msg_processor_enqueue_p2p_block(&mp, &blk, &hash,
                                               89, &state));
        msg_processor_get_block_intake_stats(&mp, &stats);
        const uint64_t capacity = stats.capacity;
        ASSERT(capacity > 0);
        for (uint64_t i = 0; i < capacity; i++) {
            validation_state_init(&state);
            ASSERT(msg_processor_enqueue_p2p_block(&mp, &blk, &hash,
                                                   89, &state));
            ASSERT(strcmp(state.reject_reason,
                          "p2p-block-queued-for-reducer") == 0 ||
                   strcmp(state.reject_reason,
                          "p2p-block-already-queued") == 0);
            ASSERT(msg_block_validation_is_retryable(&state));
        }
        msg_processor_get_block_intake_stats(&mp, &stats);
        ASSERT(stats.enqueued + stats.duplicates == capacity + 1);
        ASSERT(stats.enqueued <= 2);
        ASSERT(stats.duplicates >= capacity - 1);
        ASSERT(stats.current_depth <= 1);
        ASSERT(stats.dropped == 0);

        /* The ring is nearly empty: a first-seen body is admitted
         * immediately, not dropped as full. */
        struct block other;
        block_init(&other);
        other.header.nVersion = 4;
        other.header.nTime = 1700000002u;
        other.header.nBits = 0x1f00ffffu;
        other.header.nNonce.data[0] = 11;
        struct uint256 other_hash;
        block_get_hash(&other, &other_hash);
        validation_state_init(&state);
        ASSERT(msg_processor_enqueue_p2p_block(&mp, &other, &other_hash,
                                               89, &state));
        ASSERT(strcmp(state.reject_reason,
                      "p2p-block-queued-for-reducer") == 0);
        msg_processor_get_block_intake_stats(&mp, &stats);
        ASSERT(stats.enqueued <= 3);
        ASSERT(stats.dropped == 0);
        block_free(&other);

        atomic_store_explicit(&submit_ctx.release, 1,
                              memory_order_release);
        msg_processor_stop_block_intake(&mp);
        block_free(&blk);
        main_state_free(&ms);
        test_msg_sync_to_idle();
        PASS();
    } _test_next:;
    return failures;
}

static void test_init_complete_empty_message(struct net_message *msg,
                                             const char *command)
{
    static const unsigned char msgstart[MESSAGE_START_SIZE] = {
        0x24, 0xe9, 0x27, 0x64
    };
    unsigned char hash[SHA256_OUTPUT_SIZE];
    net_message_init(msg, msgstart);
    msg_header_init_full(&msg->hdr, msgstart, command, 0);
    hash256(NULL, 0, hash);
    memcpy(&msg->hdr.nChecksum, hash, sizeof(msg->hdr.nChecksum));
    msg->in_data = true;
    msg->data_pos = 0;
}

static int test_msg_process_messages_yields_after_bounded_batch(void)
{
    int failures = 0;
    TEST("msg_handlers: inbound processing yields after bounded batch") {
        const size_t total = ZCL_MSG_PROCESS_MAX_PER_CYCLE + 3;
        struct p2p_node node;
        struct msg_processor mp;
        memset(&node, 0, sizeof(node));
        memset(&mp, 0, sizeof(mp));
        node.id = 88;
        node.version = 170011;
        atomic_store(&node.state, PEER_ACTIVE);
        zcl_mutex_init(&node.cs_recv);
        node.recv_msg_cap = total;
        node.recv_msg_count = total;
        node.recv_msgs = zcl_calloc(total, sizeof(*node.recv_msgs),
                                    "test_recv_msgs");
        ASSERT(node.recv_msgs != NULL);
        snprintf(node.addr_name, sizeof(node.addr_name), "test-peer");
        for (size_t i = 0; i < total; i++)
            test_init_complete_empty_message(&node.recv_msgs[i], "noop");

        ASSERT(msg_process_messages(&mp, &node));
        ASSERT(!node.disconnect);
        ASSERT(node.recv_msg_count == total - ZCL_MSG_PROCESS_MAX_PER_CYCLE);

        for (size_t i = 0; i < node.recv_msg_count; i++)
            net_message_free(&node.recv_msgs[i]);
        free(node.recv_msgs);
        zcl_mutex_destroy(&node.cs_recv);
        PASS();
    } _test_next:;
    return failures;
}

/* ── D1: swarm aggregate SHA3 UTXO-snapshot mismatch is not a
 * silent-accept ───────────────────────────────────────────────────
 *
 * Via msgprocessor_test_swarm_utxo_sha3_verify: PASSED records nothing and
 * returns true; a mismatch records PEER_OFFENCE_INVALID_PROOF, names the
 * "snapshot_sync.utxo_sha3_mismatch" typed DEPENDENCY blocker, and returns
 * false (sync not complete). */

static int test_swarm_utxo_sha3_verify_passed_is_quiet(void)
{
    int failures = 0;
    TEST("msg_handlers: swarm UTXO SHA3 verify PASSED records nothing, "
         "returns true") {
        blocker_module_init();
        blocker_reset_for_testing();

        struct net_manager nm;
        memset(&nm, 0, sizeof(nm));
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 601);

        uint8_t root[32];
        memset(root, 0x42, sizeof(root));

        bool ok = msgprocessor_test_swarm_utxo_sha3_verify(
            &mp, &node, root, root, 12345);

        ASSERT(ok);
        ASSERT(atomic_load(&node.misbehavior) == 0);
        ASSERT(!blocker_exists("snapshot_sync.utxo_sha3_mismatch"));

        blocker_reset_for_testing();
        PASS();
    } _test_next:;
    return failures;
}

static int test_swarm_utxo_sha3_verify_mismatch_is_not_silent(void)
{
    int failures = 0;
    TEST("msg_handlers: swarm UTXO SHA3 mismatch scores the peer, names "
         "a typed blocker, and reports sync NOT complete") {
        blocker_module_init();
        blocker_reset_for_testing();

        struct net_manager nm;
        net_manager_init(&nm);
        struct msg_processor mp;
        memset(&mp, 0, sizeof(mp));
        mp.net_mgr = &nm;

        struct p2p_node node;
        unreq_setup_node(&node, 602);

        uint8_t local_root[32];
        uint8_t expected_root[32];
        memset(local_root, 0x11, sizeof(local_root));
        memset(expected_root, 0x99, sizeof(expected_root));

        bool ok = msgprocessor_test_swarm_utxo_sha3_verify(
            &mp, &node, local_root, expected_root, 999);
        net_manager_free(&nm);

        /* Sync must NOT be reported complete on a mismatch. */
        ASSERT(!ok);

        /* Peer offence recorded — PEER_OFFENCE_INVALID_PROOF weight. */
        ASSERT_EQ(atomic_load(&node.misbehavior),
                  peer_offence_weight(PEER_OFFENCE_INVALID_PROOF));

        /* Typed blocker named (visible to dumpstate blocker). */
        ASSERT(blocker_exists("snapshot_sync.utxo_sha3_mismatch"));
        struct blocker_snapshot snaps[16];
        int n = blocker_snapshot_all(snaps, 16);
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(snaps[i].id, "snapshot_sync.utxo_sha3_mismatch") == 0) {
                found = true;
                ASSERT(snaps[i].class == BLOCKER_DEPENDENCY);
                ASSERT(snaps[i].retry_budget == -1);
                break;
            }
        }
        ASSERT(found);

        blocker_reset_for_testing();
        PASS();
    } _test_next:;
    return failures;
}

/* ── Entry point ───────────────────────────────────────────────── */

int test_msg_handlers(void);

int test_msg_handlers(void)
{
    int failures = 0;

    failures += test_headers_stats_null_safe();
    failures += test_headers_stats_initial();
    failures += test_block_dedup_basic();
    failures += test_block_dedup_mark_and_check();
    failures += test_block_dedup_clear();
    failures += test_block_dedup_multiple();
    failures += test_tx_dedup_basic();
    failures += test_tx_dedup_mark_and_check();
    failures += test_dandelion_initial_state();
    failures += test_p148_should_mark_seen_rejects_null();
    failures += test_p148_should_mark_seen_rejects_orphan();
    failures += test_p148_should_mark_seen_accepts_active();
    failures += test_should_announce_getblocks();
    failures += test_source_header_echo_policy();
    failures += test_block_validation_retryable_classifier();
    failures += test_process_block_msg_reducer_pending_stays_retryable();
    failures += test_process_block_msg_scores_unrequested();
    failures += test_process_block_msg_no_score_when_requested();
    failures += test_process_block_msg_malformed_releases_owned_requests();
    failures += test_process_blocktxn_malformed_retries_full_body();
    failures += test_process_blocktxn_bad_fill_retries_full_body();
    failures += test_process_blocktxn_stale_retries_full_body();
    failures += test_process_cmpctblock_replacement_retries_old_body();
    failures += test_process_cmpctblock_pending_alloc_failure_retries_body();
    failures += test_process_cmpctblock_request_serialize_failure_retries_body();
    failures += test_process_block_msg_no_score_when_requested_from_peer_zero();
    failures += test_process_block_msg_no_score_within_settle_grace();
    failures += test_process_block_msg_no_score_during_shutdown();
    failures += test_process_block_msg_queues_reducer_during_catchup();
    failures += test_msg_block_intake_full_stays_retryable();
    failures += test_msg_block_intake_duplicate_reuses_slot();
    failures += test_msg_process_messages_yields_after_bounded_batch();
    failures += test_swarm_utxo_sha3_verify_passed_is_quiet();
    failures += test_swarm_utxo_sha3_verify_mismatch_is_not_silent();

    return failures;
}
