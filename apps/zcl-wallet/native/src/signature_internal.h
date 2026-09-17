/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SIGNATURE_INTERNAL_H
#define ZCL_SIGNATURE_INTERNAL_H
#include "zcl_wallet.h"

#define ZCL_SIGNATURE_DER_MAX ((size_t)72)
#define ZCL_SIGNATURE_NONCE_ATTEMPTS 8U

/* Public output only. DER has no sighash-type byte or script pushes; unused
 * bytes/padding are zero. This is not a signed transaction or authorization. */
typedef struct {
    uint8_t der[ZCL_SIGNATURE_DER_MAX];
    size_t der_len;
    uint8_t public_key[33];
} zcl_signature;

/* Internal raw-digest primitive, no JNI or wallet unlock. Both input spans are
 * exactly32 bytes and remain caller-owned, stable and nonoverlapping with the
 * fixed-size output. Copy before OS randomness/provider work; private copies,
 * OS blinding and the transient bounded EC context clear on every work exit.
 * Caller must clear its own secret. Invalid secret/RNG/provider/encoding or
 * self-verification refuses without modifying any output byte. No key retry.
 * Deterministic RFC6979/HMAC-SHA256, at most8 nonce candidates, low-S DER and
 * compressed public key. Parsing the encoded public outputs and
 * verifying their consistency precedes publication. No pointers are retained.
 * This primitive does not establish authority to sign its inputs. A future
 * wallet caller MUST bind authenticated wallet/key ownership, current chain
 * context, exact live review and consent in its own operation before invoking
 * it, and must recheck cancellation/completion before publishing a transaction.
 * The internal review-bound composition retains those caller prerequisites;
 * current execution callers are isolated public synthetic fixtures only. */
zcl_status zcl_signature_create(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, zcl_signature *output);

#define ZCL_SIGNATURE_SCRIPT_MAX ((size_t)107)
/* Public-data verifier/encoder only, no secret or signing authority. Accept
 * strict DER/low-S and compressed33-byte keys; independently match HASH160 of
 * that key against the supplied20-byte P2PKH destination and verify the exact
 * supplied32-byte digest. Append SIGHASH_ALL and use two minimal direct pushes.
 * The digest's transaction/branch/amount and the funding hash MUST come from
 * the same owned review in a wallet caller. This helper proves no source,
 * current chain, consent, unspentness or completion-time freshness.
 * All inputs copy before provider work, remain stable/caller-owned and do not
 * overlap outputs. No heap, RNG, retained pointer or JNI. Capacity must fit
 * DER length+36 (44..107). Only that prefix and length publish on complete
 * success; every failure preserves both outputs. Work clears on every exit.
 * Unused signature bytes/padding are ignored and never enter the script. */
zcl_status zcl_signature_p2pkh(const zcl_signature *signature,
    const uint8_t *digest, size_t digest_len, const uint8_t *key_hash, size_t key_hash_len,
    uint8_t *script, size_t capacity, size_t *length);
#endif
