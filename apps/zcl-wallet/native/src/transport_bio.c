/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_internal.h"
#include <errno.h>
#include <sys/socket.h>

static int io_error(zcl_transport *transport, int retry, int failure)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return retry;
    (void)zcl_net_fail(transport, ZCL_IO_FAILURE);
    return failure;
}

static zcl_status io_limit(zcl_transport *transport, size_t length, size_t used, size_t maximum)
{
    if (length == 0 || length > 32768) return ZCL_OUT_OF_RANGE;
    if (used >= maximum) return ZCL_RESOURCE_EXHAUSTED;
    return zcl_net_limit_check(&transport->limit);
}

int zcl_net_send(void *context, const unsigned char *data, size_t length)
{
    zcl_transport *transport = context;
    const size_t maximum = 32768;
    const zcl_status allowed = io_limit(transport, length, transport->sent, maximum);
    if (allowed != ZCL_OK) {
        (void)zcl_net_fail(transport, allowed);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    const size_t remaining = maximum - transport->sent;
    const size_t count = length < remaining ? length : remaining;
#if defined(MSG_NOSIGNAL)
    const ssize_t sent = send(transport->socket, data, count, MSG_NOSIGNAL);
#else
    (void)data;
    (void)count;
    (void)zcl_net_fail(transport, ZCL_UNSUPPORTED);
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
#endif
#if defined(MSG_NOSIGNAL)
    if (sent < 0) return io_error(transport, MBEDTLS_ERR_SSL_WANT_WRITE, MBEDTLS_ERR_SSL_INTERNAL_ERROR);
    if (sent == 0 || (size_t)sent > count) {
        (void)zcl_net_fail(transport, ZCL_IO_FAILURE);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    transport->sent += (size_t)sent;
    return (int)sent; /* count <=32768, proven above. */
#endif
}

int zcl_net_receive(void *context, unsigned char *data, size_t length)
{
    zcl_transport *transport = context;
    const size_t maximum = 262144;
    const zcl_status allowed = io_limit(transport, length, transport->received, maximum);
    if (allowed != ZCL_OK) {
        (void)zcl_net_fail(transport, allowed);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    const size_t remaining = maximum - transport->received;
    const size_t count = length < remaining ? length : remaining;
    const ssize_t received = recv(transport->socket, data, count, 0);
    if (received < 0) return io_error(transport, MBEDTLS_ERR_SSL_WANT_READ, MBEDTLS_ERR_SSL_INTERNAL_ERROR);
    if (received == 0 || (size_t)received > count) {
        (void)zcl_net_fail(transport, ZCL_IO_FAILURE);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    transport->received += (size_t)received;
    return (int)received;
}
