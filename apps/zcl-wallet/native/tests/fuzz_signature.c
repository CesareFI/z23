/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_internal.h"
#ifdef ZCL_SIGNATURE_ORACLE
#include "signature_oracle.h"
#endif
#include <secp256k1.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static size_t claimed_length(uint8_t control)
{
    if (control == 255) return SIZE_MAX;
    return (control & 128U) != 0 ? control % 65 : 32;
}

static zcl_status expected_status(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, const zcl_signature *output)
{
    if (secret == NULL || digest == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (secret_len != 32 || digest_len != 32) return ZCL_OUT_OF_RANGE;
    return secp256k1_ec_seckey_verify(secp256k1_context_static, secret) == 1 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

static void valid_signature(const uint8_t *secret, const uint8_t *digest, const zcl_signature *signature)
{
    if (signature->der_len < 8 || signature->der_len > 71) abort();
    for (size_t i = signature->der_len; i < 72; ++i) if (signature->der[i] != 0) abort();
    secp256k1_pubkey key = {{0}};
    secp256k1_ecdsa_signature parsed = {{0}};
    if (secp256k1_ec_pubkey_parse(secp256k1_context_static, &key, signature->public_key, 33) != 1) abort();
    if (secp256k1_ecdsa_signature_parse_der(secp256k1_context_static, &parsed, signature->der, signature->der_len) != 1) abort();
    if (secp256k1_ecdsa_signature_normalize(secp256k1_context_static, NULL, &parsed) != 0) abort();
    if (secp256k1_ecdsa_verify(secp256k1_context_static, &parsed, digest, &key) != 1) abort();
#ifdef ZCL_SIGNATURE_ORACLE
    if (!zcl_test_signature_oracle(secret, 32, digest, 32, signature)) abort();
#endif
    zcl_signature repeated;
    memset(&repeated, 0xa5, sizeof(repeated));
    if (zcl_signature_create(secret, 32, digest, 32, &repeated) != ZCL_OK) abort();
    if (memcmp(signature, &repeated, sizeof(repeated)) != 0) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size != 66) return 0;
    const uint8_t *secret = data[0] == 254 ? NULL : data + 2;
    const uint8_t *digest = data[1] == 254 ? NULL : data + 34;
    const size_t secret_len = claimed_length(data[0]), digest_len = claimed_length(data[1]);
    struct { uint8_t before[8]; zcl_signature signature; uint8_t after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_signature original;
    memcpy(&original, &box.signature, sizeof(original));
    zcl_signature *output = data[0] == 253 ? NULL : &box.signature;
    const zcl_status status = expected_status(secret, secret_len, digest, digest_len, output);
    if (zcl_signature_create(secret, secret_len, digest, digest_len, output) != status) abort();
    for (size_t i = 0; i < 8; ++i) if (box.before[i] != 0xa5 || box.after[i] != 0xa5) abort();
    if (status == ZCL_OK) valid_signature(secret, digest, output);
    else if (memcmp(&box.signature, &original, sizeof(original)) != 0) abort();
    return 0;
}
