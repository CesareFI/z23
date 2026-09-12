/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSPORT_ALLOC_H
#define ZCL_TRANSPORT_ALLOC_H
#include <stddef.h>
#include <stdbool.h>
typedef union zcl_tls_allocation zcl_tls_allocation;
typedef struct {
    zcl_tls_allocation *first;
    size_t used, count, peak;
    bool denied;
} zcl_tls_heap;
/* Zero-initialize a new heap. One worker exclusively owns it. enter/leave
 * surround provider operations; no allocation can occur outside such a scope.
 * Destroy only after provider cleanup; returns false if residual allocations
 * required emergency cleanup. No pointer/heap may outlive its transport. */
bool zcl_tls_heap_enter(zcl_tls_heap *heap);
void zcl_tls_heap_leave(void);
bool zcl_tls_heap_clear(zcl_tls_heap *heap);
#endif
