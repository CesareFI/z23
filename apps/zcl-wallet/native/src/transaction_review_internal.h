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
#endif
