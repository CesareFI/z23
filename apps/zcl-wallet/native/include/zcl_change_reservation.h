/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_RESERVATION_H
#define ZCL_CHANGE_RESERVATION_H
#include "zcl_change_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t index;
    zcl_network network;
    uint8_t address[35]; /* Canonical public ASCII, no terminator. */
} zcl_change_reservation;

/* Platform MUST first authenticate this exact ciphertext/header/entropy with
 * GCM and its per-use hardware policy. C cannot prove that platform action.
 * C copies the bounded ciphertext record and checks recovered wallet identity.
 * Entropy and trusted directory remain stable caller-owned spans; clear
 * entropy after the synchronous call. No pointer, secret or handle is retained.
 * Inputs/output must not overlap. OS blinding is generated/cleared internally.
 * No transaction signing, change-output classification, funding verification,
 * malicious-filesystem-rollback protection or consent receipt is supplied.
 */

/* Fresh paired wallet/state creation only. No caller-selected initial counter.
 * Wrong entropy, RNG or codec failure cannot create wallet/state files.
 * Existing/orphan/missing-state migration is never silently repaired/reset. */
zcl_status zcl_wallet_change_create(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len);

/* Authenticate head, require its index equals its bounded file position,
 * privately derive internal-chain address, then authenticate/append successor.
 * Publish index/network/address only after every IO durability/cleanup check.
 * Output remains unchanged on failure, including a fully appended but
 * uncertain result. Such indexes are consumed; never retry by reusing them.
 * Missing/partial/invalid/exhausted state refuses, without automatic repair.
 * No internal retry. Cancellation after successful reservation burns its index.
 * This synchronous bounded operation belongs on a platform worker thread. */
zcl_status zcl_wallet_change_reserve(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len,
    zcl_change_reservation *reservation);

#ifdef __cplusplus
}
#endif
#endif
