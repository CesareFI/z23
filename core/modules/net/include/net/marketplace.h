/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_NET_MARKETPLACE_H
#define ZCL_NET_MARKETPLACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Optional application traffic, independent of blockchain/file-service
 * transport. Only the operator's -marketplace=1 opts in. Policy ports are
 * installed by composition; absent ports refuse marketplace traffic. */
bool marketplace_enabled(void);
bool marketplace_message(const char *command);
bool marketplace_path(const char *path);
bool marketplace_path_disabled(const char *path);
bool marketplace_app_disabled(const char *id, size_t size);
bool marketplace_rpc(const char *method);
/* Serialize optional listing writes with durable refusal and cache removal.
 * Recursive because admitted cache ingestion may call its model save port. */
void marketplace_lock(void);
void marketplace_unlock(void);
/* Nonzero admission generation for pending marketplace transport segments.
 * A successful durable refusal invalidates old optional sends. */
uint64_t marketplace_generation(void);
void marketplace_invalidate_pending(void);
bool marketplace_generation_current(uint64_t generation);
typedef bool (*marketplace_root_filter_fn)(const uint8_t root[32]);
typedef bool (*marketplace_wire_filter_fn)(const char *command,
                                          const uint8_t *wire, size_t size);
void marketplace_set_filters(marketplace_root_filter_fn root_filter,
                             marketplace_wire_filter_fn wire_filter);
bool marketplace_root_allowed(const uint8_t root[32]);
bool marketplace_wire_allowed(const char *command, const uint8_t *wire,
                              size_t size);

#endif
