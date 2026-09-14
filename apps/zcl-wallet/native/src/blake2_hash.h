/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLAKE2_HASH_H
#define ZCL_BLAKE2_HASH_H
#include "zcl_wallet.h"

#define ZCL_BLAKE2_INPUT_MAX ((size_t)4096)
#define ZCL_BLAKE2_PERSONAL_BYTES ((size_t)16)
#define ZCL_BLAKE2_DIGEST_BYTES ((size_t)32)

/* Internal public-data-only, unkeyed sequential BLAKE2b-256 with zero salt.
 * Personalization is exactly 16 bytes, including any explicit zero padding.
 * Caller owns stable, nonoverlapping spans for the call. NULL is rejected even
 * for empty input; at most 4096 input bytes are read. Output needs >=32 bytes;
 * only its first 32 bytes change on success, and none change on failure.
 * No allocation, retained state, I/O, secret-key hashing or signing authority.
 * Wrapper scratch is cleared; provider compression temporaries are public.
 */
zcl_status zcl_blake2b256(const uint8_t *input, size_t input_len,
                         const uint8_t *personal, size_t personal_len,
                         uint8_t *output, size_t output_capacity);
#endif
