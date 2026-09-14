/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_oracle.h"
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/params.h>
#include <string.h>

static int public_matches(const uint8_t *secret, const uint8_t *expected)
{
    int okay = 0;
    EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX *context = BN_CTX_new();
    BIGNUM *scalar = BN_bin2bn(secret, 32, NULL);
    EC_POINT *point = group == NULL ? NULL : EC_POINT_new(group);
    uint8_t actual[33] = {0};
    if (group == NULL || context == NULL || scalar == NULL || point == NULL) goto cleanup;
    if (EC_POINT_mul(group, point, scalar, NULL, NULL, context) != 1) goto cleanup;
    if (EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, actual, sizeof(actual), context) != 33) goto cleanup;
    okay = memcmp(actual, expected, 33) == 0;
cleanup:
    EC_POINT_clear_free(point);
    BN_clear_free(scalar);
    BN_CTX_free(context);
    EC_GROUP_free(group);
    return okay;
}

static int canonical_encoding(const ECDSA_SIG *parsed, const zcl_signature *signature)
{
    const int length = i2d_ECDSA_SIG(parsed, NULL);
    if (length <= 0 || (size_t)length != signature->der_len) return 0;
    uint8_t encoded[72] = {0};
    uint8_t *cursor = encoded;
    if (i2d_ECDSA_SIG(parsed, &cursor) != length) return 0;
    if (cursor != encoded + signature->der_len) return 0;
    return memcmp(encoded, signature->der, signature->der_len) == 0;
}

static int canonical_low_s(const zcl_signature *signature)
{
    int okay = 0;
    const uint8_t *cursor = signature->der;
    ECDSA_SIG *parsed = d2i_ECDSA_SIG(NULL, &cursor, (long)signature->der_len);
    BIGNUM *half = NULL;
    const BIGNUM *r = NULL, *s = NULL;
    if (parsed == NULL || cursor != signature->der + signature->der_len) goto cleanup;
    if (!canonical_encoding(parsed, signature)) goto cleanup;
    ECDSA_SIG_get0(parsed, &r, &s);
    if (r == NULL || s == NULL) goto cleanup;
    if (BN_is_zero(r) || BN_is_negative(r) || BN_is_zero(s) || BN_is_negative(s)) goto cleanup;
    if (BN_hex2bn(&half, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141") != 64) goto cleanup;
    if (half == NULL || BN_rshift1(half, half) != 1) goto cleanup;
    okay = BN_cmp(s, half) <= 0;
cleanup:
    BN_free(half);
    ECDSA_SIG_free(parsed);
    return okay;
}

static int verify_digest(const uint8_t *digest, const zcl_signature *signature)
{
    int okay = 0;
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *import = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    EVP_PKEY_CTX *verification = NULL;
    char group[] = "secp256k1";
    uint8_t public_key[33];
    memcpy(public_key, signature->public_key, sizeof(public_key));
    OSSL_PARAM parameters[3];
    parameters[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group, 0);
    parameters[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, public_key, sizeof(public_key));
    parameters[2] = OSSL_PARAM_construct_end();
    if (import == NULL || EVP_PKEY_fromdata_init(import) != 1) goto cleanup;
    if (EVP_PKEY_fromdata(import, &key, EVP_PKEY_PUBLIC_KEY, parameters) != 1 || key == NULL) goto cleanup;
    verification = EVP_PKEY_CTX_new(key, NULL);
    if (verification == NULL || EVP_PKEY_verify_init(verification) != 1) goto cleanup;
    /* EVP_PKEY_verify consumes the already computed32-byte digest; this does
     * not hash it again or replace Zclassic's BLAKE2 signature-hash domain. */
    if (EVP_PKEY_CTX_set_signature_md(verification, EVP_sha256()) != 1) goto cleanup;
    okay = EVP_PKEY_verify(verification, signature->der, signature->der_len, digest, 32) == 1;
cleanup:
    EVP_PKEY_CTX_free(verification);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(import);
    return okay;
}

int zcl_test_signature_oracle(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, const zcl_signature *signature)
{
    if (secret == NULL || digest == NULL || signature == NULL) return 0;
    if (secret_len != 32 || digest_len != 32) return 0;
    if (signature->der_len < 8 || signature->der_len > 72) return 0;
    return public_matches(secret, signature->public_key) && canonical_low_s(signature) && verify_digest(digest, signature);
}
