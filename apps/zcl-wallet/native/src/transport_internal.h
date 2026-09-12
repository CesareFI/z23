/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSPORT_INTERNAL_H
#define ZCL_TRANSPORT_INTERNAL_H
#include "zcl_transport.h"
#include "transport_alloc.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

struct zcl_transport {
    zcl_tls_heap heap;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt anchors;
    zcl_net_limit limit;
    int socket;
    zcl_status fault;
    bool verified;
    size_t received, sent, random_bytes;
    uint8_t plaintext[16384];
};

zcl_status zcl_net_now(uint64_t *milliseconds);
zcl_status zcl_net_host(const uint8_t *input, size_t length, uint8_t *canonical);
zcl_status zcl_net_wait(zcl_transport *transport, short events);
zcl_status zcl_net_connect(zcl_transport *transport, const zcl_endpoint *endpoint);
void zcl_net_socket_close(zcl_transport *transport);
int zcl_net_send(void *context, const unsigned char *data, size_t length);
int zcl_net_receive(void *context, unsigned char *data, size_t length);
zcl_status zcl_tls_configure(zcl_transport *transport, const zcl_endpoint *endpoint,
                             const zcl_trust_anchor *anchors, size_t anchor_count);
zcl_status zcl_tls_handshake(zcl_transport *transport);
zcl_status zcl_tls_retry(zcl_transport *transport, int result);
zcl_status zcl_net_fail(zcl_transport *transport, zcl_status status);
#endif
