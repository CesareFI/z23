/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_internal.h"
#include "zcl_keys.h"
#include <stdlib.h>

static zcl_status endpoint_valid(const zcl_endpoint *endpoint)
{
    if (endpoint == NULL || endpoint->port == 0) return ZCL_INVALID_ARGUMENT;
    if (endpoint->address_count == 0 || endpoint->address_count > ZCL_NET_ADDRESSES_MAX)
        return ZCL_OUT_OF_RANGE;
    uint8_t host[ZCL_NET_HOST_MAX];
    const zcl_status status = zcl_net_host(endpoint->host, endpoint->host_length, host);
    if (status != ZCL_OK) return status;
    for (size_t i = 0; i < endpoint->address_count; ++i) {
        if (endpoint->addresses[i].length != 4 && endpoint->addresses[i].length != 16)
            return ZCL_INVALID_ENCODING;
    }
    return ZCL_OK;
}

void zcl_transport_close(zcl_transport **handle)
{
    if (handle == NULL || *handle == NULL) return;
    zcl_transport *transport = *handle;
    *handle = NULL;
    zcl_net_socket_close(transport);
    mbedtls_ssl_free(&transport->ssl);
    mbedtls_ssl_config_free(&transport->config);
    mbedtls_x509_crt_free(&transport->anchors);
    /* Provider cleanup must normally empty the heap. Emergency cleanup still
     * clears/releases every remaining allocation owned by this transport. */
    if (!zcl_tls_heap_clear(&transport->heap)) transport->fault = ZCL_IO_UNCERTAIN;
    zcl_secure_zero(transport, sizeof(*transport));
    free(transport);
}

static zcl_status open_connection(zcl_transport *transport, const zcl_endpoint *endpoint,
                                   const zcl_trust_anchor *anchors, size_t count)
{
    if (!zcl_tls_heap_enter(&transport->heap)) return ZCL_BUSY;
    zcl_status status = zcl_tls_configure(transport, endpoint, anchors, count);
    if (status == ZCL_OK) status = zcl_net_connect(transport, endpoint);
    if (status == ZCL_OK) status = zcl_tls_handshake(transport);
    zcl_tls_heap_leave();
    if (transport->heap.denied) return ZCL_RESOURCE_EXHAUSTED;
    return status;
}

zcl_status zcl_transport_open(const zcl_endpoint *endpoint, const zcl_trust_anchor *anchors,
                               size_t anchor_count, const zcl_net_limit *limit,
                               zcl_transport **output)
{
    if (output == NULL || *output != NULL || anchors == NULL) return ZCL_INVALID_ARGUMENT;
    if (anchor_count == 0 || anchor_count > ZCL_NET_ANCHORS_MAX) return ZCL_OUT_OF_RANGE;
    zcl_status status = endpoint_valid(endpoint);
    if (status != ZCL_OK) return status;
    status = zcl_net_limit_check(limit);
    if (status != ZCL_OK) return status;
    /* Constant checked object size; invocation owns it until output succeeds.
     * All failure paths converge on zcl_transport_close. */
    zcl_transport *transport = calloc(1, sizeof(*transport));
    if (transport == NULL) return ZCL_RESOURCE_EXHAUSTED;
    transport->socket = -1;
    transport->limit = *limit;
    mbedtls_ssl_init(&transport->ssl);
    mbedtls_ssl_config_init(&transport->config);
    mbedtls_x509_crt_init(&transport->anchors);
    status = open_connection(transport, endpoint, anchors, anchor_count);
    if (status != ZCL_OK) {
        zcl_transport_close(&transport);
        return status;
    }
    transport->verified = true;
    *output = transport;
    return ZCL_OK;
}
