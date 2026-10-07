/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_alloc.h"
#include <stdio.h>
#include <stdlib.h>

/* Linux host tests only: observe the actual production cleanup boundary.
 * Production retains emergency cleanup; fuzzing must not mistake it for
 * successful normal provider cleanup. No ownership is transferred here. */
bool __real_zcl_tls_heap_clear(zcl_tls_heap *heap);
bool __wrap_zcl_tls_heap_clear(zcl_tls_heap *heap);
bool __wrap_zcl_tls_heap_clear(zcl_tls_heap *heap)
{
    if (heap == NULL || heap->first != NULL || heap->used != 0 || heap->count != 0) {
        fputs("TLS provider cleanup left owned allocations\n", stderr);
        abort();
    }
    return __real_zcl_tls_heap_clear(heap);
}
