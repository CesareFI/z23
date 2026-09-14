/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SIGNATURE_ORACLE_H
#define ZCL_SIGNATURE_ORACLE_H
#include "signature_internal.h"
/* Host-only OpenSSL verification of public synthetic fixtures. Independently
 * derive the public key, check canonical DER/low-S and verify the raw digest.
 * No wallet/secp/hash-provider helper is called and no object is retained. */
int zcl_test_signature_oracle(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, const zcl_signature *signature);
/* Independently verify only public P2PKH signature/digest/key-hash data. */
int zcl_test_signature_script_oracle(const zcl_signature *signature,
    const uint8_t *digest, size_t digest_len, const uint8_t *hash, size_t hash_len);
#endif
