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
/* Retire the copied record, borrowed entropy pointer and metadata after the
 * enclosing operation; callers must invoke this on every exit path. */
void zcl_change_custody_clear(zcl_change_custody *wallet);
/* Generate fresh OS blinding internally, then clear it on every exit. These
 * helpers recheck recovered wallet identity; no MAC key/secret is retained. */
zcl_status zcl_change_custody_encode(const zcl_change_custody *wallet, uint32_t index, uint8_t *state);
zcl_status zcl_change_custody_decode(const zcl_change_custody *wallet,
    const uint8_t *state, size_t state_len, uint32_t *index);
zcl_status zcl_change_custody_address(const zcl_change_custody *wallet,
    uint32_t index, uint8_t *address, size_t capacity);

/* Private adapter variant: caller transfers a writable nonoverlapping32-byte
 * entropy scratch span for this call. A NULL span or capacity other than32
 * refuses without access. An admitted span is fully wiped after private work,
 * including crypto/record refusal, and before any public storage operation.
 * Caller still clears its own original entropy and handles invalid-span refusal.
 * All authentication/fresh-wallet/storage prerequisites of the borrowed API hold. */
zcl_status zcl_wallet_change_create_owned(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, uint8_t *entropy, size_t entropy_len, size_t capacity);
#endif
