/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: the peers this node's operator explicitly named as connection
 * targets, and the one rule that lets such a peer serve headers over an
 * inbound connection.
 *
 * WHY THIS EXISTS. Two routable nodes told to addnode each other end up with
 * one TCP connection: the sealed connection manager evicts a same-IP inbound
 * once its own outbound to that IP completes. The node left holding only the
 * other's inbound connection never began header sync, because an inbound peer
 * may not become a header source (anti-eclipse), and FINDING_PEERS has no
 * edge to AT_TIP. It stayed out of sync forever.
 *
 * THE RULE. An inbound peer may be a header source only when all of these
 * hold (syncsvc_peer_is_configured_inbound):
 *   - its remote IP equals the IP of a target this node's operator named with
 *     -addnode=, -connect=, -addnode-file= or the authenticated `addnode
 *     add|onetry` RPC;
 *   - it is not a Tor address and not a loopback/unspecified address. Every
 *     Tor hidden-service inbound arrives from 127.0.0.1, so a loopback source
 *     proves nothing about who dialled. Loopback peers are never evicted by
 *     the connection manager anyway, so they never hit the deadlock;
 *   - it passes every other begin-sync condition an outbound peer must pass,
 *     including PEER_ACTIVE (VERSION/VERACK complete, not disconnecting).
 *
 * WHY THIS IS SECURITY-EQUIVALENT. Our outbound dial to a configured target
 * is authenticated by nothing but that address: whoever answers at the IP is
 * accepted once the handshake completes. A completed inbound TCP handshake
 * from the same IP proves the peer sends and receives at that IP, which is
 * the same proof. The port is not compared: an inbound source port is
 * ephemeral, and the advertised listening port in VERSION is self-reported,
 * so matching either would add no authentication. The rule only ADDS a
 * header source the operator already chose; it never removes, displaces or
 * outranks an outbound source, headers stay fully validated, and every
 * stall/misbehaviour rule still applies to the peer.
 *
 * Addresses learned from DHT hints, addr gossip, seeds, anchors or an onion
 * directory walk are never recorded here. */

#ifndef ZCL_SERVICES_CONFIGURED_SYNC_PEERS_H
#define ZCL_SERVICES_CONFIGURED_SYNC_PEERS_H

#include "net/net.h"
#include "net/netaddr.h"

#include <stdbool.h>
#include <stddef.h>

/* Capacity mirrors the connection manager's MAX_ADDNODES scale. A full table
 * refuses the new entry (the dial still happens; only the inbound header
 * exemption is withheld). */
#define CONFIGURED_SYNC_PEERS_MAX 64

/* Record an operator-named target. Returns true when it is recorded (or was
 * already present), false for NULL or a full table. */
bool configured_sync_peer_note(const struct net_service *target);

/* Forget one operator-named target (`addnode remove`). Returns true when an
 * entry was removed. */
bool configured_sync_peer_forget(const struct net_service *target);

/* True when `ip` is the IP of at least one recorded target. */
bool configured_sync_peer_ip_matches(const struct net_addr *ip);

size_t configured_sync_peer_count(void);

/* Test-only: empty the table. */
void configured_sync_peers_reset_for_testing(void);

/* The inbound half of the rule above. False for outbound peers. */
bool syncsvc_peer_is_configured_inbound(const struct p2p_node *node);

/* An outbound peer, or an inbound peer that satisfies the rule above. */
bool syncsvc_peer_may_serve_headers(const struct p2p_node *node);

#endif
