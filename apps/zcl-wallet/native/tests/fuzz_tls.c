/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct { const uint8_t *data; size_t length, offset, sent; } memory_peer;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int zcl_fuzz_tls_input(const uint8_t *data, size_t size);

static int receive_bytes(void *context, unsigned char *output, size_t capacity)
{
    memory_peer *peer = context;
    if (capacity > 32768 || peer->offset > peer->length) abort();
    const size_t left = peer->length - peer->offset;
    const size_t count = capacity < left ? capacity : left;
    if (count != 0) memcpy(output, peer->data + peer->offset, count);
    peer->offset += count;
    return (int)count;
}

static int discard_bytes(void *context, const unsigned char *input, size_t length)
{
    memory_peer *peer = context;
    (void)input;
    if (length > 32768 - peer->sent) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    peer->sent += length;
    return (int)length;
}

static int public_random(void *context, unsigned char *output, size_t length)
{
    (void)context;
    if (length == 0 || length > 1024) return -1;
    /* Deterministic test-only TLS randomness. No socket, wallet, key import or
     * production callback uses this function. It is never exported. */
    memset(output, 0x42, length);
    return 0;
}

int zcl_fuzz_tls_input(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 20000) return 0;
    const size_t root_length = (size_t)data[0] * 256 + data[1];
    if (root_length > ZCL_NET_CERT_MAX || root_length > size - 2) return 0;
    zcl_net_cancel *cancel = NULL;
    if (zcl_net_cancel_create(&cancel) != ZCL_OK) return 0;
    /* Constant checked allocation, transferred only to the normal close path. */
    zcl_transport *transport = calloc(1, sizeof(*transport));
    if (transport == NULL) { zcl_net_cancel_destroy(&cancel); return 0; }
    transport->socket = -1;
    mbedtls_ssl_init(&transport->ssl);
    mbedtls_ssl_config_init(&transport->config);
    mbedtls_x509_crt_init(&transport->anchors);
    if (zcl_net_limit_start(cancel, 1000, &transport->limit) != ZCL_OK) abort();
    const zcl_trust_anchor root = {data + 2, root_length};
    zcl_endpoint endpoint = {0};
    static const char host[] = "wallet-fixture.invalid";
    memcpy(endpoint.host, host, sizeof(host) - 1);
    endpoint.host_length = sizeof(host) - 1;
    if (!zcl_tls_heap_enter(&transport->heap)) abort();
    int state = -1;
    if (zcl_tls_configure(transport, &endpoint, &root, 1) == ZCL_OK) {
        memory_peer peer = {data + 2 + root_length, size - 2 - root_length, 0, 0};
        mbedtls_ssl_conf_rng(&transport->config, public_random, NULL);
        mbedtls_ssl_set_bio(&transport->ssl, &peer, discard_bytes, receive_bytes, NULL);
        const zcl_status status = zcl_tls_handshake(transport);
        if (status == ZCL_OK && mbedtls_ssl_get_verify_result(&transport->ssl) != 0) abort();
        if (mbedtls_ssl_get_verify_result(&transport->ssl) == 0)
            state = transport->ssl.MBEDTLS_PRIVATE(state);
    }
    zcl_tls_heap_leave();
    if (transport->heap.peak > 4194304 || transport->heap.count > 8192) abort();
    zcl_transport_close(&transport);
    zcl_net_cancel_destroy(&cancel);
    return state;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    (void)zcl_fuzz_tls_input(data, size);
    return 0;
}
