/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_STORAGE_INTERNAL_H
#define ZCL_STORAGE_INTERNAL_H
#include "zcl_storage.h"

typedef enum { ZCL_STORE_COMMITTED, ZCL_STORE_PENDING, ZCL_STORE_CHANGE } zcl_store_slot;
typedef struct { int directory; int lock; } zcl_store;

/* Initialize {-1,-1}; always call close exactly once after open, even on error.
 * No fd may be copied, retained or passed outside this invocation. */
zcl_status zcl_store_open(const uint8_t *path, size_t path_len, zcl_store *store);
zcl_status zcl_store_close(zcl_store *store, zcl_status status);
const char *zcl_store_name(zcl_store_slot slot);
zcl_status zcl_store_read_file(const zcl_store *store, zcl_store_slot slot,
                              uint8_t *record, size_t capacity, size_t *length, bool durable);
zcl_status zcl_store_sync(int fd);
/* Internal bounded complete write; caller owns fd and a stable valid span. */
zcl_status zcl_store_write_bytes(int fd, const uint8_t *record, size_t length);
zcl_status zcl_store_absent(const zcl_store *store, zcl_store_slot slot);
zcl_status zcl_store_write_pending(const zcl_store *store, const uint8_t *record, size_t length);
zcl_status zcl_store_commit(const zcl_store *store);
#endif
