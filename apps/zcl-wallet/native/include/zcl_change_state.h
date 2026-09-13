/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_STATE_H
#define ZCL_CHANGE_STATE_H
#include "zcl_wallet_record.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_CHANGE_STATE_BYTES ((size_t)80)
#define ZCL_CHANGE_INDEX_EXHAUSTED UINT32_C(0x80000000)

/* Content-authenticated internal-chain counter codec, NOT index reservation.
 * Platform MUST authenticate this exact wallet header/entropy with GCM first.
 * Each call verifies the recovered wallet profile and derives a private MAC
 * key internally; it is cleared and never returned. Supply32 independent
 * OS-random blinding bytes for wallet verification; fixed bytes are test-only.
 * Secret spans remain caller-owned and must clear after use. No pointer or
 * secret is retained; stable input/output spans must not overlap.
 * Record and decoded index remain unchanged on failure. Exhausted is a valid
 * stored sentinel but can never be used for address derivation.
 * Authentication proves content, not freshness, non-reuse, chain state or
 * transaction approval. Storage must enforce monotonic durable reservation
 * and recovery; missing/corrupt state never authorizes reset to zero. */
zcl_status zcl_change_state_encode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint32_t next_index, uint8_t *record, size_t capacity);
zcl_status zcl_change_state_decode(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    const uint8_t *record, size_t record_len, uint32_t *next_index);

#ifdef __cplusplus
}
#endif
#endif
