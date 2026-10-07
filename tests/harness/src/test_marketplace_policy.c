/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Observe real optional-data admission, persistence, delivery and refusal.
 * Successful controls use the same signed bytes and paths as the refusals. */
#include "test/test_core.h"
#include "chain/chainparams.h"
#include "base/hex.h"
#include "controllers/api_controller.h"
#include "controllers/file_market_controller.h"
#include "controllers/store_controller.h"
#include "controllers/yardsale_site_controller.h"
#include "core/hash.h"
#include "core/serialize.h"
#include "crypto/ed25519.h"
#include "models/file_offer.h"
#include "models/zswap_ad.h"
#include "net/marketplace.h"
#include "net/fast_sync.h"
#include "util/safe_alloc.h"
#include "net/msgprocessor.h"
#include "services/market_participation_service.h"
#include "services/market_moderation_service.h"
#include "platform/time_compat.h"
#include "platform/socket_compat.h"
#include "util/util.h"
#include "zswap/zswap_yardsale.h"

#define CHECK(claim, observation) do { \
    bool passed = (observation); \
    printf("marketplace_policy: %s: %s\n", (claim), passed ? "PASS" : "FAIL"); \
    failures += !passed; \
} while (0)

struct policy_fixture {
    struct node_db db;
    char dir[512];
    char path[640];
    uint8_t wire[ZSWAP_QUOTE_WIRE_BYTES];
    struct zswap_yardsale_ad ad;
    struct file_offer free_offer;
    int64_t now;
    int saves;
};

static void policy_arguments(const char *flag)
{
    const char *args[] = {"test", flag};
    ParseParameters(flag ? 2 : 1, args);
}

static bool policy_quote(struct policy_fixture *f)
{
    uint8_t seed[32] = {0x35}, secret[32];
    struct zswap_quote_v1 *q = &f->ad.quote;
    q->schema_version = ZSWAP_QUOTE_VERSION;
    memcpy(q->network_genesis_root, chain_params_get()->consensus.hashGenesisBlock.data, 32);
    ed25519_keypair(q->seller_pubkey, secret, seed);
    memset(q->token_id, 0x45, 32);
    q->nonce = 17;
    q->token_amount = 1000;
    q->zcl_amount = 2000;
    q->issued_unix = f->now - 1;
    q->expires_unix = f->now + 45;
    f->ad.first_seen_unix = f->ad.last_seen_unix = f->now;
    f->ad.seen_count = 1;
    return zswap_quote_seal(q, seed) == ZSWAP_QUOTE_OK &&
           zswap_quote_encode(q, f->wire) == ZSWAP_QUOTE_OK &&
           zswap_quote_root(q, f->ad.quote_root) == ZSWAP_QUOTE_OK;
}

static bool policy_fixture_open(struct policy_fixture *f)
{
    memset(f, 0, sizeof(*f));
    chain_params_select(CHAIN_REGTEST);
    test_make_tmpdir(f->dir, sizeof(f->dir), "marketplace_policy", "db");
    snprintf(f->path, sizeof(f->path), "%s/node.db", f->dir);
    if (!node_db_open(&f->db, f->path)) return false;
    rpc_market_set_state(&f->db);
    f->now = (int64_t)platform_time_wall_time_t();
    if (!policy_quote(f)) return false;
    memset(f->free_offer.root_hash, 0x68, 32);
    snprintf(f->free_offer.filename, sizeof(f->free_offer.filename), "fixture.bin");
    f->free_offer.size_bytes = 1;
    f->free_offer.num_chunks = 1;
    f->free_offer.ttl = FILE_MARKET_MAX_TTL;
    f->free_offer.last_seen = f->now;
    return true;
}

static void policy_fixture_close(struct policy_fixture *f)
{
    rpc_market_set_state(NULL);
    file_market_forget(f->free_offer.root_hash);
    zswap_yardsale_reset();
    node_db_close(&f->db);
    test_cleanup_tmpdir(f->dir);
    policy_arguments(NULL);
}

static enum zswap_yardsale_ingest policy_ingest(struct policy_fixture *f)
{
    return zswap_yardsale_ingest_wire(f->wire, sizeof(f->wire),
        f->ad.quote.network_genesis_root, 71, f->now, NULL);
}

static int policy_default_and_opt_in(struct policy_fixture *f)
{
    int failures = 0;
    policy_arguments(NULL);
    CHECK("fresh default disabled", !marketplace_enabled());
    CHECK("default refuses verified quote wire", !marketplace_wire_allowed("zswapquote", f->wire, sizeof(f->wire)));
    CHECK("default refuses cache ingest", policy_ingest(f) == ZSWAP_YARDSALE_INGEST_REFUSED);
    CHECK("default no Yardsale projection write", !db_zswap_ad_save(&f->db, &f->ad));
    CHECK("default no legacy listing cache", !file_market_add_offer(&f->free_offer));
    CHECK("default no legacy listing persistence", !db_file_offer_save(&f->db, &f->free_offer));
    CHECK("default empty cache", zswap_yardsale_count(f->now) == 0 && file_market_count() == 0);
    struct zswap_yardsale_ad back;
    CHECK("default empty database", !db_zswap_ad_find(&f->db, f->ad.quote_root, &back));
    policy_arguments("-marketplace=1");
    CHECK("explicit opt-in enables", marketplace_enabled());
    CHECK("same quote accepted by wire policy", marketplace_wire_allowed("zswapquote", f->wire, sizeof(f->wire)));
    CHECK("same signed quote ingests", policy_ingest(f) == ZSWAP_YARDSALE_INGEST_NEW);
    CHECK("same quote persists", db_zswap_ad_save(&f->db, &f->ad));
    CHECK("persisted quote observable", db_zswap_ad_find(&f->db, f->ad.quote_root, &back));
    CHECK("same legacy listing caches", file_market_add_offer(&f->free_offer));
    CHECK("same legacy listing persists", db_file_offer_save(&f->db, &f->free_offer));
    return failures;
}

static bool policy_send(const char *command, const uint8_t *wire, size_t len,
                         size_t *queued)
{
    struct p2p_node node = {0};
    node.socket = ZCL_INVALID_SOCKET;
    zcl_mutex_init(&node.cs_send);
    const unsigned char magic[4] = {0x24, 0xe9, 0x27, 0x64};
    bool begun = p2p_node_begin_message(&node, command, magic);
    bool sent = false;
    if (begun) {
        p2p_node_write_message_data(&node, wire, len);
        sent = p2p_node_end_message(&node);
    }
    *queued = node.send_size;
    while (node.send_head) {
        struct send_segment *segment = node.send_head;
        node.send_head = segment->next;
        send_segment_free(segment);
    }
    zcl_mutex_destroy(&node.cs_send);
    return sent;
}

static int policy_ingest_port(const uint8_t *wire, size_t len,
    const uint8_t network[32], int64_t peer, int64_t now,
    struct zswap_yardsale_ad *out, void *ctx)
{
    (void)ctx;
    return zswap_yardsale_ingest_wire(wire, len, network, peer, now, out);
}

static bool policy_save_port(const struct zswap_yardsale_ad *ad, void *ctx)
{
    struct policy_fixture *f = ctx;
    f->saves++;
    return db_zswap_ad_save(&f->db, ad);
}

static bool policy_receive(struct policy_fixture *f)
{
    struct net_message message;
    const unsigned char magic[4] = {0x24, 0xe9, 0x27, 0x64};
    net_message_init(&message, magic);
    msg_header_init_full(&message.hdr, magic, "zswapquote", sizeof(f->wire));
    message.recv_data = zcl_malloc(sizeof(f->wire), "marketplace frame fixture");
    if (!message.recv_data) return false;
    memcpy(message.recv_data, f->wire, sizeof(f->wire));
    message.recv_alloc = message.data_pos = sizeof(f->wire);
    message.in_data = true;
    uint8_t hash[32];
    hash256(f->wire, sizeof(f->wire), hash);
    memcpy(&message.hdr.nChecksum, hash, sizeof(message.hdr.nChecksum));
    struct p2p_node node = {0};
    node.socket = ZCL_INVALID_SOCKET;
    node.id = 17;
    node.version = 170011;
    node.services = NODE_ZCL23;
    atomic_store(&node.state, PEER_ACTIVE);
    zcl_mutex_init(&node.cs_recv);
    node.recv_msgs = &message;
    node.recv_msg_cap = node.recv_msg_count = 1;
    struct msg_processor mp = {0};
    mp.params = chain_params_get();
    msg_processor_set_zswap_ad_ingest(&mp, policy_ingest_port, NULL);
    msg_processor_set_zswap_ad_save(&mp, policy_save_port, f);
    bool processed = msg_process_messages(&mp, &node);
    bool drained = node.recv_msg_count == 0;
    if (!drained) net_message_free(&message);
    zcl_mutex_destroy(&node.cs_recv);
    return processed && drained && !node.disconnect;
}

static int policy_framed_ingress(struct policy_fixture *f)
{
    int failures = 0;
    policy_arguments(NULL);
    CHECK("default consumes refused frame without disconnect", policy_receive(f));
    CHECK("default frame does not reach persistence", f->saves == 0);
    policy_arguments("-marketplace=1");
    CHECK("opt-in same frame reaches handler", policy_receive(f));
    CHECK("live persistence control", f->saves == 1);
    struct zswap_yardsale_ad before, after;
    CHECK("frame projection is readable", db_zswap_ad_find(&f->db, f->ad.quote_root, &before));
    policy_arguments("-marketplace=0");
    CHECK("off consumes a repeated valid frame", policy_receive(f));
    CHECK("off frame cannot update the row", f->saves == 1 &&
        db_zswap_ad_find(&f->db, f->ad.quote_root, &after) &&
        before.seen_count == after.seen_count);
    policy_arguments("-marketplace=1");
    return failures;
}

static bool policy_queue_quote(struct policy_fixture *f, struct p2p_node *node)
{
    /* A sentinel delays only the optimistic drain, leaving production framing,
     * encryption admission and segment accounting unchanged. */
    struct send_segment sentinel = {0};
    node->send_head = node->send_tail = &sentinel;
    const unsigned char magic[4] = {0x24, 0xe9, 0x27, 0x64};
    bool begun = p2p_node_begin_message(node, "zswapquote", magic);
    bool queued = false;
    if (begun) {
        p2p_node_write_message_data(node, f->wire, sizeof(f->wire));
        queued = p2p_node_end_message(node);
    }
    node->send_head = sentinel.next;
    if (!node->send_head) node->send_tail = NULL;
    return queued && node->send_head && node->send_size > 0;
}

static void policy_queue_close(struct p2p_node *node, platform_socket_t receiver)
{
    if (node->socket != ZCL_INVALID_SOCKET) platform_socket_close(node->socket);
    platform_socket_close(receiver);
    while (node->send_head) {
        struct send_segment *segment = node->send_head;
        node->send_head = segment->next;
        send_segment_free(segment);
    }
    zcl_mutex_destroy(&node->cs_send);
}

static int policy_queued_delivery(struct policy_fixture *f, bool refuse)
{
    int failures = 0;
    platform_socket_t sockets[2];
    if (!platform_socket_pair(sockets)) {
        printf("marketplace_policy: queue socket fixture FAILED\n");
        return 1;
    }
    struct p2p_node node = {0};
    node.socket = sockets[0];
    atomic_store(&node.state, PEER_ACTIVE);
    zcl_mutex_init(&node.cs_send);
    CHECK("queue receiver is nonblocking", platform_socket_set_nonblocking(sockets[1], true));
    CHECK("real signed quote queued", policy_queue_quote(f, &node));
    if (refuse) CHECK("queued root refused before drain", market_participation_refuse(f->ad.quote_root).ok);
    zcl_mutex_lock(&node.cs_send);
    socket_send_data(&node);
    zcl_mutex_unlock(&node.cs_send);
    uint8_t observed[MSG_HEADER_SIZE + ZSWAP_QUOTE_WIRE_BYTES];
    ssize_t received = recv(sockets[1], (char *)observed, sizeof(observed), 0);
    if (refuse) {
        CHECK("refusal prevents queued payload delivery", received <= 0 && node.send_bytes == 0);
        CHECK("stale frame retires connection without corrupting framing", node.disconnect);
    } else {
        CHECK("live queue observation receives exact signed bytes",
            received == (ssize_t)sizeof(observed) &&
            memcmp(observed + MSG_HEADER_SIZE, f->wire, sizeof(f->wire)) == 0);
    }
    policy_queue_close(&node, sockets[1]);
    return failures;
}

static int policy_refusal_and_alternate_path(struct policy_fixture *f)
{
    int failures = 0;
    size_t queued = 0;
    CHECK("refusal succeeds", market_participation_refuse(f->ad.quote_root).ok);
    CHECK("refused signed quote stays out of cache", policy_ingest(f) == ZSWAP_YARDSALE_INGEST_REFUSED);
    CHECK("refused quote cannot persist directly", !db_zswap_ad_save(&f->db, &f->ad));
    CHECK("refused root removed from cache", zswap_yardsale_count(f->now) == 0);
    CHECK("refused frame is consumed without re-ingestion", policy_receive(f) && f->saves == 1);
    struct zswap_yardsale_ad back;
    CHECK("refused root removed from database", !db_zswap_ad_find(&f->db, f->ad.quote_root, &back));
    CHECK("refused outbound has no queued bytes", !policy_send("zswapquote", f->wire, sizeof(f->wire), &queued) && queued == 0);
    CHECK("legacy root refusal succeeds", market_participation_refuse(f->free_offer.root_hash).ok);
    struct byte_stream stream;
    stream_init(&stream, 512);
    stream_write_u8(&stream, 1);
    bool encoded = file_offer_serialize(&f->free_offer, &stream);
    CHECK("alternate legacy wire is valid", encoded && !stream.error);
    CHECK("refused root cannot re-enter unsigned carrier", !marketplace_wire_allowed("zfilelist", stream.data, stream.size));
    CHECK("refused root cannot leave unsigned carrier", !policy_send("zfilelist", stream.data, stream.size, &queued) && queued == 0);
    CHECK("refused root cannot recache", !file_market_add_offer(&f->free_offer));
    CHECK("refused root cannot persist", !db_file_offer_save(&f->db, &f->free_offer));
    stream_free(&stream);
    return failures;
}

static int policy_restart_and_errors(struct policy_fixture *f)
{
    int failures = 0;
    rpc_market_set_state(NULL);
    node_db_close(&f->db);
    CHECK("database reopens", node_db_open(&f->db, f->path));
    rpc_market_set_state(&f->db);
    CHECK("quote refusal survives restart", !marketplace_root_allowed(f->ad.quote_root));
    CHECK("legacy refusal survives restart", !marketplace_root_allowed(f->free_offer.root_hash));
    uint8_t other[32] = {0x73};
    CHECK("unrefused root remains available", marketplace_root_allowed(other));
    marketplace_set_filters(NULL, NULL);
    CHECK("absent policy ports refuse", !marketplace_root_allowed(other));
    rpc_market_set_state(&f->db);
    policy_arguments("-marketplace=invalid");
    CHECK("invalid option does not opt in", !marketplace_enabled());
    policy_arguments("-nomarketplace");
    CHECK("negative option disables", !marketplace_enabled());
    policy_arguments("-marketplace=0");
    CHECK("disabled mode can still remove", market_participation_refuse(other).ok);
    return failures;
}

static int policy_http_and_blockchain(struct policy_fixture *f)
{
    int failures = 0;
    uint8_t response[4096] = {0};
    policy_arguments(NULL);
    size_t n = yardsale_site_handle_request("GET", "/yardsale", NULL, 0, response, sizeof(response));
    CHECK("default Yardsale HTTP refuses", n > 0 && strstr((char *)response, "403 Forbidden"));
    n = store_handle_request("GET", "/store/products", NULL, 0, response, sizeof(response), f->dir);
    CHECK("default store HTTP refuses", n > 0 && strstr((char *)response, "403 Forbidden"));
    n = api_handle_request("GET", "/api/v1/market", NULL, 0, response, sizeof(response));
    CHECK("default market API refuses", n > 0 && strstr((char *)response, "403 Forbidden"));
    const char *commands[] = {"version", "verack", "ping", "getheaders", "headers", "block", "tx", "zfileaddr", "getdata"};
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        size_t queued = 0;
        CHECK(commands[i], policy_send(commands[i], NULL, 0, &queued) && queued > 0);
    }
    CHECK("RPC mining unaffected by method policy", !marketplace_rpc("getblocktemplate"));
    CHECK("RPC normal status unaffected", !marketplace_rpc("getblockchaininfo"));
    CHECK("ROM seed registration unaffected", !marketplace_rpc("romseed_register"));
    CHECK("file service path unaffected", !marketplace_path_disabled("/block_index.bin"));
    CHECK("bootstrap path unaffected", !marketplace_path_disabled("/bundles/consensus-state-bundle-3056758.sqlite"));
    return failures;
}

static bool policy_refuse_rpc(struct rpc_table *table, const char *root,
                              const char *mode, const char *token,
                              struct json_value *result)
{
    struct json_value args, value;
    json_init(&args);
    json_init(&value);
    json_set_array(&args);
    const char *strings[] = {root, mode, token};
    for (size_t i = 0; i < 3; i++) {
        json_set_str(&value, strings[i]);
        json_push_back(&args, &value);
    }
    bool ok = rpc_table_execute(table, "zmarket_refuse", &args, result);
    json_free(&value);
    json_free(&args);
    return ok;
}

static int policy_operator_rpc(void)
{
    int failures = 0;
    struct rpc_table table;
    rpc_table_init(&table);
    register_market_rpc_commands(&table);
    set_rpc_warmup_finished();
    struct json_value result;
    json_init(&result);
    policy_arguments(NULL);
    CHECK("disabled resident RPC refuses listing view", !rpc_table_execute(&table, "zmarket_list", NULL, &result));
    json_free(&result);
    json_init(&result);
    CHECK("disabled resident RPC keeps ROM seed catalog", rpc_table_execute(&table, "romseed_list", NULL, &result));
    json_free(&result);
    json_init(&result);
    uint8_t root[32] = {0x99};
    char hex[65], token[65] = "";
    zcl_hex_encode(root, 32, hex);
    CHECK("off mode can plan root refusal", policy_refuse_rpc(&table, hex, "plan", "", &result));
    const char *planned = json_get_str(json_get(&result, "plan_token"));
    if (planned) snprintf(token, sizeof(token), "%s", planned);
    CHECK("plan yields exact token", strlen(token) == 64);
    json_free(&result);
    json_init(&result);
    CHECK("wrong refusal token is rejected", !policy_refuse_rpc(&table, hex, "commit", "bad", &result));
    policy_arguments("-marketplace=1");
    CHECK("uncommitted plan preserves listing admission", marketplace_root_allowed(root));
    policy_arguments("-marketplace=0");
    json_free(&result);
    json_init(&result);
    CHECK("exact refusal commit works while off", policy_refuse_rpc(&table, hex, "commit", token, &result));
    policy_arguments("-marketplace=1");
    CHECK("committed refusal is effective on re-enable", !marketplace_root_allowed(root));
    json_free(&result);
    return failures;
}

int test_marketplace_policy(void)
{
    struct policy_fixture f;
    if (!policy_fixture_open(&f)) {
        printf("marketplace_policy: fixture setup FAILED\n");
        policy_fixture_close(&f);
        return 1;
    }
    int failures = policy_default_and_opt_in(&f);
    failures += policy_framed_ingress(&f);
    failures += policy_queued_delivery(&f, false);
    failures += policy_queued_delivery(&f, true);
    failures += policy_refusal_and_alternate_path(&f);
    failures += policy_restart_and_errors(&f);
    failures += policy_http_and_blockchain(&f);
    failures += policy_operator_rpc();
    policy_fixture_close(&f);
    return failures;
}
