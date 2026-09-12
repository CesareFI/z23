/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_tls_alloc.h"
#include "transport_alloc.h"
#include "mbedtls/platform_util.h"
#include <stdint.h>
#include <stdlib.h>

union zcl_tls_allocation {
    max_align_t alignment;
    struct {
        zcl_tls_heap *owner;
        zcl_tls_allocation *before, *after;
        size_t bytes;
    } data;
};
static _Thread_local zcl_tls_heap *active_heap;
#define TLS_HEAP_MAX ((size_t)4194304)
#define TLS_ALLOCATIONS_MAX ((size_t)8192)

bool zcl_tls_heap_enter(zcl_tls_heap *heap)
{
    if (heap == NULL || active_heap != NULL) return false;
    active_heap = heap;
    return true;
}

void zcl_tls_heap_leave(void) { active_heap = NULL; }

static bool allocation_size(zcl_tls_heap *heap, size_t count, size_t size, size_t *bytes)
{
    if (size == 0 || count == 0) return false;
    if (count > (SIZE_MAX - sizeof(zcl_tls_allocation)) / size) return false;
    const size_t total = count * size + sizeof(zcl_tls_allocation);
    if (heap->used > TLS_HEAP_MAX || total > TLS_HEAP_MAX - heap->used) return false;
    if (heap->count >= TLS_ALLOCATIONS_MAX) return false;
    *bytes = total;
    return true;
}

void *zcl_tls_calloc(size_t count, size_t size)
{
    zcl_tls_heap *heap = active_heap;
    if (heap == NULL) return NULL;
    size_t bytes = 0;
    if (!allocation_size(heap, count, size, &bytes)) {
        heap->denied = true;
        return NULL;
    }
    /* Size and return checked; this heap owns the block until zcl_tls_free,
     * or its sole emergency-cleanup path if provider cleanup leaves it live. */
    zcl_tls_allocation *block = calloc(1, bytes);
    if (block == NULL) {
        heap->denied = true;
        return NULL;
    }
    block->data.owner = heap;
    block->data.after = heap->first;
    block->data.bytes = bytes;
    if (heap->first != NULL) heap->first->data.before = block;
    heap->first = block;
    heap->used += bytes;
    ++heap->count;
    if (heap->used > heap->peak) heap->peak = heap->used;
    return block + 1;
}

static void release_allocation(zcl_tls_heap *heap, zcl_tls_allocation *block)
{
    if (heap->first == block) heap->first = block->data.after;
    if (block->data.before != NULL) block->data.before->data.after = block->data.after;
    if (block->data.after != NULL) block->data.after->data.before = block->data.before;
    heap->used -= block->data.bytes;
    --heap->count;
    mbedtls_platform_zeroize(block, block->data.bytes);
    free(block);
}

void zcl_tls_free(void *pointer)
{
    if (pointer == NULL) return;
    /* Provider contract: pointer is a live result of our calloc hook. */
    zcl_tls_allocation *block = (zcl_tls_allocation *)pointer - 1;
    release_allocation(block->data.owner, block);
}

bool zcl_tls_heap_clear(zcl_tls_heap *heap)
{
    const bool empty = heap->first == NULL && heap->used == 0 && heap->count == 0;
    /* Pass the owner explicitly so each iteration visibly unlinks its head
     * before release, without relying on the block's back-reference. */
    while (heap->first != NULL) release_allocation(heap, heap->first);
    return empty;
}
