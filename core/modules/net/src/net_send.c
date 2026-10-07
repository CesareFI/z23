/* Copyright (c) 2009-2010 Satoshi Nakamoto
 * Copyright (c) 2009-2014 The Bitcoin Core developers
 * Copyright 2026 Rhett Creighton - Apache License 2.0
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 * Purpose: drain framed send segments under transport and operator policy. */
#include "net/net.h"
#include "net/marketplace.h"
#include "net/peer_lifecycle.h"
#include "platform/socket_compat.h"
#include "core/utiltime.h"
#include "util/log_macros.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

/* --- socket_send_data --- */

static bool send_segment_write(struct p2p_node *node,
                                const struct send_segment *segment,
                                size_t remain, ssize_t *sent)
{
    if (segment->marketplace_generation == 0) {
        *sent = send(node->socket, (const char *)segment->data + node->send_offset,
                     remain, MSG_NOSIGNAL | MSG_DONTWAIT);
        return true;
    }
    /* Serialize the actual nonblocking write with refusal, including a
     * partially written segment. Skipping encrypted records or a partial v1
     * frame would corrupt framing, so retire this connection instead. */
    marketplace_lock();
    if (!marketplace_generation_current(segment->marketplace_generation)) {
        marketplace_unlock();
        (void)p2p_node_request_disconnect(node, P2P_DISCONNECT_POLICY_ROTATION,
            P2P_DISCONNECT_SOURCE_PEER_POLICY, node->endpoint_generation);
        LOG_WARN("market", "pending marketplace send invalidated by local refusal");
        return false;
    }
    *sent = send(node->socket, (const char *)segment->data + node->send_offset,
                 remain, MSG_NOSIGNAL | MSG_DONTWAIT);
    marketplace_unlock();
    return true;
}

void socket_send_data(struct p2p_node *node)
{
    while (node->send_head) {
        struct send_segment *seg = node->send_head;
        size_t remain = seg->size - node->send_offset;
        ssize_t sent;
        if (!send_segment_write(node, seg, remain, &sent)) return;
        if (sent > 0) {
            node->last_send = GetTime();
            node->send_bytes += (uint64_t)sent;
            node->send_offset += (size_t)sent;

            if (node->send_offset >= seg->size) {
                node->send_head = seg->next;
                if (!node->send_head)
                    node->send_tail = NULL;
                node->send_size -= seg->size;
                node->send_offset = 0;
                send_segment_free(seg);
            } else {
                break;
            }
        } else {
            if (sent < 0) {
                int err = platform_socket_last_error(); /* Winsock reports here, never errno */
                if (!platform_socket_error_would_block(err) && !platform_socket_error_interrupted(err) &&
                    !platform_socket_error_in_progress(err))
                    p2p_node_close_socket(node);
            }
            break;
        }
    }
}
