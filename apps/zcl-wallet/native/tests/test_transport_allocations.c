/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "tls_fixture.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Transport allocation check failed at %d\n", __LINE__); abort(); } } while (0)
typedef struct { void *pointer; size_t bytes; } allocation;
static _Thread_local allocation live[8193];
static _Thread_local bool tracking, denied;
static _Thread_local size_t calls, fail_at, live_count;
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);
void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t count, size_t size);
void __wrap_free(void *pointer);

static bool refuse(void)
{
    if (!tracking) return false;
    ++calls;
    if (calls != fail_at) return false;
    denied = true;
    return true;
}

static void remember(void *pointer, size_t bytes)
{
    if (!tracking || pointer == NULL) return;
    for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); ++i) {
        if (live[i].pointer != NULL) continue;
        live[i] = (allocation){pointer, bytes};
        ++live_count;
        return;
    }
    abort();
}

void *__wrap_malloc(size_t size)
{
    if (refuse()) return NULL;
    void *pointer = __real_malloc(size);
    remember(pointer, size);
    return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
    if (refuse()) return NULL;
    CHECK(size == 0 || count <= SIZE_MAX / size);
    void *pointer = __real_calloc(count, size);
    remember(pointer, count * size);
    return pointer;
}

void __wrap_free(void *pointer)
{
    if (tracking && pointer != NULL) {
        bool found = false;
        for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); ++i) {
            if (live[i].pointer != pointer) continue;
            const uint8_t *bytes = pointer;
            for (size_t j = 0; j < live[i].bytes; ++j) CHECK(bytes[j] == 0);
            live[i] = (allocation){0};
            --live_count;
            found = true;
            break;
        }
        CHECK(found); /* Includes double-free and unowned cleanup attempts. */
    }
    __real_free(pointer);
}

static size_t run(tls_fixture_keys *keys, size_t failure)
{
    tls_fixture_server server;
    tls_fixture_start(&server, keys, TLS_FIXTURE_CLOSE);
    const zcl_endpoint endpoint = tls_fixture_endpoint(&server);
    const zcl_trust_anchor root = {keys->root, keys->root_length};
    zcl_net_cancel *cancel = NULL;
    CHECK(zcl_net_cancel_create(&cancel) == ZCL_OK);
    zcl_net_limit budget = {0};
    CHECK(zcl_net_limit_start(cancel, 3000, &budget) == ZCL_OK);
    CHECK(live_count == 0);
    calls = 0;
    fail_at = failure;
    denied = false;
    tracking = true;
    zcl_transport *transport = NULL;
    const zcl_status status = zcl_transport_open(&endpoint, &root, 1, &budget, &transport);
    if (failure != 0) {
        CHECK(denied && status == ZCL_RESOURCE_EXHAUSTED && transport == NULL);
    } else {
        CHECK(status == ZCL_OK && transport != NULL);
    }
    zcl_transport_close(&transport);
    CHECK(live_count == 0);
    const size_t observed = calls;
    tracking = false;
    tls_fixture_stop(&server);
    zcl_net_cancel_destroy(&cancel);
    return observed;
}

static void cancel_failure(void)
{
    calls = 0; fail_at = 1; denied = false; tracking = true;
    zcl_net_cancel *cancel = NULL;
    CHECK(zcl_net_cancel_create(&cancel) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(cancel == NULL && denied && live_count == 0);
    tracking = false;
}

int main(void)
{
    CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    cancel_failure();
    tls_fixture_keys keys;
    tls_fixture_keys_init(&keys, -60, 3600, "serverAuth");
    const size_t baseline = run(&keys, 0);
    CHECK(baseline >= 128);
    /* Every initial configuration allocation and representative later crypto
     * allocations. This does not claim every internal provider control path. */
    size_t tested = 0;
    for (size_t i = 1; i <= 80; ++i) { (void)run(&keys, i); ++tested; }
    for (size_t i = 128; i <= baseline / 2; i *= 2) { (void)run(&keys, i); ++tested; }
    tls_fixture_keys_clear(&keys);
    printf("TLS allocation faults: %zu exercised; baseline %zu calls; every observed owned block cleared and freed\n",
        tested, baseline);
    return 0;
}
