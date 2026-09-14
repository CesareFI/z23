/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_REVIEW_INTERNAL_H
#define ZCL_TRANSACTION_REVIEW_INTERNAL_H
#include "zcl_transaction_review.h"

/* Separate unit keeps the owned parsed transaction out of the publisher's
 * optimized stack frame. Caller supplies private, initialized candidate data. */
zcl_status zcl_review_prepare(const uint8_t *wire, size_t length, zcl_network network,
                              const zcl_previous_transaction *previous, size_t previous_count,
                              uint64_t maximum_fee, zcl_review_data *candidate);
/* Shared lifetime transition; caller checks owner != NULL and holds its lock. */
zcl_status zcl_review_live(zcl_review_owner *owner, uint64_t id, uint64_t now_ms);

/* Internal public-data operation, NOT signing/approval. Resolve the amount and
 * exact standard P2PKH script exclusively from the owned assessment of this
 * live unsigned review; resolve transaction fields from its owned wire. No
 * borrowed previous transaction, caller-supplied amount or script is accepted.
 * P2SH inputs refuse: their redeem script is not part of this review contract.
 * branch is explicit public domain data, NOT authenticated current-chain state.
 * No key, consent, ownership, inclusion/unspentness or finality is established.
 * The same serialized-access, ID, monotonic-clock and nonoverlap requirements
 * apply as for review reads. A live call advances last_ms even if capacity,
 * index or destination refuses; the fixed deadline never extends. NULL owner/
 * digest refuses before any transition. Digest capacity must be >=32; only its
 * first32 raw bytes publish on success and no output changes on failure.
 * Cancellation/expiry/rollback clears the review under the existing rules.
 * This has no JNI caller and cannot retain an authorization across callbacks.
 */
zcl_status zcl_review_sighash_p2pkh(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, uint32_t branch, uint8_t *digest, size_t capacity);

/* Invocation-only candidate key location. Every span has a stable caller owner;
 * directory is the trusted app-private path, never network/URI input. Only the
 * v1 receive address (chain0/index0) and consumed change (chain1/index0..65534)
 * are supported. The claim itself establishes no ownership or authorization. */
typedef struct {
    const uint8_t *directory;
    size_t directory_len;
    const uint8_t *record;
    size_t record_len;
    const uint8_t *entropy;
    size_t entropy_len;
    uint32_t chain;
    uint32_t index;
} zcl_review_wallet_input;

/* Internal synchronous ownership comparison, no JNI/signing/approval handle.
 * Platform MUST first authenticate the exact record/header/entropy with GCM
 * and per-use hardware policy; C cannot prove that prerequisite. Match the
 * exact committed wallet, recovered identity/network and requested key location
 * to this live review's P2PKH input destination. Change also authenticates its
 * consumed-index head; no reservation, state initialization or repair occurs.
 * Receive0 can be checked without a change journal; a pending wallet refuses.
 * Validated metadata/path/record/entropy copy before any crypto or storage
 * operation; the whole private work, including entropy, clears before return.
 * Caller still owns/clears its entropy. No pointer, key or result token escapes.
 * Success describes only this input at supplied now_ms, not user consent,
 * funding/unspentness, current branch/height or other inputs. It MUST NOT be
 * cached as authorization. Future signing must recheck exact review/lifetime
 * and authenticated context within its own operation. Same exclusive adapter
 * lock, no reentrancy and nonoverlap rules as other review reads apply. Run on
 * a worker; no wall-clock or filesystem-completion deadline is promised here.
 */
zcl_status zcl_review_input_wallet_check(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_wallet_input *claim);

/* Invocation-only public candidate, not authenticated chain context or consent.
 * height is the candidate block height, not a tip to increment implicitly.
 * lock_time_cutoff is the already selected block/relay finality comparison time,
 * <=INT64_MAX; never use an untrusted server timestamp or Android wall clock as
 * verified chain evidence. Caller owns a stable, nonoverlapping object. */
typedef struct {
    zcl_network network;
    uint32_t height;
    uint64_t lock_time_cutoff;
} zcl_review_block;

/* Public-data P2PKH digest for this exact live review and explicit candidate.
 * Match network; select the pinned v4 branch at its height; compare the owned
 * expiry/lock/sequence fields with the candidate using original IsExpiredTx/
 * IsFinalTx semantics. Zero expiry remains valid; equality is not expired.
 * Lock equality is nonfinal unless EVERY input sequence is UINT32_MAX.
 * Unsupported v4 epoch/network refuses; expired/nonfinal/out-of-domain context
 * returns OUT_OF_RANGE. This is not complete contextual validation, mempool
 * policy, chain authentication, ownership, maturity/unspentness or authority to
 * sign. Caller must independently establish the chain source/currentness and
 * consent; no cached digest becomes authorization. Same lock/ID/clock rules as
 * other review reads; no completion-time deadline is promised. NULL arguments
 * refuse before liveness. Capacity>=32; only32 bytes publish after all checks
 * and hashing succeed; the whole private candidate clears on every work exit.
 * No JNI, network request, implicit next-height calculation or retained handle.
 */
zcl_status zcl_review_sighash_context(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_block *block, uint8_t *digest, size_t capacity);
#endif
