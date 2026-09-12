/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "tls_fixture.h"
#include "transport_internal.h"
#include "zcl_tls_alloc.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Transport check failed at %d\n", __LINE__); abort(); } } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1

static zcl_net_limit limit(zcl_net_cancel **cancel, uint32_t milliseconds)
{
    CHECK(zcl_net_cancel_create(cancel) == ZCL_OK);
    zcl_net_limit budget = {0};
    CHECK(zcl_net_limit_start(*cancel, milliseconds, &budget) == ZCL_OK);
    return budget;
}

static void heap_bounds(void)
{
    zcl_tls_heap heap = {0};
    CHECK(zcl_tls_calloc(1, 16) == NULL); /* No unowned provider allocation. */
    CHECK(zcl_tls_heap_enter(&heap));
    CHECK(!zcl_tls_heap_enter(&heap));
    unsigned char *one = zcl_tls_calloc(8, 8);
    unsigned char *two = zcl_tls_calloc(1, 1000);
    CHECK(one != NULL && two != NULL && heap.count == 2);
    for (size_t i = 0; i < 64; ++i) CHECK(one[i] == 0);
    CHECK((uintptr_t)one % _Alignof(max_align_t) == 0);
    CHECK(zcl_tls_calloc(SIZE_MAX, 2) == NULL);
    CHECK(zcl_tls_calloc(1, 4194304) == NULL && heap.denied);
    zcl_tls_free(one);
    zcl_tls_free(two);
    CHECK(heap.count == 0 && heap.used == 0 && zcl_tls_heap_clear(&heap));
    zcl_tls_heap_leave();
}

static void host_and_cancel(void)
{
    uint8_t canonical[253] = {0};
    CHECK(zcl_net_host(TEXT("EXAMPLE.invalid"), canonical) == ZCL_OK);
    CHECK(memcmp(canonical, "example.invalid", 15) == 0);
    static const char *bad[] = {"", "a.", ".a", "a..b", "-a.b", "a-.b", "https://a.b",
        "a.b:50002", "a_b.invalid", "a\nb", "*.invalid", "127.0.0.1", "::1"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        CHECK(zcl_net_host((const uint8_t *)bad[i], strlen(bad[i]), canonical) != ZCL_OK);
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 1000);
    CHECK(zcl_net_limit_check(&budget) == ZCL_OK);
    zcl_endpoint resolved = {0};
    CHECK(zcl_endpoint_resolve(TEXT("localhost"), 50002, &budget, &resolved) == ZCL_OK);
    CHECK(resolved.address_count > 0 && resolved.address_count <= ZCL_NET_ADDRESSES_MAX);
    zcl_net_cancel_request(cancel);
    CHECK(zcl_net_limit_check(&budget) == ZCL_CANCELLED);
    zcl_endpoint endpoint;
    memset(&endpoint, 0xa5, sizeof(endpoint));
    CHECK(zcl_endpoint_resolve(TEXT("localhost"), 50002, &budget, &endpoint) == ZCL_CANCELLED);
    CHECK(endpoint.host[0] == 0xa5);
    zcl_net_cancel_destroy(&cancel);
    CHECK(cancel == NULL);
}

static void heap_cleanup(void)
{
    zcl_tls_heap first = {0}, second = {0};
    CHECK(zcl_tls_heap_enter(&first));
    void *oldest = zcl_tls_calloc(1, 17);
    void *middle = zcl_tls_calloc(1, 31);
    void *newest = zcl_tls_calloc(1, 127);
    CHECK(oldest != NULL && middle != NULL && newest != NULL);
    memset(oldest, 0xa5, 17); memset(middle, 0xa5, 31); memset(newest, 0xa5, 127);
    zcl_tls_heap_leave();
    CHECK(zcl_tls_heap_enter(&second));
    CHECK(zcl_tls_calloc(1, 63) != NULL);
    zcl_tls_free(middle); /* The block's owner, not the active heap, is used. */
    CHECK(first.count == 2 && second.count == 1);
    zcl_tls_free(newest);
    CHECK(first.count == 1);
    CHECK(!zcl_tls_heap_clear(&first));
    CHECK(first.first == NULL && first.used == 0 && first.count == 0);
    CHECK(zcl_tls_heap_clear(&first));
    CHECK(!zcl_tls_heap_clear(&second));
    CHECK(second.first == NULL && second.used == 0 && second.count == 0);
    for (size_t i = 0; i < 8192; ++i) CHECK(zcl_tls_calloc(1, 1) != NULL);
    CHECK(zcl_tls_calloc(1, 1) == NULL && second.denied);
    CHECK(!zcl_tls_heap_clear(&second));
    CHECK(zcl_tls_heap_clear(&second));
    zcl_tls_heap_leave();
}

static void malformed_trust(tls_fixture_keys *keys)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_ECHO);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 3000);
    zcl_transport *transport = NULL;
    zcl_trust_anchor root = {keys->root, keys->root_length - 1};
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_TLS_FAILURE);
    CHECK(keys->root_length < sizeof(keys->root));
    keys->root[keys->root_length] = 0;
    root.length = keys->root_length + 1;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_INVALID_ENCODING);
    root.length = ZCL_NET_CERT_MAX + 1;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_OUT_OF_RANGE);
    root.length = 0;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transport_open(&endpoint, &root, ZCL_NET_ANCHORS_MAX + 1, &budget, &transport)
        == ZCL_OUT_OF_RANGE);
    CHECK(transport == NULL);
    tls_fixture_stop(&server);
    CHECK(!server.application_received);
    zcl_net_cancel_destroy(&cancel);
}

static void exchange(tls_fixture_keys *keys)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_ECHO);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 5000);
    zcl_transport *transport = NULL;
    zcl_status opened = zcl_transport_open(&endpoint, &root, 1, &budget, &transport);
    if (opened != ZCL_OK) fprintf(stderr, "TLS open status %d\n", opened);
    CHECK(opened == ZCL_OK && transport != NULL);
    CHECK(transport->verified && transport->heap.used > 0 && transport->heap.peak <= 4194304);
    CHECK(zcl_transport_write(transport, TEXT("fixture\n")) == ZCL_OK);
    uint8_t text[13] = {0};
    size_t used = 0;
    while (used < sizeof(text)) {
        uint8_t packet[4] = {0xa5, 0xa5, 0xa5, 0xa5};
        size_t length = 0;
        CHECK(zcl_transport_read(transport, packet + 1, 2, &length) == ZCL_OK);
        CHECK(length > 0 && length <= 2 && length <= sizeof(text) - used);
        CHECK(packet[0] == 0xa5 && packet[length + 1] == 0xa5);
        memcpy(text + used, packet + 1, length);
        used += length;
    }
    CHECK(memcmp(text, "public reply\n", sizeof(text)) == 0);
    zcl_transport_close(&transport);
    CHECK(transport == NULL);
    tls_fixture_stop(&server);
    CHECK(server.application_received);
    zcl_net_cancel_destroy(&cancel);
}

static void rejected(tls_fixture_keys *keys, const tls_fixture_keys *trusted, bool wrong_name)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_ECHO);
    zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    if (wrong_name) endpoint.host[0] = 'x';
    const zcl_trust_anchor root = {trusted->root, trusted->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 3000);
    zcl_transport *transport = NULL;
    const zcl_status status = zcl_transport_open(&endpoint, &root, 1, &budget, &transport);
    CHECK(status == ZCL_TLS_FAILURE && transport == NULL);
    tls_fixture_stop(&server);
    CHECK(!server.application_received);
    zcl_net_cancel_destroy(&cancel);
}

static void corrupt_record(tls_fixture_keys *keys)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_CORRUPT);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 3000);
    zcl_transport *transport = NULL;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_OK);
    uint8_t output[16];
    memset(output, 0xa5, sizeof(output));
    size_t length = 77;
    CHECK(zcl_transport_read(transport, output, sizeof(output), &length) == ZCL_TLS_FAILURE);
    CHECK(length == 77);
    for (size_t i = 0; i < sizeof(output); ++i) CHECK(output[i] == 0xa5);
    CHECK(zcl_transport_write(transport, TEXT("fixture\n")) == ZCL_TLS_FAILURE);
    zcl_transport_close(&transport);
    tls_fixture_stop(&server);
    zcl_net_cancel_destroy(&cancel);
}

static void deadline(tls_fixture_keys *keys)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_SILENT);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 100);
    zcl_transport *transport = NULL;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_TIMED_OUT);
    CHECK(transport == NULL);
    tls_fixture_stop(&server);
    zcl_net_cancel_destroy(&cancel);
}

static void *cancel_thread(void *argument)
{
    const struct timespec delay = {0, 20000000};
    CHECK(nanosleep(&delay, NULL) == 0);
    zcl_net_cancel_request(argument);
    return NULL;
}

static void cancellation(tls_fixture_keys *keys)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_SILENT);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 2000);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, cancel_thread, cancel) == 0);
    zcl_transport *transport = NULL;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_CANCELLED);
    CHECK(transport == NULL);
    CHECK(pthread_join(thread, NULL) == 0);
    tls_fixture_stop(&server);
    zcl_net_cancel_destroy(&cancel);
}

static void io_refusal(tls_fixture_keys *keys, tls_fixture_mode mode, zcl_status expected)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, mode);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    zcl_net_limit budget = limit(&cancel, 3000);
    zcl_transport *transport = NULL;
    CHECK(zcl_transport_open(&endpoint, &root, 1, &budget, &transport) == ZCL_OK);
    static uint8_t output[16384];
    zcl_status status = ZCL_OK;
    size_t length = 77;
    for (size_t i = 0; i < 100 && status == ZCL_OK; ++i) {
        memset(output, 0xa5, sizeof(output));
        length = 77;
        status = zcl_transport_read(transport, output, sizeof(output), &length);
    }
    CHECK(status == expected && length == 77 && transport->received <= 262144);
    for (size_t i = 0; i < sizeof(output); ++i) CHECK(output[i] == 0xa5);
    CHECK(zcl_transport_write(transport, TEXT("fixture\n")) == expected);
    zcl_transport_close(&transport);
    tls_fixture_stop(&server);
    zcl_net_cancel_destroy(&cancel);
}

int main(void)
{
    /* Test-server OpenSSL owns its blocking BIO; prevent a failed client from
     * terminating this fixture process. Production C uses MSG_NOSIGNAL. */
    CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    heap_bounds(); heap_cleanup(); host_and_cancel();
    tls_fixture_keys valid, other, expired, future, wrong_usage;
    tls_fixture_keys_init(&valid, -60, 3600, "serverAuth");
    tls_fixture_keys_init(&other, -60, 3600, "serverAuth");
    tls_fixture_keys_init(&expired, -7200, -3600, "serverAuth");
    tls_fixture_keys_init(&future, 3600, 7200, "serverAuth");
    tls_fixture_keys_init(&wrong_usage, -60, 3600, "clientAuth");
    exchange(&valid);
    malformed_trust(&valid);
    rejected(&valid, &other, false);
    rejected(&valid, &valid, true);
    rejected(&expired, &expired, false);
    rejected(&future, &future, false);
    rejected(&wrong_usage, &wrong_usage, false);
    tls_fixture_empty_names(&wrong_usage);
    rejected(&wrong_usage, &wrong_usage, false);
    corrupt_record(&valid);
    deadline(&valid);
    cancellation(&valid);
    io_refusal(&valid, TLS_FIXTURE_CLOSE, ZCL_IO_FAILURE);
    io_refusal(&valid, TLS_FIXTURE_FLOOD, ZCL_RESOURCE_EXHAUSTED);
    tls_fixture_keys rsa, weak_rsa;
    tls_fixture_rsa_keys_init(&rsa, 2048);
    tls_fixture_rsa_keys_init(&weak_rsa, 1024);
    exchange(&rsa);
    rejected(&weak_rsa, &weak_rsa, false);
    CHECK(SSL_CTX_set_cipher_list(valid.context, "ECDHE-ECDSA-AES128-GCM-SHA256") == 1);
    rejected(&valid, &valid, false); /* No software-AES fallback in the C client. */
    tls_fixture_keys_clear(&rsa); tls_fixture_keys_clear(&weak_rsa);
    tls_fixture_keys_clear(&valid); tls_fixture_keys_clear(&other);
    tls_fixture_keys_clear(&expired); tls_fixture_keys_clear(&future);
    tls_fixture_keys_clear(&wrong_usage);
    puts("C TLS ECDSA/RSA exchange, trust/hostname/time/EKU/key/suite rejection, malformed roots, "
        "record integrity, quotas, cancellation, timeout and heap cleanup passed");
    return 0;
}
