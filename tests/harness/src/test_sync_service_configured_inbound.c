/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: deterministic regression for the two-node mutual-dial deadlock
 * and the configured-inbound header-sync rule (services/configured_sync_peers.h).
 *
 * Each model node owns a real struct connman. A connection is a real
 * outbound p2p_node on the dialer and a real inbound p2p_node on the
 * listener, joined by a real in-memory Noise XX handshake with each node's
 * static key. The sealed eviction (connman_evict_same_ip_inbound_when_outbound)
 * runs at exactly the two points msg_version.c calls it: the listener's
 * inbound VERSION and the dialer's VERACK. The header-sync decision is the
 * product entry point syncsvc_begin_peer_sync. The identity prober and the
 * clocks are injected, so no model case depends on sockets or wall-clock
 * timing. The probe cases run the real prober against loopback listeners
 * (Noise, silent, trickling, holding) and the real probe thread with a gated
 * prober; every wait there is bounded, and none is a sleep.
 * Part of the sync_service group (test_sync_service.c calls the entry). */

#include "test/test_core.h"
#include "chain/chainparams.h"
#include "net/connman.h"
#include "net/net.h"
#include "net/noise_transport.h"
#include "services/configured_sync_peers.h"
#include "sync/sync_planner.h"
#include "sync/sync_state.h"
#include "platform/socket_compat.h"
#include "platform/time_compat.h"
#include "util/safe_alloc.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MODEL_MAX_NODES 4
#define MODEL_MAX_CONNS 8

struct model_node {
    const char *name;
    uint8_t ip[4];
    uint16_t port;
    uint8_t priv[32];
    uint8_t pub[32];
    bool up;
    struct connman cm;
};

struct model_conn {
    struct model_node *dialer;
    struct model_node *listener;
    struct p2p_node *out;   /* on dialer->cm */
    struct p2p_node *in;    /* on listener->cm */
    bool verack_sent;       /* the listener processed VERSION and replied */
};

static struct model_node *g_model[MODEL_MAX_NODES];
static size_t g_model_count;
static int64_t g_model_now = 1000;
static int g_probe_calls;
static uint8_t g_probe_priv[32];

static int64_t model_clock(void) { return g_model_now; }

static void model_key(uint8_t out[32], uint8_t seed)
{
    for (int i = 0; i < 32; i++)
        out[i] = (uint8_t)(seed * 31 + i * 7 + 1);
}

/* A real Noise XX handshake between two static keys, in memory. */
static bool noise_pair(const uint8_t init_priv[32], const uint8_t resp_priv[32],
                       struct noise_transport **init_out,
                       struct noise_transport **resp_out)
{
    const unsigned char *magic = chain_params_get()->pchMessageStart;
    uint8_t *m1 = NULL, *m2 = NULL, *m3 = NULL, *m4 = NULL;
    uint8_t *p = NULL;
    size_t m1n = 0, m2n = 0, m3n = 0, m4n = 0, pn = 0;
    struct noise_transport *i =
        noise_transport_begin(true, init_priv, magic, &m1, &m1n);
    struct noise_transport *r =
        noise_transport_begin(false, resp_priv, magic, NULL, NULL);
    bool ok = i && r &&
              noise_transport_feed(r, m1, m1n, &m2, &m2n, &p, &pn);
    free(p); p = NULL;
    ok = ok && noise_transport_feed(i, m2, m2n, &m3, &m3n, &p, &pn);
    free(p); p = NULL;
    ok = ok && noise_transport_feed(r, m3, m3n, &m4, &m4n, &p, &pn);
    free(p);
    free(m1); free(m2); free(m3); free(m4);
    struct noise_transport_snapshot si, sr;
    ok = ok && noise_transport_snapshot(i, &si) &&
         noise_transport_snapshot(r, &sr);
    if (!ok) {
        noise_transport_free(i);
        noise_transport_free(r);
        return false;
    }
    *init_out = i;
    *resp_out = r;
    return true;
}

static bool model_public_key(const uint8_t priv[32], uint8_t pub[32])
{
    uint8_t other[32];
    model_key(other, 250);
    struct noise_transport *i = NULL, *r = NULL;
    if (!noise_pair(other, priv, &i, &r))
        return false;
    struct noise_transport_snapshot si;
    bool ok = noise_transport_snapshot(i, &si);
    if (ok)
        memcpy(pub, si.remote_static, 32);
    noise_transport_free(i);
    noise_transport_free(r);
    return ok;
}

/* The injected prober: complete XX against the node listening at `target`. */
static bool model_prober(const struct net_service *target, uint8_t out[32])
{
    g_probe_calls++;
    for (size_t k = 0; k < g_model_count; k++) {
        struct model_node *m = g_model[k];
        struct net_service svc;
        memset(&svc, 0, sizeof(svc));
        net_addr_set_ipv4(&svc.addr, m->ip);
        svc.port = m->port;
        if (!m->up || !net_service_eq(&svc, target))
            continue;
        struct noise_transport *i = NULL, *r = NULL;
        if (!noise_pair(g_probe_priv, m->priv, &i, &r))
            return false;
        struct noise_transport_snapshot si;
        bool ok = noise_transport_snapshot(i, &si);
        if (ok)
            memcpy(out, si.remote_static, 32);
        noise_transport_free(i);
        noise_transport_free(r);
        return ok;
    }
    return false;
}

static bool model_node_init(struct model_node *m, const char *name,
                            uint8_t last_octet, uint16_t port, uint8_t key)
{
    memset(m, 0, sizeof(*m));
    m->name = name;
    m->ip[0] = 198; m->ip[1] = 51; m->ip[2] = 100; m->ip[3] = last_octet;
    m->port = port;
    m->up = true;
    model_key(m->priv, key);
    struct node_signals sigs;
    memset(&sigs, 0, sizeof(sigs));
    if (!connman_init(&m->cm, chain_params_get(), &sigs))
        return false;
    /* connman_init leaves the node array to its first accept; the model
     * attaches nodes directly, as the connman fixtures do. */
    m->cm.manager.nodes = zcl_calloc(MODEL_MAX_CONNS * 2,
                                     sizeof(*m->cm.manager.nodes),
                                     "configured_inbound_model_nodes");
    m->cm.manager.nodes_cap = MODEL_MAX_CONNS * 2;
    if (g_model_count < MODEL_MAX_NODES)
        g_model[g_model_count++] = m;
    return m->cm.manager.nodes && model_public_key(m->priv, m->pub);
}

static void model_reset(void)
{
    for (size_t k = 0; k < g_model_count; k++)
        connman_free(&g_model[k]->cm);
    g_model_count = 0;
    configured_sync_peers_reset_for_testing();
    configured_sync_peers_set_prober_for_testing(model_prober);
    configured_sync_peers_set_clock_for_testing(model_clock);
    g_probe_calls = 0;
    g_model_now = 1000;
    model_key(g_probe_priv, 200);
}

/* `m` configures `target` as an operator-named sync peer. */
static bool model_configure(struct model_node *m, const struct model_node *target)
{
    (void)m;  /* the table is process-wide; each model node owns a distinct
               * target set by construction in these scenarios */
    struct net_service svc;
    memset(&svc, 0, sizeof(svc));
    net_addr_set_ipv4(&svc.addr, target->ip);
    svc.port = target->port;
    return configured_sync_peer_note(&svc);
}

static struct p2p_node *model_attach(struct model_node *m,
                                     const uint8_t ip[4], uint16_t port,
                                     bool inbound,
                                     struct noise_transport *transport)
{
    struct net_address addr;
    net_address_init(&addr);
    net_addr_set_ipv4(&addr.svc.addr, ip);
    addr.svc.port = port;
    struct p2p_node *n = p2p_node_create(&m->cm.manager, ZCL_INVALID_SOCKET,
                                         &addr, m->name, inbound);
    if (!n) {
        noise_transport_free(transport);
        return NULL;
    }
    n->state = PEER_VERSION_SENT;
    n->starting_height = 100;
    n->services = NODE_NETWORK;
    n->transport = transport;
    m->cm.manager.nodes[m->cm.manager.num_nodes++] = n;
    return n;
}

/* `dialer` opens a TCP connection to `listener` presenting `key_priv` as its
 * Noise static (normally its own key; an impostor sharing the dialer's IP
 * presents another). `noise` false models a plaintext session. */
static bool model_connect(struct model_conn *c, struct model_node *dialer,
                          struct model_node *listener,
                          const uint8_t key_priv[32], bool noise,
                          uint16_t source_port)
{
    struct noise_transport *ti = NULL, *tr = NULL;
    if (noise && !noise_pair(key_priv, listener->priv, &ti, &tr))
        return false;
    c->dialer = dialer;
    c->listener = listener;
    c->out = model_attach(dialer, listener->ip, listener->port, false, ti);
    c->in = model_attach(listener, dialer->ip, source_port, true, tr);
    c->verack_sent = false;
    return c->out && c->in;
}

/* msg_version.c: the listener marks the inbound session complete after
 * processing the dialer's VERSION, then runs the sealed eviction. */
static void ev_inbound_version(struct model_conn *c)
{
    /* An inbound session already evicted never processes the VERSION, so it
     * never sends the VERSION+VERACK the dialer is waiting for. */
    if (c->in->disconnect || c->out->disconnect)
        return;
    c->in->state = PEER_HANDSHAKE_COMPLETE;
    c->verack_sent = true;
    connman_evict_same_ip_inbound_when_outbound(&c->listener->cm, c->in);
}

/* msg_version.c: the dialer completes on VERACK, then runs the eviction.
 * The VERACK exists only once the listener processed the VERSION. */
static void ev_outbound_verack(struct model_conn *c)
{
    if (!c->verack_sent || c->out->disconnect)
        return;
    c->out->state = PEER_HANDSHAKE_COMPLETE;
    connman_evict_same_ip_inbound_when_outbound(&c->dialer->cm, c->out);
}

/* A side that was disconnected closes the socket, so the other side sees a
 * remote close; msg_send_messages then promotes survivors to ACTIVE. */
static void model_settle(struct model_conn *conns, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (conns[i].in->disconnect || conns[i].out->disconnect ||
            !conns[i].listener->up || !conns[i].dialer->up) {
            conns[i].in->disconnect = true;
            conns[i].out->disconnect = true;
        }
    }
    for (size_t i = 0; i < n; i++) {
        if (!conns[i].in->disconnect &&
            conns[i].in->state == PEER_HANDSHAKE_COMPLETE)
            conns[i].in->state = PEER_ACTIVE;
        if (!conns[i].out->disconnect &&
            conns[i].out->state == PEER_HANDSHAKE_COMPLETE)
            conns[i].out->state = PEER_ACTIVE;
    }
}

/* One product tick for `n`: may it become a header source? */
static bool model_begin(struct p2p_node *n)
{
    sync_set_state(SYNC_IDLE, "configured inbound model reset");
    (void)sync_set_state(SYNC_FINDING_PEERS, "configured inbound model");
    bool began = syncsvc_begin_peer_sync(n, 0, 0);
    if (began)
        n->state = PEER_ACTIVE;
    return began;
}

/* Does `m` have at least one live session it may sync headers from? Two
 * ticks: the first may schedule the identity probe (synchronous here). */
static bool model_has_header_source(struct model_node *m)
{
    for (int tick = 0; tick < 2; tick++) {
        for (size_t i = 0; i < m->cm.manager.num_nodes; i++) {
            struct p2p_node *n = m->cm.manager.nodes[i];
            if (n->disconnect || n->state != PEER_ACTIVE)
                continue;
            if (model_begin(n))
                return true;
        }
    }
    return false;
}

static size_t model_live_sessions(const struct model_conn *conns, size_t n)
{
    size_t live = 0;
    for (size_t i = 0; i < n; i++)
        live += !conns[i].in->disconnect && conns[i].in->state == PEER_ACTIVE;
    return live;
}

/* Apply the four handshake events in `order` (0=V1 1=K1 2=V2 3=K2). A
 * VERACK already on the wire when its inbound half was evicted is delivered
 * or lost per `lose` (bit i for connection i): TCP may hand it over before
 * the close or not. */
static void apply_handshake_events(struct model_conn conns[2],
                                   const int order[4], unsigned lose)
{
    for (int e = 0; e < 4; e++) {
        struct model_conn *c = &conns[order[e] / 2];
        bool lost = c->in->disconnect && (lose & (1u << (order[e] / 2)));
        if (order[e] % 2 == 0)
            ev_inbound_version(c);
        else if (!lost)
            ev_outbound_verack(c);
    }
}

/* Two nodes that addnode each other, one dial in each direction. */
static int run_two_node_interleaving(const int order[4], unsigned lose,
                                     size_t *zero_survivor_cases)
{
    static struct model_node a, b;
    struct model_conn conns[2];
    model_reset();
    bool ok = model_node_init(&a, "A", 7, 18233, 1) &&
              model_node_init(&b, "B", 8, 18234, 2) &&
              model_configure(&a, &b) && model_configure(&b, &a) &&
              model_connect(&conns[0], &a, &b, a.priv, true, 40001) &&
              model_connect(&conns[1], &b, &a, b.priv, true, 40002);
    if (!ok) {
        model_reset();
        return 1;
    }
    apply_handshake_events(conns, order, lose);
    model_settle(conns, 2);
    int failures = 0;
    if (model_live_sessions(conns, 2) == 0) {
        (*zero_survivor_cases)++;
    } else {
        bool a_src = model_has_header_source(&a);
        bool b_src = model_has_header_source(&b);
        if (!a_src || !b_src) {
            printf("\n  order=%d%d%d%d lose=%u: A source=%d B source=%d",
                   order[0], order[1], order[2], order[3], lose, a_src, b_src);
            failures++;
        }
    }
    model_reset();
    return failures;
}


static int test_configured_inbound_every_interleaving(void)
{
    int failures = 0;
    TEST("configured inbound: every A/B dial interleaving leaves both nodes "
         "a header source whenever one session survives (A first, B first, "
         "simultaneous)") {
        static const int orders[][4] = {
            {0, 1, 2, 3}, {0, 2, 1, 3}, {0, 2, 3, 1},
            {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 3, 0, 1},
        };
        size_t zero = 0, runs = 0;
        int bad = 0;
        for (size_t o = 0; o < sizeof(orders) / sizeof(orders[0]); o++) {
            for (unsigned lose = 0; lose < 4; lose++) {
                bad += run_two_node_interleaving(orders[o], lose, &zero);
                runs++;
            }
        }
        if (bad)
            printf("\n  %d of %zu interleavings left a node without a "
                   "header source\n", bad, runs);
        /* In a truly simultaneous dial both VERACKs can land before either
         * close, and each side evicts the other's inbound: zero sessions
         * survive and the addnode redial produces one of the serial orders
         * below. Those cases carry no sync decision and are counted. */
        printf("[%zu interleavings, %zu with no surviving session] ", runs,
               zero);
        ASSERT(bad == 0);
        /* The A-first and B-first serial orders are in the set and each
         * leaves exactly one surviving session. */
        ASSERT(zero < runs);
        PASS();
    } _test_next:;
    return failures;
}

static int test_configured_inbound_serial_orders_named(void)
{
    int failures = 0;
    TEST("configured inbound: A dials first, then B dials first — the "
         "inbound-only side syncs from the proven identity") {
        for (int first = 0; first < 2; first++) {
            static struct model_node a, b;
            model_reset();
            ASSERT(model_node_init(&a, "A", 7, 18233, 1));
            ASSERT(model_node_init(&b, "B", 8, 18234, 2));
            ASSERT(model_configure(&a, &b) && model_configure(&b, &a));
            struct model_node *d1 = first == 0 ? &a : &b;
            struct model_node *d2 = first == 0 ? &b : &a;
            struct model_conn conns[2];
            ASSERT(model_connect(&conns[0], d1, d2, d1->priv, true, 40001));
            ev_inbound_version(&conns[0]);
            ev_outbound_verack(&conns[0]);
            ASSERT(model_connect(&conns[1], d2, d1, d2->priv, true, 40002));
            ev_inbound_version(&conns[1]);     /* d1 evicts d2's dial */
            model_settle(conns, 2);
            ASSERT(conns[1].in->disconnect && conns[1].out->disconnect);
            ASSERT(!conns[0].in->disconnect);
            /* d2 holds only d1's inbound session: the deadlock shape. */
            ASSERT(model_has_header_source(d1));
            ASSERT(model_has_header_source(d2));
            ASSERT(g_probe_calls == 1);
            ASSERT(syncsvc_peer_is_configured_inbound(conns[0].in));
            model_reset();
        }
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

static int test_configured_inbound_reconnect_reproves(void)
{
    int failures = 0;
    TEST("configured inbound: a reconnect proves the identity again; an "
         "impostor at the configured IP and a plaintext session are refused") {
        static struct model_node a, b, impostor;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_node_init(&impostor, "X", 7, 18299, 9)); /* A's IP */
        impostor.up = false;  /* never answers at the configured address */
        ASSERT(model_configure(&b, &a));
        struct model_conn c[4];
        ASSERT(model_connect(&c[0], &a, &b, a.priv, true, 40001));
        ev_inbound_version(&c[0]);
        ev_outbound_verack(&c[0]);
        model_settle(c, 1);
        ASSERT(model_has_header_source(&b));
        ASSERT(g_probe_calls == 1);

        /* Drop. The same IP comes back with another static key. */
        c[0].in->disconnect = c[0].out->disconnect = true;
        ASSERT(model_connect(&c[1], &a, &b, impostor.priv, true, 40002));
        ev_inbound_version(&c[1]);
        model_settle(&c[1], 1);
        c[1].out->state = PEER_ACTIVE;
        ASSERT(!syncsvc_peer_is_configured_inbound(c[1].in));
        ASSERT(!model_begin(c[1].in));
        ASSERT(g_probe_calls == 1);          /* inside the retry window */
        g_model_now += CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
        ASSERT(!model_begin(c[1].in));       /* re-probe still finds A's key */
        ASSERT(g_probe_calls == 2);
        ASSERT(!model_begin(c[1].in));
        ASSERT(g_probe_calls == 2);          /* bounded: one per window */

        /* A plaintext session from the configured IP never binds. */
        c[1].in->disconnect = c[1].out->disconnect = true;
        ASSERT(model_connect(&c[2], &a, &b, a.priv, false, 40003));
        ev_inbound_version(&c[2]);
        model_settle(&c[2], 1);
        ASSERT(!syncsvc_peer_is_configured_inbound(c[2].in));
        ASSERT(!model_begin(c[2].in));

        /* A's own key over a fresh session binds with no new probe. */
        c[2].in->disconnect = c[2].out->disconnect = true;
        ASSERT(model_connect(&c[3], &a, &b, a.priv, true, 40004));
        ev_inbound_version(&c[3]);
        model_settle(&c[3], 1);
        ASSERT(model_begin(c[3].in));
        ASSERT(g_probe_calls == 2);
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

static int test_configured_inbound_publisher_loss(void)
{
    int failures = 0;
    TEST("configured inbound: after A disappears, B still syncs from its "
         "other configured peer C") {
        static struct model_node a, b, cnode;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_node_init(&cnode, "C", 9, 18235, 3));
        ASSERT(model_configure(&b, &a) && model_configure(&b, &cnode));
        ASSERT(model_configure(&cnode, &b));
        struct model_conn c[4];
        /* A and C both win their races against B: B is inbound-only. */
        ASSERT(model_connect(&c[0], &a, &b, a.priv, true, 40001));
        ev_inbound_version(&c[0]);
        ev_outbound_verack(&c[0]);
        ASSERT(model_connect(&c[1], &cnode, &b, cnode.priv, true, 40002));
        ev_inbound_version(&c[1]);
        ev_outbound_verack(&c[1]);
        ASSERT(model_connect(&c[2], &b, &cnode, b.priv, true, 40003));
        ev_inbound_version(&c[2]);          /* C evicts B's dial */
        model_settle(c, 3);
        ASSERT(model_has_header_source(&b));

        /* The publisher goes away. */
        a.up = false;
        model_settle(c, 3);
        ASSERT(c[0].in->disconnect);
        ASSERT(!c[1].in->disconnect);
        ASSERT(syncsvc_peer_is_configured_inbound(c[1].in) ||
               model_begin(c[1].in));
        ASSERT(model_has_header_source(&b));
        ASSERT(model_has_header_source(&cnode));
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

static int test_configured_inbound_refusals(void)
{
    int failures = 0;
    TEST("configured inbound: unconfigured, mismatched, non-ACTIVE, "
         "loopback and onion inbound sessions are refused") {
        static struct model_node a, b, stranger;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_node_init(&stranger, "S", 66, 18236, 4));
        ASSERT(model_configure(&b, &a));
        struct model_conn c[3];

        /* Unconfigured: a stranger's authenticated session. */
        ASSERT(model_connect(&c[0], &stranger, &b, stranger.priv, true, 40001));
        ev_inbound_version(&c[0]);
        model_settle(c, 1);
        ASSERT(!model_begin(c[0].in));
        ASSERT(g_probe_calls == 0);

        /* Configured IP, identity that is not the one at the address. */
        ASSERT(model_connect(&c[1], &a, &b, stranger.priv, true, 40002));
        ev_inbound_version(&c[1]);
        model_settle(&c[1], 1);
        ASSERT(!model_begin(c[1].in));
        ASSERT(g_probe_calls == 1);
        uint8_t learned[32];
        struct net_service target;
        memset(&target, 0, sizeof(target));
        net_addr_set_ipv4(&target.addr, a.ip);
        target.port = a.port;
        ASSERT(configured_sync_peer_identity(&target, learned));
        ASSERT(memcmp(learned, a.pub, 32) == 0);

        /* The right identity, but not PEER_ACTIVE. */
        ASSERT(model_connect(&c[2], &a, &b, a.priv, true, 40003));
        c[2].in->state = PEER_HANDSHAKE_COMPLETE;
        ASSERT(syncsvc_peer_is_configured_inbound(c[2].in));
        ASSERT(!syncsvc_should_begin_peer_sync(c[2].in, 0, 0,
                                               SYNC_FINDING_PEERS));
        c[2].in->state = PEER_ACTIVE;
        ASSERT(syncsvc_should_begin_peer_sync(c[2].in, 0, 0,
                                              SYNC_FINDING_PEERS));

        /* Loopback and Tor targets are never recorded, so a loopback or
         * onion inbound never binds. */
        struct net_service loop;
        memset(&loop, 0, sizeof(loop));
        net_addr_set_ipv4(&loop.addr, (const unsigned char[4]){127, 0, 0, 1});
        loop.port = 18233;
        ASSERT(!configured_sync_peer_note(&loop));
        struct net_service onion;
        memset(&onion, 0, sizeof(onion));
        onion.addr.has_torv3 = true;
        onion.addr.torv3[0] = 0x5a;
        ASSERT(!configured_sync_peer_note(&onion));
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

static int test_configured_inbound_same_limits_as_outbound(void)
{
    int failures = 0;
    TEST("configured inbound: request cadence, block assignment, stale-header "
         "and body-stall rules match an outbound twin") {
        static struct model_node a, b;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_configure(&b, &a));
        struct model_conn c[2];
        ASSERT(model_connect(&c[0], &a, &b, a.priv, true, 40001));
        ev_inbound_version(&c[0]);
        model_settle(c, 1);
        ASSERT(model_begin(c[0].in));
        struct p2p_node *in = c[0].in;
        /* An outbound twin at the same heights and state. */
        ASSERT(model_connect(&c[1], &b, &a, b.priv, true, 40002));
        struct p2p_node *out = c[1].out;
        static const int heights[] = {0, 50, 100, 5000};
        static const enum peer_state states[] = {
            PEER_SYNCING_HEADERS, PEER_SYNCING_BLOCKS,
        };
        for (size_t s = 0; s < 2; s++) {
            for (size_t h = 0; h < 4; h++) {
                in->state = out->state = states[s];
                in->starting_height = out->starting_height = 100000;
                in->time_connected = out->time_connected = 1;
                in->last_getheaders_time = out->last_getheaders_time = 0;
                ASSERT(syncsvc_should_request_headers(in, heights[h], 500) ==
                       syncsvc_should_request_headers(out, heights[h], 500));
                struct sync_block_assignment pi, po;
                syncsvc_plan_block_assignment(&pi, in, 3, heights[h]);
                syncsvc_plan_block_assignment(&po, out, 3, heights[h]);
                ASSERT(pi.should_assign == po.should_assign);
                ASSERT(pi.max_assign == po.max_assign);
                ASSERT(syncsvc_should_disconnect_stale_header_peer(
                           in, heights[h], heights[h], 0, 500) ==
                       syncsvc_should_disconnect_stale_header_peer(
                           out, heights[h], heights[h], 0, 500));
                /* Rules C and D: the configured inbound peer answers exactly
                 * what the core's outbound evaluation answers. */
                int64_t now = 1000000;
                in->time_connected = out->time_connected =
                    now - (SYNC_BODY_STALL_TIMEOUT_SECS + 5);
                ASSERT(syncsvc_configured_inbound_body_stalled(
                           in, heights[h], 0, SYNC_BODY_STALL_MIN_TIMEOUTS,
                           0, SYNC_BODY_STALL_MIN_TIMEOUTS, 0, now) ==
                       (syncsvc_should_disconnect_body_stalled_peer(
                            out, heights[h], 0, SYNC_BODY_STALL_MIN_TIMEOUTS,
                            now) ||
                        syncsvc_should_disconnect_body_dark_peer(
                            out, heights[h], 0, SYNC_BODY_STALL_MIN_TIMEOUTS,
                            0, now)));
            }
        }
        /* The same deadbeat evidence against an unconfigured inbound peer
         * changes nothing: the core exempts it and so does this rule. */
        in->state = PEER_SYNCING_HEADERS;
        ASSERT(syncsvc_configured_inbound_body_stalled(
            in, 0, 0, SYNC_BODY_STALL_MIN_TIMEOUTS, 0,
            SYNC_BODY_STALL_MIN_TIMEOUTS, 0, 1000000));
        configured_sync_peers_reset_for_testing();
        ASSERT(!syncsvc_configured_inbound_body_stalled(
            in, 0, 0, SYNC_BODY_STALL_MIN_TIMEOUTS, 0,
            SYNC_BODY_STALL_MIN_TIMEOUTS, 0, 1000000));
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

static int test_configured_inbound_outbound_unchanged(void)
{
    int failures = 0;
    TEST("configured inbound: outbound begin-sync answers do not depend on "
         "the configured set") {
        static struct model_node a, b;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        struct model_conn c;
        ASSERT(model_connect(&c, &b, &a, b.priv, true, 40001));
        static const enum sync_state states[] = {
            SYNC_IDLE, SYNC_FINDING_PEERS, SYNC_HEADERS_DOWNLOAD,
            SYNC_BLOCKS_DOWNLOAD, SYNC_AT_TIP,
        };
        static const int heights[] = {0, 99, 100, 101};
        static const enum peer_state peer_states[] = {
            PEER_HANDSHAKE_COMPLETE, PEER_ACTIVE, PEER_SYNCING_HEADERS,
        };
        for (size_t ps = 0; ps < 3; ps++) {
            for (size_t st = 0; st < 5; st++) {
                for (size_t h = 0; h < 4; h++) {
                    c.out->state = peer_states[ps];
                    configured_sync_peers_reset_for_testing();
                    bool b0 = syncsvc_should_begin_peer_sync(
                        c.out, heights[h], heights[h], states[st]);
                    bool r0 = syncsvc_should_request_headers(
                        c.out, heights[h], 1000);
                    ASSERT(model_configure(&b, &a));
                    configured_sync_peer_observe_outbound(c.out);
                    ASSERT(syncsvc_should_begin_peer_sync(
                               c.out, heights[h], heights[h], states[st]) ==
                           b0);
                    ASSERT(syncsvc_should_request_headers(
                               c.out, heights[h], 1000) == r0);
                    ASSERT(!syncsvc_peer_is_configured_inbound(c.out));
                }
            }
        }
        /* The outbound session to the exact target taught its identity. */
        uint8_t learned[32];
        struct net_service target;
        memset(&target, 0, sizeof(target));
        net_addr_set_ipv4(&target.addr, a.ip);
        target.port = a.port;
        ASSERT(configured_sync_peer_identity(&target, learned));
        ASSERT(memcmp(learned, a.pub, 32) == 0);
        ASSERT(g_probe_calls == 0);
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

/* ── the network prober's socket path, over loopback ─────────────────── */

enum listener_mode {
    LISTEN_NOISE,    /* answer as the Noise XX responder */
    LISTEN_CLOSE,    /* accept, then close without a byte */
    LISTEN_TRICKLE,  /* answer XX message 2 one byte at a time */
    LISTEN_HOLD,     /* accept and never send, until the probe closes */
};

struct probe_listener {
    platform_socket_t fd;
    uint16_t port;
    enum listener_mode mode;
    uint8_t priv[32];
    bool established;          /* the responder saw XX message 3 */
    uint8_t seen_static[32];   /* the initiator static it authenticated */
    size_t trickled;           /* bytes of message 2 sent in trickle mode */
    size_t msg2_len;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    bool accepted;
};

static bool probe_listener_open(struct probe_listener *l)
{
    pthread_mutex_init(&l->mu, NULL);
    pthread_cond_init(&l->cv, NULL);
    l->fd = platform_socket_open(AF_INET, SOCK_STREAM, 0, true, false);
    if (l->fd == PLATFORM_SOCKET_INVALID)
        return false;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    size_t slen = sizeof(sa);
    if (platform_socket_bind(l->fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 ||
        platform_socket_listen(l->fd, 4) != 0 ||
        platform_socket_local_address(l->fd, (struct sockaddr *)&sa,
                                      &slen) != 0)
        return false;
    l->port = ntohs(sa.sin_port);
    return l->port != 0;
}

/* Answer one connection as the Noise XX responder, as net_listen.c arms an
 * inbound session, until the handshake completes or the peer closes. */
static void probe_listener_respond(struct probe_listener *l,
                                   platform_socket_t s)
{
    struct noise_transport *t = noise_transport_begin(
        false, l->priv, chain_params_get()->pchMessageStart, NULL, NULL);
    uint8_t buf[256];
    while (t) {
        int n = platform_socket_receive(s, buf, sizeof(buf));
        uint8_t *wire = NULL, *plain = NULL;
        size_t wire_len = 0, plain_len = 0;
        bool fed = n > 0 && noise_transport_feed(t, buf, (size_t)n, &wire,
                                                 &wire_len, &plain,
                                                 &plain_len);
        if (fed && wire_len)
            fed = platform_socket_send_all(s, wire, wire_len);
        free(wire);
        free(plain);
        struct noise_transport_snapshot snap;
        if (fed && noise_transport_snapshot(t, &snap)) {
            l->established = true;
            memcpy(l->seen_static, snap.remote_static, 32);
        }
        if (!fed || l->established)
            break;
    }
    noise_transport_free(t);
}

/* Read XX message 1, then send the real message 2 one byte per 50 ms of
 * silence from the probe; stop as soon as the probe closes or answers. */
static void probe_listener_trickle(struct probe_listener *l,
                                   platform_socket_t s)
{
    struct noise_transport *t = noise_transport_begin(
        false, l->priv, chain_params_get()->pchMessageStart, NULL, NULL);
    uint8_t buf[256];
    uint8_t *wire = NULL, *plain = NULL;
    size_t wire_len = 0, plain_len = 0;
    int n = platform_socket_receive(s, buf, sizeof(buf));
    bool fed = t && n > 0 && noise_transport_feed(t, buf, (size_t)n, &wire,
                                                  &wire_len, &plain,
                                                  &plain_len);
    l->msg2_len = fed ? wire_len : 0;
    for (size_t i = 0; fed && i < wire_len; i++) {
        if (platform_socket_wait_readable(s, 50) != 0 ||
            platform_socket_send(s, wire + i, 1) != 1)
            break;
        l->trickled++;
    }
    free(wire);
    free(plain);
    noise_transport_free(t);
}

/* Hold the connection open without a byte until the probe closes it. */
static void probe_listener_hold(platform_socket_t s)
{
    uint8_t buf[64];
    for (int i = 0; i < 30; i++) {
        if (platform_socket_wait_readable(s, 1000) > 0 &&
            platform_socket_receive(s, buf, sizeof(buf)) <= 0)
            return;
    }
}

static void *probe_listener_main(void *arg)
{
    struct probe_listener *l = arg;
    struct sockaddr_in peer;
    size_t plen = sizeof(peer);
    platform_socket_t s =
        platform_socket_accept(l->fd, (struct sockaddr *)&peer, &plen);
    pthread_mutex_lock(&l->mu);
    l->accepted = s != PLATFORM_SOCKET_INVALID;
    pthread_cond_broadcast(&l->cv);
    pthread_mutex_unlock(&l->mu);
    if (s == PLATFORM_SOCKET_INVALID)
        return NULL;
    (void)platform_socket_set_receive_timeout(s, 5000);
    if (l->mode == LISTEN_NOISE)
        probe_listener_respond(l, s);
    else if (l->mode == LISTEN_TRICKLE)
        probe_listener_trickle(l, s);
    else if (l->mode == LISTEN_HOLD)
        probe_listener_hold(s);
    (void)platform_socket_close(s);
    return NULL;
}

static void probe_listener_finish(struct probe_listener *l, pthread_t tid)
{
    /* Wakes a listener still blocked in accept when the dial never came. */
    (void)platform_socket_shutdown_both(l->fd);
    (void)pthread_join(tid, NULL);
    (void)platform_socket_close(l->fd);
    pthread_mutex_destroy(&l->mu);
    pthread_cond_destroy(&l->cv);
}

static void loopback_target(struct net_service *target, uint16_t port)
{
    memset(target, 0, sizeof(*target));
    net_addr_set_ipv4(&target->addr, (const unsigned char[4]){127, 0, 0, 1});
    target->port = port;
}

/* Run the real prober against one loopback listener. */
static bool probe_loopback(struct probe_listener *l, const uint8_t probe_priv[32],
                           uint8_t out[32])
{
    pthread_t tid;
    if (!probe_listener_open(l) ||
        pthread_create(&tid, NULL, probe_listener_main, l) != 0)
        return false;
    struct net_service target;
    loopback_target(&target, l->port);
    bool ok = configured_sync_peer_probe_socket_for_testing(
        &target, probe_priv, chain_params_get()->pchMessageStart, out);
    probe_listener_finish(l, tid);
    return ok;
}

static int test_configured_inbound_probe_socket(void)
{
    int failures = 0;
    TEST("configured inbound: the identity probe completes Noise XX over a "
         "real socket and returns the responder's static key; a listener "
         "that never speaks Noise yields no identity") {
        uint8_t probe_priv[32], probe_pub[32], out[32];
        model_key(probe_priv, 201);
        ASSERT(model_public_key(probe_priv, probe_pub));

        struct probe_listener noise_l;
        memset(&noise_l, 0, sizeof(noise_l));
        noise_l.mode = LISTEN_NOISE;
        model_key(noise_l.priv, 1);
        uint8_t want[32];
        ASSERT(model_public_key(noise_l.priv, want));
        memset(out, 0, sizeof(out));
        ASSERT(probe_loopback(&noise_l, probe_priv, out));
        ASSERT(memcmp(out, want, 32) == 0);
        /* The responder completed too, and authenticated the probe key. */
        ASSERT(noise_l.established);
        ASSERT(memcmp(noise_l.seen_static, probe_pub, 32) == 0);

        struct probe_listener mute_l;
        memset(&mute_l, 0, sizeof(mute_l));
        mute_l.mode = LISTEN_CLOSE;
        ASSERT(!probe_loopback(&mute_l, probe_priv, out));
        PASS();
    } _test_next:;
    return failures;
}

/* A probe clock that moves one second per reading. */
static _Atomic int64_t g_fake_probe_ms;
static int64_t fake_probe_clock_ms(void)
{
    return atomic_fetch_add(&g_fake_probe_ms, 1000) + 1000;
}

static int test_configured_inbound_probe_deadline(void)
{
    int failures = 0;
    TEST("configured inbound: a target that trickles XX message 2 one byte "
         "at a time ends the probe at its absolute deadline") {
        uint8_t probe_priv[32], out[32];
        model_key(probe_priv, 201);
        struct probe_listener l;
        memset(&l, 0, sizeof(l));
        l.mode = LISTEN_TRICKLE;
        model_key(l.priv, 1);
        atomic_store(&g_fake_probe_ms, 0);
        configured_sync_peers_set_probe_clock_ms_for_testing(fake_probe_clock_ms);
        int64_t real_start = platform_time_monotonic_us();
        bool ok = probe_loopback(&l, probe_priv, out);
        int64_t real_ms = (platform_time_monotonic_us() - real_start) / 1000;
        configured_sync_peers_set_probe_clock_ms_for_testing(NULL);
        ASSERT(!ok);
        /* The deadline, not the peer, ended it: bytes kept arriving, the
         * whole message never did, and the probe clock passed the deadline. */
        ASSERT(l.msg2_len > 0);
        ASSERT(l.trickled > 0 && l.trickled < l.msg2_len);
        ASSERT(atomic_load(&g_fake_probe_ms) >=
               CONFIGURED_SYNC_PEER_PROBE_DEADLINE_MS);
        ASSERT(real_ms < CONFIGURED_SYNC_PEER_PROBE_DEADLINE_MS);
        PASS();
    } _test_next:;
    configured_sync_peers_set_probe_clock_ms_for_testing(NULL);
    return failures;
}

struct socket_probe_run {
    struct net_service target;
    uint8_t priv[32];
    bool ok;
};

static void *socket_probe_main(void *arg)
{
    struct socket_probe_run *run = arg;
    uint8_t out[32];
    run->ok = configured_sync_peer_probe_socket_for_testing(
        &run->target, run->priv, chain_params_get()->pchMessageStart, out);
    return NULL;
}

static bool listener_wait_accepted(struct probe_listener *l)
{
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 5;
    pthread_mutex_lock(&l->mu);
    while (!l->accepted &&
           pthread_cond_timedwait(&l->cv, &l->mu, &until) == 0) {
    }
    bool accepted = l->accepted;
    pthread_mutex_unlock(&l->mu);
    return accepted;
}

static int test_configured_inbound_probe_stop(void)
{
    int failures = 0;
    TEST("configured inbound: a stop request ends a probe waiting on a silent "
         "target within a wait slice") {
        struct probe_listener l;
        memset(&l, 0, sizeof(l));
        l.mode = LISTEN_HOLD;
        pthread_t ltid, ptid;
        ASSERT(probe_listener_open(&l));
        ASSERT(pthread_create(&ltid, NULL, probe_listener_main, &l) == 0);
        struct socket_probe_run run;
        memset(&run, 0, sizeof(run));
        loopback_target(&run.target, l.port);
        model_key(run.priv, 201);
        bool started = pthread_create(&ptid, NULL, socket_probe_main, &run) == 0;
        bool accepted = started && listener_wait_accepted(&l);
        int64_t stop_at = platform_time_monotonic_us();
        configured_sync_peers_stop();
        if (started)
            (void)pthread_join(ptid, NULL);
        int64_t stop_ms = (platform_time_monotonic_us() - stop_at) / 1000;
        probe_listener_finish(&l, ltid);
        configured_sync_peers_reset_for_testing();
        ASSERT(started && accepted);
        ASSERT(!run.ok);
        /* Far inside the per-target deadline the probe would otherwise use. */
        ASSERT(stop_ms < 2000);
        PASS();
    } _test_next:;
    configured_sync_peers_reset_for_testing();
    return failures;
}

/* A threaded test prober held at a gate until the test opens it or a stop
 * request arrives. */
static pthread_mutex_t g_gate_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_gate_cv = PTHREAD_COND_INITIALIZER;
static bool g_gate_open;
static int g_gate_calls;
static bool g_gate_saw_stop;

static bool gate_prober(const struct net_service *target, uint8_t out[32])
{
    pthread_mutex_lock(&g_gate_mu);
    g_gate_calls++;
    pthread_cond_broadcast(&g_gate_cv);
    while (!g_gate_open && !configured_sync_peers_probe_should_stop()) {
        struct timespec until;
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_nsec += 10 * 1000 * 1000;
        if (until.tv_nsec >= 1000000000L) {
            until.tv_sec++;
            until.tv_nsec -= 1000000000L;
        }
        (void)pthread_cond_timedwait(&g_gate_cv, &g_gate_mu, &until);
    }
    g_gate_saw_stop = configured_sync_peers_probe_should_stop();
    pthread_mutex_unlock(&g_gate_mu);
    return !g_gate_saw_stop && model_prober(target, out);
}

static void gate_set(bool open)
{
    pthread_mutex_lock(&g_gate_mu);
    g_gate_open = open;
    pthread_cond_broadcast(&g_gate_cv);
    pthread_mutex_unlock(&g_gate_mu);
}

static bool gate_wait_calls(int calls)
{
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 5;
    pthread_mutex_lock(&g_gate_mu);
    while (g_gate_calls < calls &&
           pthread_cond_timedwait(&g_gate_cv, &g_gate_mu, &until) == 0) {
    }
    bool reached = g_gate_calls >= calls;
    pthread_mutex_unlock(&g_gate_mu);
    return reached;
}

static int test_configured_inbound_probe_thread(void)
{
    int failures = 0;
    TEST("configured inbound: one probe thread at a time, joined before the "
         "next spawns, and joined by stop while it waits") {
        static struct model_node a, b, x;
        model_reset();
        configured_sync_peers_set_prober_for_testing(NULL);
        configured_sync_peers_set_threaded_prober_for_testing(gate_prober);
        g_gate_calls = 0;
        g_gate_saw_stop = false;
        gate_set(false);
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_node_init(&x, "X", 7, 18299, 9));
        x.up = false;
        ASSERT(model_configure(&b, &a));
        struct model_conn c[2];
        ASSERT(model_connect(&c[0], &a, &b, a.priv, true, 40001));
        ASSERT(model_connect(&c[1], &a, &b, x.priv, true, 40002));
        c[0].in->state = c[1].in->state = PEER_ACTIVE;

        ASSERT(configured_sync_peer_request_probe(c[0].in) == 1);
        ASSERT(gate_wait_calls(1));
        ASSERT(configured_sync_peer_request_probe(c[0].in) == 0);  /* busy */
        ASSERT(!syncsvc_peer_is_configured_inbound(c[0].in));
        gate_set(true);
        ASSERT(configured_sync_peers_join_probe_for_testing());
        ASSERT(syncsvc_peer_is_configured_inbound(c[0].in));

        /* The impostor session asks again after the retry spacing: a new
         * thread spawns, learns A's key again, and the impostor stays out. */
        g_model_now += CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
        ASSERT(configured_sync_peer_request_probe(c[1].in) == 1);
        ASSERT(configured_sync_peers_join_probe_for_testing());
        ASSERT(g_gate_calls == 2);
        ASSERT(!syncsvc_peer_is_configured_inbound(c[1].in));

        /* Stop ends a probe held at the gate and joins its thread. */
        gate_set(false);
        g_model_now += CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
        ASSERT(configured_sync_peer_request_probe(c[1].in) == 1);
        ASSERT(gate_wait_calls(3));
        configured_sync_peers_stop();
        ASSERT(g_gate_saw_stop);
        ASSERT(!configured_sync_peers_join_probe_for_testing());
        g_model_now += CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
        ASSERT(configured_sync_peer_request_probe(c[1].in) == 0);
        PASS();
    } _test_next:;
    gate_set(true);
    configured_sync_peers_set_threaded_prober_for_testing(NULL);
    model_reset();
    return failures;
}

static int test_configured_inbound_revocation(void)
{
    int failures = 0;
    TEST("configured inbound: addnode remove revokes a bound session at the "
         "next check, and a probe that learns a new key revokes the old one") {
        static struct model_node a, b, a2;
        model_reset();
        ASSERT(model_node_init(&a, "A", 7, 18233, 1));
        ASSERT(model_node_init(&b, "B", 8, 18234, 2));
        ASSERT(model_configure(&b, &a));
        struct model_conn c[2];
        ASSERT(model_connect(&c[0], &a, &b, a.priv, true, 40001));
        ev_inbound_version(&c[0]);
        model_settle(c, 1);
        ASSERT(model_begin(c[0].in));
        struct net_service target;
        memset(&target, 0, sizeof(target));
        net_addr_set_ipv4(&target.addr, a.ip);
        target.port = a.port;
        ASSERT(configured_sync_peer_forget(&target));
        ASSERT(!syncsvc_peer_is_configured_inbound(c[0].in));
        ASSERT(!model_begin(c[0].in));

        /* Re-added; then the host at A's address changes its key. */
        ASSERT(model_configure(&b, &a));
        ASSERT(model_begin(c[0].in));
        a.up = false;
        ASSERT(model_node_init(&a2, "A2", 7, 18233, 11));
        ASSERT(model_connect(&c[1], &a2, &b, a2.priv, true, 40002));
        ev_inbound_version(&c[1]);
        model_settle(&c[1], 1);
        g_model_now += CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
        ASSERT(model_begin(c[1].in));
        ASSERT(!syncsvc_peer_is_configured_inbound(c[0].in));
        PASS();
    } _test_next:;
    model_reset();
    return failures;
}

int check_sync_service_configured_inbound(void)
{
    int failures = 0;
    chain_params_select(CHAIN_REGTEST);
    failures += test_configured_inbound_every_interleaving();
    failures += test_configured_inbound_serial_orders_named();
    failures += test_configured_inbound_reconnect_reproves();
    failures += test_configured_inbound_publisher_loss();
    failures += test_configured_inbound_refusals();
    failures += test_configured_inbound_same_limits_as_outbound();
    failures += test_configured_inbound_outbound_unchanged();
    failures += test_configured_inbound_probe_socket();
    failures += test_configured_inbound_probe_deadline();
    failures += test_configured_inbound_probe_stop();
    failures += test_configured_inbound_probe_thread();
    failures += test_configured_inbound_revocation();
    model_reset();
    configured_sync_peers_set_prober_for_testing(NULL);
    configured_sync_peers_set_clock_for_testing(NULL);
    sync_set_state(SYNC_IDLE, "configured inbound done");
    return failures;
}
