/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_STORAGE_H
#define ZCL_STORAGE_H
#include "zcl_wallet_record.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_STORAGE_PATH_MAX ((size_t)1024)

/* Android/Linux private storage adapter. Directory is an absolute, trusted
 * app-private path supplied by the platform, never a URI or network input.
 * Its final component is created mode 0700 if absent; parents must exist.
 * C owns every file descriptor for one call and retains no state. No secrets
 * enter this interface, only the bounded ciphertext record. No overwrite/erase.
 * Read outputs remain unchanged on error. A pending result requires platform
 * GCM authentication and zcl_wallet_recovered_address before promotion/use.
 */
zcl_status zcl_storage_read(const uint8_t *directory, size_t directory_len,
                           uint8_t *record, size_t capacity, size_t *record_len, bool *pending);
zcl_status zcl_storage_create(const uint8_t *directory, size_t directory_len,
                             const uint8_t *record, size_t record_len);
zcl_status zcl_storage_promote(const uint8_t *directory, size_t directory_len,
                              const uint8_t *authenticated_record, size_t record_len);

#ifdef __cplusplus
}
#endif
#endif
