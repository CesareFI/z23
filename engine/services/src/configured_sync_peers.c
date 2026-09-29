/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: record the operator's explicitly named connection targets and
 * decide whether an inbound peer from one of them may serve headers.
 * The rule and its security argument live in services/configured_sync_peers.h. */

// one-result-type-ok:configured-sync-peer-predicates — exported bools are pure membership/eligibility answers, not fallible operations
#include "services/configured_sync_peers.h"

#include <pthread.h>
#include <string.h>

static pthread_mutex_t g_configured_lock = PTHREAD_MUTEX_INITIALIZER;
static struct net_service g_configured[CONFIGURED_SYNC_PEERS_MAX];
static size_t g_configured_count;

/* A target whose address cannot authenticate an inbound source is never
 * recorded: Tor names have no inbound IP, loopback is where every Tor
 * hidden-service stream arrives from, and an unspecified address matches
 * nothing real. */
static bool configured_address_usable(const struct net_addr *ip)
{
    return ip && net_addr_is_valid(ip) && !net_addr_is_tor(ip) &&
           !net_addr_is_local(ip);
}

bool configured_sync_peer_note(const struct net_service *target)
{
    if (!target || !configured_address_usable(&target->addr))
        return false;  // raw-return-ok:unusable-address-is-refused-by-the-bool
    bool recorded = false;
    pthread_mutex_lock(&g_configured_lock);
    for (size_t i = 0; i < g_configured_count && !recorded; i++)
        recorded = net_service_eq(&g_configured[i], target);
    if (!recorded && g_configured_count < CONFIGURED_SYNC_PEERS_MAX) {
        g_configured[g_configured_count++] = *target;
        recorded = true;
    }
    pthread_mutex_unlock(&g_configured_lock);
    return recorded;
}

bool configured_sync_peer_forget(const struct net_service *target)
{
    if (!target)
        return false;
    bool removed = false;
    pthread_mutex_lock(&g_configured_lock);
    for (size_t i = 0; i < g_configured_count; i++) {
        if (!net_service_eq(&g_configured[i], target))
            continue;
        g_configured[i] = g_configured[g_configured_count - 1];
        memset(&g_configured[g_configured_count - 1], 0,
               sizeof(g_configured[0]));
        g_configured_count--;
        removed = true;
        break;
    }
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
        match = net_addr_eq(&g_configured[i].addr, ip);
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

void configured_sync_peers_reset_for_testing(void)
{
    pthread_mutex_lock(&g_configured_lock);
    memset(g_configured, 0, sizeof(g_configured));
    g_configured_count = 0;
    pthread_mutex_unlock(&g_configured_lock);
}

bool syncsvc_peer_is_configured_inbound(const struct p2p_node *node)
{
    return node && node->inbound &&
           configured_sync_peer_ip_matches(&node->addr.svc.addr);
}

bool syncsvc_peer_may_serve_headers(const struct p2p_node *node)
{
    return node && (!node->inbound || syncsvc_peer_is_configured_inbound(node));
}
