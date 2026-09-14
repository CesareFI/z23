/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_SIGHASH_H
#define ZCL_TRANSACTION_SIGHASH_H
#include "zcl_transaction.h"

/* Internal public-data hash only: the bounded transparent v4 codec profile,
 * SIGHASH_ALL, and an explicit transparent input. scriptCode is an exact public
 * byte span of 0..128 bytes, not an implicitly resolved previous/redeem script.
 * Input scriptSig bytes are excluded as specified by the v4 hash construction.
 * Amount must be <= MAX_MONEY; branch is an explicit uint32 domain value.
 * Caller owns stable nonoverlapping objects/spans throughout this call; NULL
 * is rejected even for empty scriptCode. Output needs >=32 bytes; only its
 * first 32 bytes change on success, and none change on failure. Digest order
 * is raw hash bytes, not the reversed display order used by transaction IDs.
 * No allocation, retained pointer, secret, JNI or signing authority. This does
 * not establish current branch/height, script validity, funding/unspentness,
 * wallet ownership, authenticated review, user approval or chain acceptance.
 */
zcl_status zcl_transaction_sighash_all(const zcl_transparent_tx *transaction,
    size_t input_index, const uint8_t *script_code, size_t script_length,
    uint64_t amount, uint32_t branch, uint8_t *digest, size_t capacity);
#endif
