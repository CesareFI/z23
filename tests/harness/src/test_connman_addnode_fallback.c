/* Copyright 2026 Rhett Creighton - Apache License 2.0 */

#include "platform/time_compat.h"
#include "test/test_core.h"
#include "coins/undo.h"
#include "net/connman.h"
#include "net/tor_integration.h"
#include "net/onion_v3_address.h"
#include "storage/census_read.h"
#include "util/blocker.h"
#include <unistd.h>

#include "test/test_connman_addnode_fallback_priv.h"

void test_set_ipv4(struct net_address *addr,
                          uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                          uint16_t port)
{
    net_address_init(addr);
    addr->svc.addr.ip[10] = 0xff;
    addr->svc.addr.ip[11] = 0xff;
    addr->svc.addr.ip[12] = a;
    addr->svc.addr.ip[13] = b;
    addr->svc.addr.ip[14] = c;
    addr->svc.addr.ip[15] = d;
    addr->svc.port = port;
}

struct p2p_node *add_test_peer(struct connman *cm,
                                      uint8_t a, uint8_t b,
                                      uint8_t c, uint8_t d,
                                      enum peer_state state,
                                      bool inbound,
                                      bool disconnect)
{
    if (!cm)
        return NULL;
    if (!cm->manager.nodes) {
        cm->manager.nodes = zcl_calloc(8, sizeof(*cm->manager.nodes),
                                       "connman_test_nodes");
        cm->manager.nodes_cap = 8;
    }
    struct net_address addr;
    test_set_ipv4(&addr, a, b, c, d, 8033);
    struct p2p_node *node = p2p_node_create(
        &cm->manager, ZCL_INVALID_SOCKET, &addr, "connman-test", inbound);
    if (!node)
        return NULL;
    node->state = state;
    node->disconnect = disconnect;
    node->starting_height = 3117074;
    node->services = NODE_NETWORK;
    cm->manager.nodes[cm->manager.num_nodes++] = node;
    return node;
}

/* Force an addrman entry (matched by its IPv4 first octet ip[12], which is
 * unique per address in the dialer-backoff test below) into a chosen
 * consecutive-failure state: `attempts` drives connman_addrman_retry_cooldown,
 * `last_try` places the entry inside or outside its cooldown window. Returns
 * true iff an entry with that octet was found. Direct field pokes are the
 * standard fixture idiom in this file (see "addrman repeated failures cool
 * down"): they set the exact ledger state a real run reaches after N failed
 * dials without needing to actually drive N dials. */
bool test_addrman_set_fail(struct connman *cm, uint8_t first_octet,
                                  int attempts, int64_t last_try)
{
    for (int i = 0; i < cm->manager.addrman.id_count; i++) {
        struct addr_info *info = &cm->manager.addrman.entries[i];
        if (!info->used || !net_addr_is_ipv4(&info->addr.svc.addr))
            continue;
        if (info->addr.svc.addr.ip[12] == first_octet) {
            info->attempts = attempts;
            info->last_try = last_try;
            info->last_success = 0;   /* never handshook = dead-on-arrival */
            return true;
        }
    }
    return false;
}

static int test_dial_cadence_after_clock_rollback(void)
{
    printf("connman_addnode_fallback: dial cadences recover after clock "
           "rollback... ");
    bool ok = connman_dial_interval_elapsed_for_test(900, 1000, 60) &&
              connman_dial_interval_elapsed_for_test(1060, 1000, 60) &&
              !connman_dial_interval_elapsed_for_test(1059, 1000, 60);
    if (ok) printf("OK\n");
    else printf("FAIL\n");
    return ok ? 0 : 1;
}

static int test_addnode_cooldown_after_clock_rollback(void)
{
    printf("connman_addnode_fallback: addnode cooldown recovers after clock "
           "rollback... ");
    chain_params_select(CHAIN_MAIN);
    const struct chain_params *params = chain_params_get();
    struct connman cm;
    struct node_signals sigs;
    memset(&sigs, 0, sizeof(sigs));
    bool ok = connman_init(&cm, params, &sigs);

    if (ok) {
        struct net_address addr;
        test_set_ipv4(&addr, 203, 0, 113, 90, 8033);
        cm.addnodes[cm.num_addnodes++] = addr;
        cm.addnode_backoff_sec[0] = 60;
        cm.addnode_last_attempt[0] =
            (int64_t)platform_time_wall_time_t() + 3600;

        struct addr_info pick;
        enum connman_outbound_target_source source = CONNMAN_TARGET_NONE;
        size_t addnode_index = SIZE_MAX;
        memset(&pick, 0, sizeof(pick));
        ok = connman_pick_next_outbound_target(&cm,
                                               &cm.next_addnode_cursor,
                                               &pick,
                                               &source,
                                               &addnode_index) &&
             source == CONNMAN_TARGET_ADDNODE && addnode_index == 0;
    }

    connman_free(&cm);
    if (ok) printf("OK\n");
    else printf("FAIL\n");
    return ok ? 0 : 1;
}

int test_connman_addnode_fallback(void)
{
    int failures = 0;
    failures += test_dial_cadence_after_clock_rollback();
    failures += test_addnode_cooldown_after_clock_rollback();
    failures += check_connman_addnode_dht_hint_priority_dial();
    failures += check_connman_addnode_loopback_edges_distinct();
    failures += check_connman_addnode_custom_port_onion_redial();
    failures += check_connman_addnode_zcl23_backoff_ownership();
    failures += check_connman_addnode_addnodes_drain_before_addrman();
    failures += check_connman_addnode_remove_compacts_state();
    failures += check_connman_addnode_outbound_health_diversity();
    failures += check_connman_addnode_non_network_not_floor();
    failures += check_connman_addnode_cold_start_discovery_floor();
    failures += check_connman_addnode_max_height_ignores_unusable();
    failures += check_connman_addnode_peer_floor_diverse_subnets();
    failures += check_connman_addnode_addrman_skips_saturated();
    failures += check_connman_addnode_inbound_ephemeral_not_block();
    failures += check_connman_addnode_prehandshake_protocol_backoff();
    failures += check_connman_addnode_protocol_backoff_gt_tcp();
    failures += check_connman_addnode_addrman_failures_cool_down();
    failures += check_connman_addnode_failure_aware_distinct_batch();
    failures += check_connman_addnode_onion_seed_last_resort();
    failures += check_connman_addnode_ipv6_onion_diversity_caps();
    failures += check_connman_addnode_onion_queues_no_blocking();
    failures += check_connman_addnode_reactor_clamps_over_cap();
    failures += check_connman_addnode_reactor_passthrough_in_cap();
    failures += check_connman_addnode_reactor_refuses_exhausted();
    failures += check_connman_addnode_reactor_passes_default();
    failures += check_connman_addnode_retire_both_thresholds();
    failures += check_connman_addnode_retire_needs_both_gates();
    failures += check_connman_addnode_retire_suppressed_below_floor();
    failures += check_connman_addnode_retire_revive_on_success();
    failures += check_connman_addnode_retire_revive_on_readd();
    failures += check_connman_addnode_census_harvest_to_addrman();
    failures += check_connman_addnode_fixed_seeds_onion_noop();
    failures += check_connman_addnode_relay_block_handshaked_only();
    failures += check_connman_addnode_noise_capability_upgrade();
    failures += check_connman_addnode_evict_inbound_when_outbound();
    failures += check_connman_addnode_inbound_only_same_ip_stays();
    failures += check_connman_addnode_operator_local_sockets_stay();
    failures += check_connman_addnode_feeler_does_not_evict();
    failures += check_connman_addnode_unreserved_dial_serial();
    return failures;
}
