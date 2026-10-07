/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSPORT_H
#define ZCL_TRANSPORT_H
#include "zcl_wallet.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_NET_HOST_MAX ((size_t)253)
#define ZCL_NET_ADDRESSES_MAX ((size_t)8)
#define ZCL_NET_ANCHORS_MAX ((size_t)256)
#define ZCL_NET_CERT_MAX ((size_t)4096)
#define ZCL_NET_TRUST_MAX ((size_t)524288)

typedef struct zcl_net_cancel zcl_net_cancel;
typedef struct zcl_transport zcl_transport;
typedef struct {
    const zcl_net_cancel *cancel;
    uint64_t deadline_ms;
} zcl_net_limit;
typedef struct {
    uint8_t bytes[16];
    uint8_t length; /* 4 for IPv4, 16 for IPv6; no scoped/link-local override. */
} zcl_net_address;
typedef struct {
    uint8_t host[ZCL_NET_HOST_MAX];
    size_t host_length;
    uint16_t port;
    zcl_net_address addresses[ZCL_NET_ADDRESSES_MAX];
    size_t address_count;
} zcl_endpoint;
typedef struct {
    const uint8_t *der;
    size_t length;
} zcl_trust_anchor;

/* One explicit owner per allocated cancel/transport. Initialize output handles
 * to NULL; creation never overwrites a live handle. Destroy clears that handle.
 * A cancel must outlive every limit/operation/transport borrowing it. Request
 * is the ONLY operation allowed concurrently; never concurrently destroy.
 * Transport operations are synchronous on one worker, never the Android UI.
 * Stable, nonoverlapping caller spans are borrowed only for the call. */
zcl_status zcl_net_cancel_create(zcl_net_cancel **output);
void zcl_net_cancel_request(zcl_net_cancel *cancel);
void zcl_net_cancel_destroy(zcl_net_cancel **cancel);
/* A single 1..60000 ms monotonic budget covers resolve/connect/handshake/I/O. */
zcl_status zcl_net_limit_start(const zcl_net_cancel *cancel, uint32_t milliseconds,
                               zcl_net_limit *limit);
zcl_status zcl_net_limit_check(const zcl_net_limit *limit);

/* Canonical printable ASCII DNS names only; labels 1..63, no trailing dot,
 * control characters, URLs, wildcards, numeric-only names or embedded NUL. Output
 * host is lowercase. OS getaddrinfo may block beyond the deadline: cancellation
 * and timeout are enforced again when it returns. The adapter must allow at
 * most one pending resolver and must never wait for it on the UI thread.
 * Retain the first eight distinct supported addresses among the first 32 OS entries,
 * preserving order; equal bytes in different address families remain distinct. */
zcl_status zcl_endpoint_resolve(const uint8_t *host, size_t host_length, uint16_t port,
                                const zcl_net_limit *limit, zcl_endpoint *endpoint);

/* Trusted roots are explicit DER certificate spans, never fetched from a peer.
 * An endpoint may be resolved in advance; TLS still verifies its exact DNS host.
 * Only successful required chain/hostname/time/profile verification publishes
 * a transport. No insecure mode, fallback or client certificate is provided.
 * The transport owns its copied TLS configuration/roots, socket and buffers.
 * It borrows the limit's cancel until close; other arguments need not survive.
 * TLS allocations are capped at 4 MiB including allocation headers and 8192
 * live blocks. Provider allocations are zeroed before release. Linux/Android
 * sockets require atomic nonblocking/CLOEXEC creation and MSG_NOSIGNAL.
 */
zcl_status zcl_transport_open(const zcl_endpoint *endpoint, const zcl_trust_anchor *anchors,
                               size_t anchor_count, const zcl_net_limit *limit,
                               zcl_transport **output);
/* Read/write are limited to 16384/256 bytes per call. Reads preserve output
 * and length on failure; writes may have reached the peer before an error.
 * Every I/O failure poisons this connection until close. Only authenticated
 * application bytes are exposed. The whole connection is capped at 256 KiB
 * incoming and 32 KiB outgoing wire bytes, including TLS negotiation. */
zcl_status zcl_transport_read(zcl_transport *transport, uint8_t *output, size_t capacity,
                               size_t *length);
zcl_status zcl_transport_write(zcl_transport *transport, const uint8_t *input, size_t length);
void zcl_transport_close(zcl_transport **transport);
#ifdef __cplusplus
}
#endif
#endif
