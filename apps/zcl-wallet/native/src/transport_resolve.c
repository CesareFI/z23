/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_internal.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

static bool letter(uint8_t c) { return c >= 'a' && c <= 'z'; }
static bool digit(uint8_t c) { return c >= '0' && c <= '9'; }

static bool valid_label(const uint8_t *text, size_t length)
{
    if (length == 0 || length > 63) return false;
    if (text[0] == '-' || text[length - 1] == '-') return false;
    for (size_t i = 0; i < length; ++i) {
        if (!letter(text[i]) && !digit(text[i]) && text[i] != '-') return false;
    }
    return true;
}

static bool host_labels(const uint8_t *host, size_t length)
{
    size_t start = 0;
    bool has_letter = false;
    for (size_t i = 0; i < length; ++i) {
        has_letter = has_letter || letter(host[i]);
        if (host[i] == '.') {
            if (!valid_label(host + start, i - start)) return false;
            start = i + 1;
        }
    }
    return has_letter && valid_label(host + start, length - start);
}

zcl_status zcl_net_host(const uint8_t *input, size_t length, uint8_t *canonical)
{
    if (input == NULL || canonical == NULL) return ZCL_INVALID_ARGUMENT;
    if (length == 0 || length > ZCL_NET_HOST_MAX) return ZCL_OUT_OF_RANGE;
    uint8_t host[ZCL_NET_HOST_MAX];
    for (size_t i = 0; i < length; ++i) {
        const uint8_t c = input[i];
        host[i] = c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c;
    }
    if (!host_labels(host, length)) return ZCL_INVALID_ENCODING;
    memcpy(canonical, host, length);
    return ZCL_OK;
}

static bool copy_address(const struct addrinfo *entry, zcl_net_address *output)
{
    if (entry->ai_addr == NULL) return false;
    if (entry->ai_family == AF_INET && entry->ai_addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in address;
        memcpy(&address, entry->ai_addr, sizeof(address));
        memcpy(output->bytes, &address.sin_addr, 4);
        output->length = 4;
        return true;
    }
    if (entry->ai_family == AF_INET6 && entry->ai_addrlen >= sizeof(struct sockaddr_in6)) {
        struct sockaddr_in6 address;
        memcpy(&address, entry->ai_addr, sizeof(address));
        if (address.sin6_scope_id != 0) return false;
        memcpy(output->bytes, &address.sin6_addr, 16);
        output->length = 16;
        return true;
    }
    return false;
}

static void append_distinct(zcl_endpoint *endpoint, const struct addrinfo *entry)
{
    zcl_net_address candidate = {0};
    if (!copy_address(entry, &candidate)) return;
    /* copy_address admits only 4/16 bytes. The caller bounds count below 8;
     * retain first-seen order without spending slots on repeated OS answers. */
    for (size_t i = 0; i < endpoint->address_count; ++i) {
        const zcl_net_address *present = &endpoint->addresses[i];
        if (present->length == candidate.length &&
            memcmp(present->bytes, candidate.bytes, candidate.length) == 0) return;
    }
    endpoint->addresses[endpoint->address_count++] = candidate;
}

static zcl_status resolve_addresses(zcl_endpoint *endpoint)
{
    char hostname[ZCL_NET_HOST_MAX + 1], port[6];
    memcpy(hostname, endpoint->host, endpoint->host_length);
    hostname[endpoint->host_length] = 0;
    const int printed = snprintf(port, sizeof(port), "%u", (unsigned)endpoint->port);
    if (printed <= 0 || (size_t)printed >= sizeof(port)) return ZCL_OUT_OF_RANGE;
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;
    if (getaddrinfo(hostname, port, &hints, &addresses) != 0) return ZCL_IO_FAILURE;
    if (addresses == NULL) return ZCL_NOT_FOUND;
    /* The OS owns allocation size; this call owns the one returned list. */
    const struct addrinfo *entry = addresses;
    for (size_t visited = 0; entry != NULL && visited < 32; ++visited) {
        if (endpoint->address_count == ZCL_NET_ADDRESSES_MAX) break;
        append_distinct(endpoint, entry);
        entry = entry->ai_next;
    }
    freeaddrinfo(addresses);
    return endpoint->address_count != 0 ? ZCL_OK : ZCL_NOT_FOUND;
}

zcl_status zcl_endpoint_resolve(const uint8_t *host, size_t host_length, uint16_t port,
                                const zcl_net_limit *limit, zcl_endpoint *endpoint)
{
    if (endpoint == NULL || port == 0) return ZCL_INVALID_ARGUMENT;
    zcl_endpoint parsed = {0};
    zcl_status status = zcl_net_host(host, host_length, parsed.host);
    if (status != ZCL_OK) return status;
    parsed.host_length = host_length;
    parsed.port = port;
    status = zcl_net_limit_check(limit);
    if (status != ZCL_OK) return status;
    status = resolve_addresses(&parsed);
    const zcl_status timing = zcl_net_limit_check(limit);
    if (timing != ZCL_OK) return timing;
    if (status != ZCL_OK) return status;
    *endpoint = parsed;
    return ZCL_OK;
}
