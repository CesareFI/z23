/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_STATE_INTERNAL_H
#define ZCL_CHANGE_STATE_INTERNAL_H
#include "zcl_change_state.h"
#include "secret_hash.h"

/* Structural inspection ONLY; never authenticates or authorizes an index. */
zcl_status zcl_change_state_inspect(const uint8_t *record, size_t length, uint32_t *next_index);

/* Private64-byte HKDF output. Caller MUST clear it on success and failure.
 * This is not a public key-export API or a persisted authentication handle. */
zcl_status zcl_change_state_key(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint8_t *key, size_t capacity);
#endif
