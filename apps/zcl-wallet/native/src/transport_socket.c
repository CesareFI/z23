/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void close_descriptor(zcl_transport *transport, int descriptor)
{
    transport->socket = -1;
    /* Never retry close: on Linux EINTR still consumes this descriptor. */
    if (descriptor >= 0 && close(descriptor) != 0 && transport->fault == ZCL_OK)
        transport->fault = ZCL_IO_UNCERTAIN;
}

void zcl_net_socket_close(zcl_transport *transport)
{
    close_descriptor(transport, transport->socket);
}

static int new_socket(int family)
{
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    const int descriptor = socket(family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
#else
    const int descriptor = -1;
    (void)family; /* Require atomic nonblocking/CLOEXEC creation on this port. */
#endif
    return descriptor;
}

static zcl_status wait_interval(const zcl_net_limit *limit, int *milliseconds)
{
    const zcl_status status = zcl_net_limit_check(limit);
    if (status != ZCL_OK) return status;
    uint64_t now = 0;
    const zcl_status clock = zcl_net_now(&now);
    if (clock != ZCL_OK) return clock;
    if (now >= limit->deadline_ms) return ZCL_TIMED_OUT;
    const uint64_t left = limit->deadline_ms - now;
    *milliseconds = left > 100 ? 100 : (int)left;
    return ZCL_OK;
}

zcl_status zcl_net_wait(zcl_transport *transport, short events)
{
    for (;;) {
        int milliseconds = 0;
        zcl_status status = wait_interval(&transport->limit, &milliseconds);
        if (status != ZCL_OK) return status;
        struct pollfd descriptor = {transport->socket, events, 0};
        const int ready = poll(&descriptor, 1, milliseconds);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return ZCL_IO_FAILURE;
        if ((descriptor.revents & events) != 0) return ZCL_OK;
        if (descriptor.revents != 0) return ZCL_IO_FAILURE;
    }
}

static int connect_address(int socket_fd, const zcl_net_address *address, uint16_t port)
{
    if (address->length == 4) {
        struct sockaddr_in target = {0};
        target.sin_family = AF_INET;
        target.sin_port = htons(port);
        memcpy(&target.sin_addr, address->bytes, 4);
        return connect(socket_fd, (const struct sockaddr *)&target, sizeof(target));
    }
    struct sockaddr_in6 target = {0};
    target.sin6_family = AF_INET6;
    target.sin6_port = htons(port);
    memcpy(&target.sin6_addr, address->bytes, 16);
    return connect(socket_fd, (const struct sockaddr *)&target, sizeof(target));
}

static zcl_status finish_connect(zcl_transport *transport)
{
    const zcl_status status = zcl_net_wait(transport, POLLOUT);
    if (status != ZCL_OK) return status;
    int error = 0;
    socklen_t length = sizeof(error);
    if (getsockopt(transport->socket, SOL_SOCKET, SO_ERROR, &error, &length) != 0)
        return ZCL_IO_FAILURE;
    return length == sizeof(error) && error == 0 ? ZCL_OK : ZCL_IO_FAILURE;
}

static zcl_status connect_wait(zcl_transport *transport)
{
    uint64_t now = 0;
    zcl_status status = zcl_net_now(&now);
    if (status != ZCL_OK) return status;
    const uint64_t deadline = transport->limit.deadline_ms;
    if (now >= deadline) return ZCL_TIMED_OUT;
    if (deadline - now > 2000) transport->limit.deadline_ms = now + 2000;
    status = finish_connect(transport);
    transport->limit.deadline_ms = deadline;
    if (status == ZCL_TIMED_OUT && zcl_net_limit_check(&transport->limit) == ZCL_OK)
        return ZCL_IO_FAILURE; /* Try the next address within the original budget. */
    return status;
}

static zcl_status try_address(zcl_transport *transport, const zcl_net_address *address, uint16_t port)
{
    const int descriptor = new_socket(address->length == 4 ? AF_INET : AF_INET6);
    if (descriptor < 0) return ZCL_IO_FAILURE;
    transport->socket = descriptor;
    const int connected = connect_address(descriptor, address, port);
    if (connected == 0) return ZCL_OK;
    zcl_status status = ZCL_IO_FAILURE;
    if (errno == EINPROGRESS || errno == EINTR) status = connect_wait(transport);
    if (status != ZCL_OK) close_descriptor(transport, descriptor);
    return status;
}

zcl_status zcl_net_connect(zcl_transport *transport, const zcl_endpoint *endpoint)
{
    for (size_t i = 0; i < endpoint->address_count; ++i) {
        const zcl_status allowed = zcl_net_limit_check(&transport->limit);
        if (allowed != ZCL_OK) return allowed;
        const zcl_status status = try_address(transport, &endpoint->addresses[i], endpoint->port);
        if (status == ZCL_OK) return ZCL_OK;
        if (transport->fault != ZCL_OK) return transport->fault;
        if (status != ZCL_IO_FAILURE) return status;
    }
    return ZCL_IO_FAILURE;
}
