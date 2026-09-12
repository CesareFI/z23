/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_internal.h"
#include "zcl_keys.h"
#include <string.h>

static zcl_status ready(zcl_transport *transport)
{
    if (transport->fault != ZCL_OK) return transport->fault;
    if (!transport->verified) return ZCL_TLS_FAILURE;
    const zcl_status status = zcl_net_limit_check(&transport->limit);
    return status == ZCL_OK ? ZCL_OK : zcl_net_fail(transport, status);
}

static zcl_status read_bytes(zcl_transport *transport, size_t capacity, size_t *length)
{
    for (;;) {
        const zcl_status allowed = ready(transport);
        if (allowed != ZCL_OK) return allowed;
        const int count = mbedtls_ssl_read(&transport->ssl, transport->plaintext, capacity);
        if (count > 0) {
            if ((size_t)count > capacity) return ZCL_TLS_FAILURE;
            *length = (size_t)count;
            return ready(transport);
        }
        const zcl_status status = zcl_tls_retry(transport, count);
        if (status != ZCL_OK) return status;
    }
}

zcl_status zcl_transport_read(zcl_transport *transport, uint8_t *output, size_t capacity,
                               size_t *length)
{
    if (transport == NULL || output == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity == 0 || capacity > sizeof(transport->plaintext)) return ZCL_OUT_OF_RANGE;
    zcl_status status = ready(transport);
    if (status != ZCL_OK) return status;
    if (!zcl_tls_heap_enter(&transport->heap)) return ZCL_BUSY;
    size_t count = 0;
    status = read_bytes(transport, capacity, &count);
    zcl_tls_heap_leave();
    if (transport->heap.denied) status = ZCL_RESOURCE_EXHAUSTED;
    if (status == ZCL_OK) {
        memcpy(output, transport->plaintext, count);
        *length = count;
    } else {
        (void)zcl_net_fail(transport, status);
    }
    zcl_secure_zero(transport->plaintext, sizeof(transport->plaintext));
    return status;
}

static zcl_status write_bytes(zcl_transport *transport, const uint8_t *input, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        const zcl_status allowed = ready(transport);
        if (allowed != ZCL_OK) return allowed;
        const int count = mbedtls_ssl_write(&transport->ssl, input + offset, length - offset);
        if (count > 0) {
            if ((size_t)count > length - offset) return ZCL_TLS_FAILURE;
            offset += (size_t)count;
        } else {
            const zcl_status status = zcl_tls_retry(transport, count);
            if (status != ZCL_OK) return status;
        }
    }
    return ready(transport);
}

zcl_status zcl_transport_write(zcl_transport *transport, const uint8_t *input, size_t length)
{
    if (transport == NULL || input == NULL) return ZCL_INVALID_ARGUMENT;
    if (length == 0 || length > 256) return ZCL_OUT_OF_RANGE;
    zcl_status status = ready(transport);
    if (status != ZCL_OK) return status;
    if (!zcl_tls_heap_enter(&transport->heap)) return ZCL_BUSY;
    status = write_bytes(transport, input, length);
    zcl_tls_heap_leave();
    if (transport->heap.denied) status = ZCL_RESOURCE_EXHAUSTED;
    return status == ZCL_OK ? ZCL_OK : zcl_net_fail(transport, status);
}
