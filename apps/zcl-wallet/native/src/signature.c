/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_internal.h"
#include "ec_context.h"
#include <string.h>

typedef struct {
    zcl_ec_context context;
    uint8_t secret[32], digest[32], blinding[32];
    secp256k1_pubkey key, parsed_key;
    secp256k1_ecdsa_signature signature, parsed_signature;
    zcl_signature result;
} signature_work;

/* Provider callback owns no spans. The pinned signer supplies valid32-byte
 * buffers and NULL algorithm/data. The cap also bounds RFC6979's counter loop;
 * it is wallet resource policy, not a consensus or nonce-algorithm change. */
static int bounded_nonce(unsigned char *nonce32, const unsigned char *msg32,
    const unsigned char *key32, const unsigned char *algo16, void *data, unsigned int attempt)
{
    int okay = 0;
    if (attempt < ZCL_SIGNATURE_NONCE_ATTEMPTS && algo16 == NULL && data == NULL)
        okay = secp256k1_nonce_function_rfc6979(nonce32, msg32, key32, NULL, NULL, attempt);
    if (okay != 1) { zcl_secure_zero(nonce32, 32); return 0; }
    return 1;
}

static zcl_status sign_material(signature_work *work)
{
    secp256k1_context *context = work->context.handle;
    if (secp256k1_ec_pubkey_create(context, &work->key, work->secret) != 1) return ZCL_CRYPTO_FAILURE;
    return secp256k1_ecdsa_sign(context, &work->signature, work->digest, work->secret, bounded_nonce, NULL) == 1
        ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

static zcl_status encode_material(signature_work *work)
{
    const secp256k1_context *context = secp256k1_context_static;
    if (secp256k1_ecdsa_signature_normalize(context, NULL, &work->signature) != 0) return ZCL_CRYPTO_FAILURE;
    size_t length = sizeof(work->result.public_key);
    if (secp256k1_ec_pubkey_serialize(context, work->result.public_key, &length, &work->key,
        SECP256K1_EC_COMPRESSED) != 1) return ZCL_CRYPTO_FAILURE;
    if (length != sizeof(work->result.public_key)) return ZCL_CRYPTO_FAILURE;
    work->result.der_len = sizeof(work->result.der);
    if (secp256k1_ecdsa_signature_serialize_der(context, work->result.der, &work->result.der_len,
        &work->signature) != 1) return ZCL_CRYPTO_FAILURE;
    if (work->result.der_len < 8 || work->result.der_len > sizeof(work->result.der)) return ZCL_CRYPTO_FAILURE;
    memset(work->result.der + work->result.der_len, 0, sizeof(work->result.der) - work->result.der_len);
    return ZCL_OK;
}

static zcl_status verify_material(signature_work *work)
{
    const secp256k1_context *context = secp256k1_context_static;
    if (secp256k1_ec_pubkey_parse(context, &work->parsed_key, work->result.public_key,
        sizeof(work->result.public_key)) != 1) return ZCL_CRYPTO_FAILURE;
    if (secp256k1_ec_pubkey_cmp(context, &work->key, &work->parsed_key) != 0) return ZCL_CRYPTO_FAILURE;
    if (secp256k1_ecdsa_signature_parse_der(context, &work->parsed_signature, work->result.der,
        work->result.der_len) != 1) return ZCL_CRYPTO_FAILURE;
    if (secp256k1_ecdsa_signature_normalize(context, NULL, &work->parsed_signature) != 0) return ZCL_CRYPTO_FAILURE;
    return secp256k1_ecdsa_verify(context, &work->parsed_signature, work->digest, &work->parsed_key) == 1
        ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

static zcl_status signature_arguments(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, const zcl_signature *output)
{
    if (secret == NULL || digest == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (secret_len != 32 || digest_len != 32) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

zcl_status zcl_signature_create(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, zcl_signature *output)
{
    zcl_status status = signature_arguments(secret, secret_len, digest, digest_len, output);
    if (status != ZCL_OK) return status;
    signature_work work;
    memset(&work, 0, sizeof(work));
    memcpy(work.secret, secret, 32);
    memcpy(work.digest, digest, 32);
    status = zcl_random_bytes(work.blinding, sizeof(work.blinding));
    if (status == ZCL_OK) status = zcl_ec_begin(&work.context, work.blinding, sizeof(work.blinding));
    /* Context initialization consumed these bytes. Signing is the last
     * consumer of the owned randomized context and private scalar. */
    zcl_secure_zero(work.blinding, sizeof(work.blinding));
    if (status == ZCL_OK) status = sign_material(&work);
    zcl_secure_zero(work.secret, sizeof(work.secret));
    zcl_ec_end(&work.context);
    /* Context construction already ran the provider self-test. Remaining
     * encoding/verification is public-only and accepts its static context. */
    if (status == ZCL_OK) status = encode_material(&work);
    if (status == ZCL_OK) status = verify_material(&work);
    if (status == ZCL_OK) memcpy(output, &work.result, sizeof(work.result));
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
