/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TLS_ALLOC_H
#define ZCL_TLS_ALLOC_H
#include <stddef.h>
/* Private provider hooks, declared here because the provider configuration
 * includes this header. Only a scoped transport heap may allocate. */
void *zcl_tls_calloc(size_t count, size_t size);
void zcl_tls_free(void *pointer);
#endif
