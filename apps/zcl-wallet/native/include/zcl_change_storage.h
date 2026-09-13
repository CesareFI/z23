/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_STORAGE_H
#define ZCL_CHANGE_STORAGE_H
#include "zcl_storage.h"
#include "zcl_change_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_CHANGE_STORAGE_MAX_RECORDS UINT32_C(65536)
#define ZCL_CHANGE_STORAGE_MAX_BYTES UINT32_C(5242880)

/* Public, UNAUTHENTICATED observation, not an index or spending authority.
 * Owns all bytes; no pointers escape. Partial/empty existing files can be
 * observed for explicit recovery, but cannot pass normal append validation. */
typedef struct {
    uint32_t file_bytes;
    size_t tail_len;
    uint8_t tail[80];
} zcl_change_storage_snapshot;

/* Public-data IO on Android/Linux; all zcl_storage path/ownership rules apply.
 * Stable caller-owned spans must not overlap. No secrets enter these APIs.
 * Caller MUST authenticate the exact wallet ciphertext/header/entropy first,
 * and authenticate newly written state records with the recovered-wallet codec.
 * Normal append also requires an authenticated prior head. IO checks
 * state structure/position only, never its MAC, GCM or hardware protection.
 * No index/address is published here and no operation grants sending approval.
 * MACs and this append log cannot defeat malicious filesystem rollback.
 */

/* Fresh wallet only. Requires wallet, pending and change files all absent.
 * Persist authenticated initial index0 BEFORE writing/committing the wallet.
 * Every failed artifact remains; no overwrite/erase/reset. Existing wallets
 * with missing state require a separate migration/discovery decision. */
zcl_status zcl_storage_create_with_change(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len,
    const uint8_t *initial_state, size_t state_len);

/* Match exact committed authenticated wallet bytes under the directory lock.
 * Output remains unchanged on any failure, including descriptor cleanup. */
zcl_status zcl_storage_change_observe(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, zcl_change_storage_snapshot *snapshot);

/* Caller authenticates expected.tail and next_state first. Under one lock,
 * compare size/tail on the SAME descriptor used to append. Require old/new
 * counters equal their file positions. Flush and close before success. Stale
 * snapshots return BUSY; failures/uncertainty NEVER permit index publication.
 * A fully appended record consumes its prior index even if this call failed.
 * Missing/partial/structurally invalid/capped state refuses; no internal retry
 * or fallback. MAC failure is the authenticating caller's required refusal.
 * At most65535 reservations (indexes0..65534), then explicit exhaustion. */
zcl_status zcl_storage_change_append(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len,
    const zcl_change_storage_snapshot *expected, const uint8_t *next_state, size_t state_len);

/* Explicit recovery IO, never an automatic append fallback. Caller MUST verify
 * recovered wallet identity, classify damage and authenticate the replacement.
 * Expected may contain partial/invalid prior bytes. Under the same lock/fd,
 * compare exact snapshot, append only zero padding needed to finish its slot,
 * then the authenticated next-position record. Empty existing file pads80
 * bytes then appends record1, burning index0. Missing remains NOT_FOUND.
 * Preserves ALL previous bytes, never truncates/replaces/rolls over. At most160
 * new bytes, within the existing cap; failures/uncertainty preserve artifacts.
 * Successful repair returns no address or reservation. Healthy or authenticated
 * inconsistent heads must be refused by the authenticating recovery caller. */
zcl_status zcl_storage_change_repair(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len,
    const zcl_change_storage_snapshot *expected, const uint8_t *replacement, size_t state_len);

#ifdef __cplusplus
}
#endif
#endif
