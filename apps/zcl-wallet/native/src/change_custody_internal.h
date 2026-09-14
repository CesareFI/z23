/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_CUSTODY_INTERNAL_H
#define ZCL_CHANGE_CUSTODY_INTERNAL_H
#include "zcl_change_reservation.h"

/* Invocation-private ciphertext copy and stable borrowed entropy. Initialize
 * to zero; use only after successful preparation; retain nothing after return.
 * This is not a key-export or persistent unlocked-wallet handle. */
typedef struct {
    uint8_t record[140];
    size_t record_len;
    const uint8_t *entropy;
    size_t entropy_len;
    zcl_network network;
} zcl_change_custody;

zcl_status zcl_change_custody_prepare(const uint8_t *record, size_t record_len,
    const uint8_t *entropy, size_t entropy_len, zcl_change_custody *wallet);
/* Generate fresh OS blinding internally, then clear it on every exit. These
 * helpers recheck recovered wallet identity; no MAC key/secret is retained. */
zcl_status zcl_change_custody_encode(const zcl_change_custody *wallet, uint32_t index, uint8_t *state);
zcl_status zcl_change_custody_decode(const zcl_change_custody *wallet,
    const uint8_t *state, size_t state_len, uint32_t *index);
zcl_status zcl_change_custody_address(const zcl_change_custody *wallet,
    uint32_t index, uint8_t *address, size_t capacity);
#endif
