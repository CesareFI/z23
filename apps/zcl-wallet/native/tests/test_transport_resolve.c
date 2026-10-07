/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "zcl_transport.h"
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Synthetic OS results only: no socket, DNS query or external service. */
static struct addrinfo answers[40];
static union { struct sockaddr_in v4; struct sockaddr_in6 v6; } addresses[40];
static size_t answer_count, lookups, releases;
static int lookup_error;
static uint64_t now_ms, advance_ms;
static bool cancel_before, cancel_during;
static zcl_net_cancel *active_cancel;

int __wrap_clock_gettime(clockid_t clock, struct timespec *value)
{
    assert(clock == CLOCK_MONOTONIC);
    value->tv_sec = (time_t)(now_ms / 1000);
    value->tv_nsec = (long)(now_ms % 1000) * 1000000L;
    return 0;
}

int __wrap_getaddrinfo(const char *host, const char *service,
    const struct addrinfo *hints, struct addrinfo **result)
{
    assert(strcmp(host, "example.invalid") == 0 && strcmp(service, "50002") == 0);
    assert(hints->ai_socktype == SOCK_STREAM && hints->ai_protocol == IPPROTO_TCP);
    assert(hints->ai_flags == AI_NUMERICSERV);
    ++lookups; now_ms += advance_ms;
    if (cancel_during) zcl_net_cancel_request(active_cancel);
    if (lookup_error != 0) return lookup_error;
    *result = answer_count == 0 ? NULL : answers;
    return 0;
}

void __wrap_freeaddrinfo(struct addrinfo *list)
{
    assert(list == answers && releases == 0);
    ++releases;
}

static void ipv4(size_t i, uint8_t suffix)
{
    assert(i < 40);
    struct sockaddr_in *address = &addresses[i].v4;
    const uint8_t bytes[4] = {192, 0, 2, suffix}; /* Documentation-only range. */
    address->sin_family = AF_INET;
    memcpy(&address->sin_addr, bytes, sizeof(bytes));
    answers[i].ai_family = AF_INET;
    answers[i].ai_addr = (struct sockaddr *)address;
    answers[i].ai_addrlen = sizeof(*address);
}

static void ipv6(size_t i, uint8_t suffix)
{
    assert(i < 40);
    struct sockaddr_in6 *address = &addresses[i].v6;
    const uint8_t bytes[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, suffix};
    address->sin6_family = AF_INET6;
    memcpy(&address->sin6_addr, bytes, sizeof(bytes));
    answers[i].ai_family = AF_INET6;
    answers[i].ai_addr = (struct sockaddr *)address;
    answers[i].ai_addrlen = sizeof(*address);
}

static void reset(size_t count)
{
    assert(count <= 40 && active_cancel == NULL);
    memset(answers, 0, sizeof(answers)); memset(addresses, 0, sizeof(addresses));
    answer_count = count; lookups = releases = 0; lookup_error = 0;
    now_ms = 1000; advance_ms = 0; cancel_before = cancel_during = false;
    for (size_t i = 0; i < count; ++i) {
        ipv4(i, 1);
        answers[i].ai_next = i + 1 < count ? &answers[i + 1] : NULL;
    }
}

static zcl_endpoint resolve(zcl_status expected, size_t expected_lookups, size_t expected_releases)
{
    zcl_endpoint result, sentinel;
    memset(&result, 0xa5, sizeof(result)); memcpy(&sentinel, &result, sizeof(result));
    assert(zcl_net_cancel_create(&active_cancel) == ZCL_OK);
    zcl_net_limit limit = {0};
    assert(zcl_net_limit_start(active_cancel, 100, &limit) == ZCL_OK);
    if (cancel_before) zcl_net_cancel_request(active_cancel);
    assert(zcl_endpoint_resolve((const uint8_t *)"EXAMPLE.invalid", 15, 50002,
        &limit, &result) == expected);
    assert(lookups == expected_lookups && releases == expected_releases);
    zcl_net_cancel_destroy(&active_cancel);
    assert(active_cancel == NULL);
    if (expected != ZCL_OK) assert(memcmp(&result, &sentinel, sizeof(result)) == 0);
    else assert(result.host_length == 15 && memcmp(result.host, "example.invalid", 15) == 0);
    return result;
}

static void repeated_answers(void)
{
    reset(9); ipv4(8, 2);
    zcl_endpoint result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 2);
    assert(result.addresses[0].length == 4 && result.addresses[0].bytes[3] == 1);
    assert(result.addresses[1].length == 4 && result.addresses[1].bytes[3] == 2);
    reset(9);
    for (size_t i = 0; i < 8; ++i) ipv6(i, 1);
    ipv6(8, 2); result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 2);
    assert(result.addresses[0].length == 16 && result.addresses[0].bytes[15] == 1);
    assert(result.addresses[1].length == 16 && result.addresses[1].bytes[15] == 2);
}

static void order_and_families(void)
{
    reset(7);
    static const uint8_t suffixes[] = {1, 2, 1, 3, 2, 3, 4};
    for (size_t i = 0; i < 7; ++i) ipv4(i, suffixes[i]);
    zcl_endpoint result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 4);
    for (size_t i = 0; i < 4; ++i) assert(result.addresses[i].bytes[3] == i + 1);
    reset(3); ipv6(1, 1);
    /* Equal first four bytes do not make differently sized families equal. */
    memcpy(&addresses[1].v6.sin6_addr, &addresses[0].v4.sin_addr, 4);
    result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 2);
    assert(result.addresses[0].length == 4 && result.addresses[1].length == 16);
}

static void bounded_results(void)
{
    reset(40);
    for (size_t i = 0; i < 40; ++i) ipv4(i, (uint8_t)(i + 1));
    zcl_endpoint result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 8);
    for (size_t i = 0; i < 8; ++i) assert(result.addresses[i].bytes[3] == i + 1);
    reset(33); ipv4(31, 2); ipv4(32, 3);
    result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 2 && result.addresses[1].bytes[3] == 2);
    reset(33); ipv4(32, 2);
    result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 1); /* Never scan beyond 32 OS entries. */
}

static void refusals(void)
{
    reset(0); (void)resolve(ZCL_NOT_FOUND, 1, 0);
    reset(1); lookup_error = EAI_AGAIN; (void)resolve(ZCL_IO_FAILURE, 1, 0);
    reset(1); cancel_before = true; (void)resolve(ZCL_CANCELLED, 0, 0);
    reset(1); cancel_during = true; (void)resolve(ZCL_CANCELLED, 1, 1);
    reset(1); advance_ms = 100; (void)resolve(ZCL_TIMED_OUT, 1, 1);
    reset(6);
    answers[0].ai_addr = NULL;
    answers[1].ai_addrlen = sizeof(struct sockaddr_in) - 1;
    answers[2].ai_family = AF_UNSPEC;
    ipv6(3, 1); addresses[3].v6.sin6_scope_id = 1;
    ipv6(4, 1); answers[4].ai_addrlen = sizeof(struct sockaddr_in6) - 1;
    ipv4(5, 2);
    const zcl_endpoint result = resolve(ZCL_OK, 1, 1);
    assert(result.address_count == 1 && result.addresses[0].bytes[3] == 2);
    answers[4].ai_next = NULL; answer_count = 5; lookups = releases = 0;
    (void)resolve(ZCL_NOT_FOUND, 1, 1);
}

int main(void)
{
    repeated_answers(); order_and_families(); bounded_results(); refusals();
    puts("resolver distinct-address, ordering, bounds, refusal and cleanup cases passed");
    return 0;
}
