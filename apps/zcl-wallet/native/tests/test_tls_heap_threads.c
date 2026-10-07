/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_alloc.h"
#include "zcl_tls_alloc.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { pthread_barrier_t *barrier; unsigned role; } worker;

static void rendezvous(pthread_barrier_t *barrier)
{
    const int result = pthread_barrier_wait(barrier);
    assert(result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void enter_scope(zcl_tls_heap *heap, const worker *owner)
{
    assert(zcl_tls_calloc(1, 16) == NULL);
    if (owner->role == 1) rendezvous(owner->barrier);
    assert(zcl_tls_heap_enter(heap));
    if (owner->role == 0) rendezvous(owner->barrier);
    rendezvous(owner->barrier);
    zcl_tls_heap nested = {0};
    assert(!zcl_tls_heap_enter(&nested));
    assert(nested.first == NULL && nested.count == 0 && nested.used == 0);
}

static void check_bytes(const uint8_t *bytes, size_t count, uint8_t expected)
{
    for (size_t i = 0; i < count; ++i) assert(bytes[i] == expected);
}

static void allocate_round(zcl_tls_heap *heap, const worker *owner, unsigned round)
{
    static const size_t sizes[] = {16, 47, 79};
    uint8_t *blocks[3] = {0};
    const uint8_t value = (uint8_t)(round + owner->role + 1);
    for (size_t i = 0; i < 3; ++i) {
        blocks[i] = zcl_tls_calloc(1, sizes[i]);
        assert(blocks[i] != NULL);
        check_bytes(blocks[i], sizes[i], 0);
        memset(blocks[i], value, sizes[i]);
    }
    assert(heap->count == 3 && heap->used >= 142);
    if (owner->role == 0) assert(zcl_tls_calloc(SIZE_MAX, 2) == NULL);
    rendezvous(owner->barrier);
    assert(heap->denied == (owner->role == 0));
    for (size_t i = 0; i < 3; ++i) check_bytes(blocks[i], sizes[i], value);
    /* Unlink the middle, oldest and newest blocks while the other heap is live. */
    zcl_tls_free(blocks[1]);
    zcl_tls_free(blocks[0]);
    zcl_tls_free(blocks[2]);
    assert(heap->first == NULL && heap->used == 0 && heap->count == 0);
    rendezvous(owner->barrier);
}

static void leave_scope(zcl_tls_heap *heap, const worker *owner)
{
    if (owner->role == 0) zcl_tls_heap_leave();
    rendezvous(owner->barrier);
    void *remaining = zcl_tls_calloc(1, 17);
    if (owner->role == 0) {
        assert(remaining == NULL);
    } else {
        assert(remaining != NULL && heap->count == 1);
        zcl_tls_free(remaining);
    }
    rendezvous(owner->barrier);
    if (owner->role == 1) zcl_tls_heap_leave();
    assert(zcl_tls_calloc(1, 16) == NULL);
    assert(zcl_tls_heap_clear(heap));
}

static void *run_worker(void *argument)
{
    const worker *owner = argument;
    assert(owner != NULL);
    zcl_tls_heap heap = {0}; /* Only this worker accesses its heap and blocks. */
    enter_scope(&heap, owner);
    for (unsigned round = 0; round < 16; ++round) allocate_round(&heap, owner, round);
    leave_scope(&heap, owner);
    return NULL;
}

int main(void)
{
    pthread_barrier_t barrier;
    assert(pthread_barrier_init(&barrier, NULL, 2) == 0);
    worker owners[2] = {{&barrier, 0}, {&barrier, 1}};
    pthread_t threads[2];
    for (size_t i = 0; i < 2; ++i)
        assert(pthread_create(&threads[i], NULL, run_worker, &owners[i]) == 0);
    for (size_t i = 0; i < 2; ++i) assert(pthread_join(threads[i], NULL) == 0);
    assert(pthread_barrier_destroy(&barrier) == 0);
    assert(zcl_tls_calloc(1, 16) == NULL);
    puts("TLS heap scopes: two workers, staggered lifetimes and 32 allocation rounds pass");
    return 0;
}
