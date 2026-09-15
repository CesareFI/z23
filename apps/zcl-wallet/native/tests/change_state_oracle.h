/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_CHANGE_STATE_ORACLE_H
#define ZCL_TEST_CHANGE_STATE_ORACLE_H
#include <stddef.h>
#include <stdint.h>
/* Host-only independent HKDF/HMAC and record-byte reference. The exact80-byte
 * header is public context, not independently authenticated wallet ownership.
 * Returns 1 only after writing80 bytes; invalid spans/provider errors leave
 * output unchanged. No wallet implementation is called or linked here. */
int change_state_oracle_record(const uint8_t *entropy, size_t entropy_len,
    const uint8_t *header, size_t header_len, uint32_t next, uint8_t *record, size_t capacity);
#endif
