/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: record the operator's explicitly named connection targets, the
 * Noise identity this node authenticated at each of them, and decide whether
 * an inbound session proven to carry that identity may serve headers.
 * The rule and its security argument live in services/configured_sync_peers.h. */

// supervisor-ok:bounded-identity-probe — the only thread is a one-shot identity probe with bounded socket timeouts, joined before the next one starts
// one-result-type-ok:configured-sync-peer-predicates — exported bools are pure membership/eligibility answers, not fallible operations
#include "services/configured_sync_peers.h"

#include "sync/sync_planner.h"
#include "net/netbase.h"
#include "net/noise_transport.h"
#include "base/cleanse.h"
#include "platform/socket_compat.h"
#include "platform/time_compat.h"
#include "util/log_macros.h"
#include "util/safe_alloc.h"
#include "util/thread_registry.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bounds for one identity probe: TCP connect, then each handshake read or
 * write. XX message 2 is 96 bytes; anything past PROBE_MAX_BYTES without an
 * established session is not a Noise responder. */
#define PROBE_CONNECT_TIMEOUT_MS 5000
#define PROBE_IO_TIMEOUT_MS      5000
#define PROBE_MAX_BYTES          1024

struct configured_target {
    struct net_service svc;
    bool have_identity;
    uint8_t identity[32];
    bool probe_running;
    bool probe_finished_once;
    int64_t probe_finished_s;
};

static pthread_mutex_t g_configured_lock = PTHREAD_MUTEX_INITIALIZER;
static struct configured_target g_configured[CONFIGURED_SYNC_PEERS_MAX];
static size_t g_configured_count;
/* Lock-free hint for the per-tick outbound observation fast path. */
static _Atomic size_t g_configured_count_hint;
static const struct net_manager *g_network;
static configured_sync_peer_prober_fn g_test_prober;
static int64_t (*g_test_clock)(void);

static int64_t configured_now_s(void)
{
    if (g_test_clock)
        return g_test_clock();
    return platform_time_monotonic_us() / 1000000;
}

/* A target whose address cannot authenticate an inbound source is never
 * recorded: Tor names have no inbound IP, loopback is where every Tor
 * hidden-service stream arrives from, and an unspecified address matches
 * nothing real. */
static bool configured_address_usable(const struct net_addr *ip)
{
    return ip && net_addr_is_valid(ip) && !net_addr_is_tor(ip) &&
           !net_addr_is_local(ip);
}

static struct configured_target *find_target_locked(
    const struct net_service *svc)
{
    for (size_t i = 0; i < g_configured_count; i++)
        if (net_service_eq(&g_configured[i].svc, svc))
            return &g_configured[i];
    return NULL;
}

bool configured_sync_peer_note(const struct net_service *target)
{
    if (!target || !configured_address_usable(&target->addr))
        return false;  // raw-return-ok:unusable-address-is-refused-by-the-bool
    bool recorded = false;
    pthread_mutex_lock(&g_configured_lock);
    recorded = find_target_locked(target) != NULL;
    if (!recorded && g_configured_count < CONFIGURED_SYNC_PEERS_MAX) {
        struct configured_target *t = &g_configured[g_configured_count++];
        memset(t, 0, sizeof(*t));
        t->svc = *target;
        recorded = true;
    }
    atomic_store(&g_configured_count_hint, g_configured_count);
    pthread_mutex_unlock(&g_configured_lock);
    return recorded;
}

bool configured_sync_peer_forget(const struct net_service *target)
{
    if (!target)
        return false;
    bool removed = false;
    pthread_mutex_lock(&g_configured_lock);
    struct configured_target *t = find_target_locked(target);
    if (t) {
        *t = g_configured[g_configured_count - 1];
        memset(&g_configured[g_configured_count - 1], 0,
               sizeof(g_configured[0]));
        g_configured_count--;
        removed = true;
    }
    atomic_store(&g_configured_count_hint, g_configured_count);
    pthread_mutex_unlock(&g_configured_lock);
    return removed;
}

bool configured_sync_peer_ip_matches(const struct net_addr *ip)
{
    if (!configured_address_usable(ip))
        return false;  // raw-return-ok:unusable-address-never-matches
    bool match = false;
    pthread_mutex_lock(&g_configured_lock);
    for (size_t i = 0; i < g_configured_count && !match; i++)
        match = net_addr_eq(&g_configured[i].svc.addr, ip);
    pthread_mutex_unlock(&g_configured_lock);
    return match;
}

size_t configured_sync_peer_count(void)
{
    pthread_mutex_lock(&g_configured_lock);
    size_t n = g_configured_count;
    pthread_mutex_unlock(&g_configured_lock);
    return n;
}

void configured_sync_peer_record_identity(const struct net_service *target,
                                          const uint8_t remote_static[32])
{
    if (!target || !remote_static)
        return;
    bool changed = false;
    pthread_mutex_lock(&g_configured_lock);
    struct configured_target *t = find_target_locked(target);
    if (t) {
        changed = !t->have_identity ||
                  memcmp(t->identity, remote_static, 32) != 0;
        memcpy(t->identity, remote_static, 32);
        t->have_identity = true;
    }
    pthread_mutex_unlock(&g_configured_lock);
    if (changed) {
        char addr[NET_SERVICE_STR_MAX + 1];
        net_service_to_string(target, addr, sizeof(addr));
        LOG_INFO("header_sync",
                 "configured sync peer identity authenticated on our own "
                 "outbound Noise session: target=%s static=%02x%02x%02x%02x…",
                 addr, remote_static[0], remote_static[1], remote_static[2],
                 remote_static[3]);
    }
}

bool configured_sync_peer_identity(const struct net_service *target,
                                   uint8_t out_static[32])
{
    if (!target || !out_static)
        return false;
    bool have = false;
    pthread_mutex_lock(&g_configured_lock);
    struct configured_target *t = find_target_locked(target);
    if (t && t->have_identity) {
        memcpy(out_static, t->identity, 32);
        have = true;
    }
    pthread_mutex_unlock(&g_configured_lock);
    return have;
}

/* An established Noise session's authenticated remote static, or false. */
static bool session_remote_static(const struct p2p_node *node,
                                  uint8_t out_static[32])
{
    struct noise_transport_snapshot snap;
    if (!node->transport || !noise_transport_snapshot(node->transport, &snap))
        return false;  // raw-return-ok:no-established-noise-session-is-an-answer
    memcpy(out_static, snap.remote_static, 32);
    return true;
}

void configured_sync_peer_observe_outbound(const struct p2p_node *node)
{
    if (!node || node->inbound || node->is_feeler || !node->transport ||
        atomic_load(&g_configured_count_hint) == 0)
        return;
    uint8_t remote[32];
    if (!session_remote_static(node, remote))
        return;
    bool is_target = false;
    pthread_mutex_lock(&g_configured_lock);
    is_target = find_target_locked(&node->addr.svc) != NULL;
    pthread_mutex_unlock(&g_configured_lock);
    if (is_target)
        configured_sync_peer_record_identity(&node->addr.svc, remote);
}

bool syncsvc_peer_is_configured_inbound(const struct p2p_node *node)
{
    if (!node || !node->inbound ||
        atomic_load(&g_configured_count_hint) == 0 ||
        !configured_address_usable(&node->addr.svc.addr))
        return false;  // raw-return-ok:not-a-candidate-is-an-answer
    uint8_t remote[32];
    if (!session_remote_static(node, remote))
        return false;  // raw-return-ok:unauthenticated-session-never-binds
    bool bound = false;
    pthread_mutex_lock(&g_configured_lock);
    for (size_t i = 0; i < g_configured_count && !bound; i++) {
        const struct configured_target *t = &g_configured[i];
        bound = t->have_identity &&
                net_addr_eq(&t->svc.addr, &node->addr.svc.addr) &&
                memcmp(t->identity, remote, 32) == 0;
    }
    pthread_mutex_unlock(&g_configured_lock);
    return bound;
}

bool syncsvc_peer_may_serve_headers(const struct p2p_node *node)
{
    return node && (!node->inbound || syncsvc_peer_is_configured_inbound(node));
}

bool syncsvc_configured_inbound_body_stalled(const struct p2p_node *node,
                                             int our_height,
                                             uint64_t body_received,
                                             uint64_t body_timed_out,
                                             uint64_t dark_received,
                                             uint64_t dark_timed_out,
                                             int64_t last_body_time,
                                             int64_t now_seconds)
{
    if (!syncsvc_peer_is_configured_inbound(node) ||
        node->state < PEER_SYNCING_HEADERS)
        return false;  // raw-return-ok:rules-c-d-cover-only-sync-sources
    return syncsvc_should_disconnect_body_stalled_peer(
               node, our_height, body_received, body_timed_out,
               now_seconds) ||
           syncsvc_should_disconnect_body_dark_peer(
               node, our_height, dark_received, dark_timed_out,
               last_body_time, now_seconds);
}

/* ── identity probe ───────────────────────────────────────────────────── */

void configured_sync_peers_attach_network(const struct net_manager *nm)
{
    pthread_mutex_lock(&g_configured_lock);
    g_network = nm;
    pthread_mutex_unlock(&g_configured_lock);
}

static void probe_finished(const struct net_service *target, bool ok,
                           const uint8_t remote_static[32])
{
    pthread_mutex_lock(&g_configured_lock);
    struct configured_target *t = find_target_locked(target);
    if (t) {
        t->probe_running = false;
        t->probe_finished_once = true;
        t->probe_finished_s = configured_now_s();
    }
    pthread_mutex_unlock(&g_configured_lock);
    char addr[NET_SERVICE_STR_MAX + 1];
    net_service_to_string(target, addr, sizeof(addr));
    if (ok) {
        configured_sync_peer_record_identity(target, remote_static);
        LOG_INFO("header_sync", "configured sync peer identity probe "
                 "completed Noise XX with target=%s", addr);
    } else {
        LOG_WARN("header_sync", "configured sync peer identity probe of "
                 "target=%s did not complete a Noise XX handshake; an inbound "
                 "session from its IP stays refused as a header source", addr);
    }
}

/* One probe thread at a time. It carries its own copy of the local static
 * key and network magic, so it never reads the net manager, which shutdown
 * may free while a bounded probe is still waiting on a socket. */
struct probe_job {
    size_t count;
    struct net_service targets[CONFIGURED_SYNC_PEERS_MAX];
    uint8_t identity_priv[32];
    unsigned char magic[MESSAGE_START_SIZE];
};

static pthread_mutex_t g_probe_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_probe_tid;
static bool g_probe_tid_live;
static _Atomic bool g_probe_thread_done;

static bool probe_send(platform_socket_t sock, const uint8_t *bytes,
                       size_t len)
{
    return len == 0 || platform_socket_send_all(sock, bytes, len);
}

/* Drive the initiator side until the session is established. Feeding XX
 * message 2 yields message 3, which is sent so the responder also sees a
 * completed handshake; the socket then closes before any VERSION, so the
 * responder's connection manager never evaluates it as a peer. */
static bool probe_handshake(platform_socket_t sock, struct noise_transport *t,
                            uint8_t out_static[32])
{
    uint8_t buf[256];
    size_t total = 0;
    while (total < PROBE_MAX_BYTES) {
        int n = platform_socket_receive(sock, buf, sizeof(buf));
        if (n <= 0)
            return false;  // raw-return-ok:peer-closed-or-timed-out-before-xx-msg2
        total += (size_t)n;
        uint8_t *wire = NULL, *plain = NULL;
        size_t wire_len = 0, plain_len = 0;
        bool fed = noise_transport_feed(t, buf, (size_t)n, &wire, &wire_len,
                                        &plain, &plain_len);
        bool sent = fed && probe_send(sock, wire, wire_len);
        free(wire);
        free(plain);
        if (!sent)
            return false;  // raw-return-ok:handshake-refused-is-the-probe-answer
        struct noise_transport_snapshot snap;
        if (noise_transport_snapshot(t, &snap)) {
            memcpy(out_static, snap.remote_static, 32);
            return true;
        }
    }
    return false;
}

/* Dial `target`, complete Noise XX as initiator with `identity_priv`, and
 * return the responder's authenticated static key. Blocking and bounded by
 * PROBE_CONNECT_TIMEOUT_MS plus PROBE_IO_TIMEOUT_MS per read or write. Only
 * table targets reach it, and the table admits no Tor, loopback or
 * unspecified address (configured_sync_peer_note). */
static bool probe_noise_identity(const struct net_service *target,
                                 const uint8_t identity_priv[32],
                                 const unsigned char *magic,
                                 uint8_t out_static[32])
{
    zcl_socket_t sock = ZCL_INVALID_SOCKET;
    if (!connect_socket_directly(target, &sock, PROBE_CONNECT_TIMEOUT_MS) ||
        sock == ZCL_INVALID_SOCKET)
        return false;  // raw-return-ok:unreachable-target-is-the-probe-answer
    /* connect_socket_directly leaves the socket non-blocking, where a read
     * returns at once with nothing and the timeouts below never apply. */
    if (!zcl_set_socket_nonblocking(sock, false)) {
        (void)platform_socket_close(sock);
        return false;  // raw-return-ok:unusable-socket-is-the-probe-answer
    }
    (void)platform_socket_set_receive_timeout(sock, PROBE_IO_TIMEOUT_MS);
    (void)platform_socket_set_send_timeout(sock, PROBE_IO_TIMEOUT_MS);
    uint8_t *msg1 = NULL;
    size_t msg1_len = 0;
    struct noise_transport *t =
        noise_transport_begin(true, identity_priv, magic, &msg1, &msg1_len);
    bool ok = t && probe_send(sock, msg1, msg1_len) &&
              probe_handshake(sock, t, out_static);
    free(msg1);
    noise_transport_free(t);
    (void)platform_socket_close(sock);
    return ok;
}

static void *probe_thread(void *arg)
{
    struct probe_job *job = arg;
    for (size_t i = 0; i < job->count; i++) {
        uint8_t remote[32];
        bool ok = probe_noise_identity(&job->targets[i], job->identity_priv,
                                       job->magic, remote);
        probe_finished(&job->targets[i], ok, ok ? remote : NULL);
    }
    memory_cleanse(job->identity_priv, sizeof(job->identity_priv));
    free(job);
    atomic_store(&g_probe_thread_done, true);
    return NULL;
}

static bool probe_due_locked(const struct configured_target *t, int64_t now)
{
    if (t->probe_running)
        return false;  // raw-return-ok:one-probe-per-target-at-a-time
    return !t->probe_finished_once ||
           now - t->probe_finished_s >= CONFIGURED_SYNC_PEER_PROBE_RETRY_SECS;
}

/* Mark every due target at `ip` running and copy it into `out`. */
static size_t collect_due_locked(const struct net_addr *ip,
                                 struct net_service *out, int64_t now)
{
    size_t n = 0;
    for (size_t i = 0; i < g_configured_count; i++) {
        struct configured_target *t = &g_configured[i];
        if (!net_addr_eq(&t->svc.addr, ip) || !probe_due_locked(t, now))
            continue;
        t->probe_running = true;
        out[n++] = t->svc;
    }
    return n;
}

/* The deterministic seam: the injected prober runs inline. */
static size_t request_probe_with_test_prober(const struct net_addr *ip,
                                             configured_sync_peer_prober_fn fn)
{
    struct net_service due[CONFIGURED_SYNC_PEERS_MAX];
    pthread_mutex_lock(&g_configured_lock);
    size_t n = collect_due_locked(ip, due, configured_now_s());
    pthread_mutex_unlock(&g_configured_lock);
    for (size_t i = 0; i < n; i++) {
        uint8_t remote[32];
        bool ok = fn(&due[i], remote);
        probe_finished(&due[i], ok, ok ? remote : NULL);
    }
    return n;
}

static size_t request_probe_on_thread(const struct net_addr *ip)
{
    size_t n = 0;
    pthread_mutex_lock(&g_probe_thread_lock);
    if (g_probe_tid_live && !atomic_load(&g_probe_thread_done)) {
        pthread_mutex_unlock(&g_probe_thread_lock);
        return 0;  // one probe thread at a time; the next tick retries
    }
    if (g_probe_tid_live) {
        (void)pthread_join(g_probe_tid, NULL);
        g_probe_tid_live = false;
    }
    struct probe_job *job = zcl_calloc(1, sizeof(*job), "configured_sync_probe");
    if (job) {
        pthread_mutex_lock(&g_configured_lock);
        const struct net_manager *nm = g_network;
        if (nm && nm->noise_enabled) {
            memcpy(job->identity_priv, nm->identity_priv, 32);
            memcpy(job->magic, nm->message_start, sizeof(job->magic));
            job->count = collect_due_locked(ip, job->targets,
                                            configured_now_s());
        }
        pthread_mutex_unlock(&g_configured_lock);
        n = job->count;
    }
    atomic_store(&g_probe_thread_done, false);
    // thread-supervision-ok:bounded-one-shot identity probe; caller-owned tid joined before the next probe is spawned, and each socket step is bounded by PROBE_CONNECT_TIMEOUT_MS / PROBE_IO_TIMEOUT_MS
    if (n > 0 && thread_registry_spawn("cfg-sync-probe", probe_thread, job,
                                       &g_probe_tid) == 0) {
        g_probe_tid_live = true;
        pthread_mutex_unlock(&g_probe_thread_lock);
        return n;
    }
    for (size_t i = 0; job && i < job->count; i++)
        probe_finished(&job->targets[i], false, NULL);
    if (job)
        memory_cleanse(job->identity_priv, sizeof(job->identity_priv));
    free(job);
    pthread_mutex_unlock(&g_probe_thread_lock);
    return 0;
}

size_t configured_sync_peer_request_probe(const struct p2p_node *inbound)
{
    if (!inbound || !inbound->inbound ||
        atomic_load(&g_configured_count_hint) == 0 ||
        !configured_address_usable(&inbound->addr.svc.addr))
        return 0;
    pthread_mutex_lock(&g_configured_lock);
    configured_sync_peer_prober_fn test_prober = g_test_prober;
    pthread_mutex_unlock(&g_configured_lock);
    if (test_prober)
        return request_probe_with_test_prober(&inbound->addr.svc.addr,
                                              test_prober);
    return request_probe_on_thread(&inbound->addr.svc.addr);
}

void configured_sync_peer_observe_session(const struct p2p_node *node)
{
    if (!node || atomic_load(&g_configured_count_hint) == 0)
        return;
    if (!node->inbound)
        configured_sync_peer_observe_outbound(node);
    else if (!syncsvc_peer_is_configured_inbound(node))
        (void)configured_sync_peer_request_probe(node);
}

/* ── test seams ───────────────────────────────────────────────────────── */

void configured_sync_peers_set_prober_for_testing(
    configured_sync_peer_prober_fn prober)
{
    pthread_mutex_lock(&g_configured_lock);
    g_test_prober = prober;
    pthread_mutex_unlock(&g_configured_lock);
}

void configured_sync_peers_set_clock_for_testing(int64_t (*now_seconds)(void))
{
    g_test_clock = now_seconds;
}

void configured_sync_peers_reset_for_testing(void)
{
    pthread_mutex_lock(&g_configured_lock);
    memset(g_configured, 0, sizeof(g_configured));
    g_configured_count = 0;
    atomic_store(&g_configured_count_hint, 0);
    pthread_mutex_unlock(&g_configured_lock);
}

bool configured_sync_peer_probe_socket_for_testing(
    const struct net_service *target, const uint8_t identity_priv[32],
    const unsigned char magic[4], uint8_t out_static[32])
{
    if (!target || !identity_priv || !magic || !out_static)
        return false;  // raw-return-ok:missing-argument-is-a-refused-probe
    return probe_noise_identity(target, identity_priv, magic, out_static);
}
