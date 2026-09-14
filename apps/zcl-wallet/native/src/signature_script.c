/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_internal.h"
#include "zcl_keys.h"
#include <mbedtls/ripemd160.h>
#include <mbedtls/sha256.h>
#include <secp256k1.h>
#include <string.h>

typedef struct {
    zcl_signature signature;
    uint8_t digest[32], expected[20], sha256[32], actual[20];
    secp256k1_pubkey key;
    secp256k1_ecdsa_signature parsed;
    uint8_t canonical[72], script[ZCL_SIGNATURE_SCRIPT_MAX];
    size_t canonical_len;
} script_work;

static zcl_status script_arguments(const zcl_signature *signature,
    const uint8_t *digest, size_t digest_len, const uint8_t *hash, size_t hash_len,
    const uint8_t *script, const size_t *length)
{
    if (signature == NULL || digest == NULL || hash == NULL || script == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (digest_len != 32 || hash_len != 20) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status matching_key(script_work *work)
{
    const uint8_t *key = work->signature.public_key;
    if (key[0] != 2 && key[0] != 3) return ZCL_INVALID_ENCODING;
    if (secp256k1_ec_pubkey_parse(secp256k1_context_static, &work->key, key, 33) != 1)
        return ZCL_INVALID_ENCODING;
    if (mbedtls_sha256(key, 33, work->sha256, 0) != 0) return ZCL_CRYPTO_FAILURE;
    if (mbedtls_ripemd160(work->sha256, sizeof(work->sha256), work->actual) != 0) return ZCL_CRYPTO_FAILURE;
    return memcmp(work->actual, work->expected, sizeof(work->actual)) == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

static zcl_status matching_signature(script_work *work)
{
    const secp256k1_context *context = secp256k1_context_static;
    if (secp256k1_ecdsa_signature_parse_der(context, &work->parsed,
        work->signature.der, work->signature.der_len) != 1) return ZCL_INVALID_ENCODING;
    if (secp256k1_ecdsa_signature_normalize(context, NULL, &work->parsed) != 0) return ZCL_INVALID_ENCODING;
    work->canonical_len = sizeof(work->canonical);
    if (secp256k1_ecdsa_signature_serialize_der(context, work->canonical, &work->canonical_len,
        &work->parsed) != 1) return ZCL_CRYPTO_FAILURE;
    if (work->canonical_len != work->signature.der_len) return ZCL_INVALID_ENCODING;
    if (memcmp(work->canonical, work->signature.der, work->canonical_len) != 0) return ZCL_INVALID_ENCODING;
    return secp256k1_ecdsa_verify(context, &work->parsed, work->digest, &work->key) == 1 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

/* The checked8..71-byte DER and33-byte key fit direct pushes (<OP_PUSHDATA1).
 * No caller-selected hash type, script opcode or prefix enters this encoding. */
static void encode_script(script_work *work)
{
    const size_t length = work->signature.der_len;
    work->script[0] = (uint8_t)(length + 1);
    memcpy(work->script + 1, work->signature.der, length);
    work->script[length + 1] = 1; /* SIGHASH_ALL */
    work->script[length + 2] = 33;
    memcpy(work->script + length + 3, work->signature.public_key, 33);
}

zcl_status zcl_signature_p2pkh(const zcl_signature *signature,
    const uint8_t *digest, size_t digest_len, const uint8_t *key_hash, size_t key_hash_len,
    uint8_t *script, size_t capacity, size_t *length)
{
    zcl_status status = script_arguments(signature, digest, digest_len, key_hash, key_hash_len, script, length);
    if (status != ZCL_OK) return status;
    script_work work;
    memset(&work, 0, sizeof(work));
    work.signature.der_len = signature->der_len;
    if (work.signature.der_len < 8 || work.signature.der_len > 71) { status = ZCL_INVALID_ENCODING; goto cleanup; }
    if (capacity < work.signature.der_len + 36) { status = ZCL_BUFFER_TOO_SMALL; goto cleanup; }
    memcpy(work.signature.der, signature->der, work.signature.der_len);
    memcpy(work.signature.public_key, signature->public_key, sizeof(work.signature.public_key));
    memcpy(work.digest, digest, sizeof(work.digest));
    memcpy(work.expected, key_hash, sizeof(work.expected));
    status = matching_key(&work);
    if (status == ZCL_OK) status = matching_signature(&work);
    if (status == ZCL_OK) {
        encode_script(&work);
        memcpy(script, work.script, work.signature.der_len + 36);
        *length = work.signature.der_len + 36;
    }
cleanup:
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
