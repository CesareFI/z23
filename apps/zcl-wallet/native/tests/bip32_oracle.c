/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "bip32_oracle.h"
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>
#include <string.h>

static BIGNUM *curve_order(void)
{
    BIGNUM *order = NULL;
    if (BN_hex2bn(&order, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141") != 64) {
        BN_free(order);
        return NULL;
    }
    return order;
}

/* Distinguish an invalid scalar from an oracle allocation/provider failure. */
static zcl_status scalar_status(const uint8_t *bytes)
{
    BIGNUM *scalar = BN_bin2bn(bytes, 32, NULL), *order = curve_order();
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (scalar != NULL && order != NULL)
        status = BN_is_zero(scalar) || BN_cmp(scalar, order) >= 0 ? ZCL_INVALID_ARGUMENT : ZCL_OK;
    BN_clear_free(scalar);
    BN_free(order);
    return status;
}

int zcl_test_bip32_public(const uint8_t *secret, size_t length, uint8_t *out, size_t capacity)
{
    if (secret == NULL || out == NULL || length != 32 || capacity < 33) return 0;
    if (scalar_status(secret) != ZCL_OK) return 0;
    int okay = 0;
    EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX *context = BN_CTX_new();
    BIGNUM *scalar = BN_bin2bn(secret, 32, NULL);
    EC_POINT *point = group == NULL ? NULL : EC_POINT_new(group);
    if (group == NULL || context == NULL || scalar == NULL || point == NULL) goto cleanup;
    if (EC_POINT_mul(group, point, scalar, NULL, NULL, context) != 1) goto cleanup;
    okay = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, out, 33, context) == 33;
cleanup:
    EC_POINT_clear_free(point);
    BN_clear_free(scalar);
    BN_CTX_free(context);
    EC_GROUP_free(group);
    return okay;
}

static zcl_status add_tweak(uint8_t *secret, const uint8_t *tweak)
{
    zcl_status status = ZCL_CRYPTO_FAILURE;
    BN_CTX *context = BN_CTX_new();
    BIGNUM *left = BN_bin2bn(secret, 32, NULL), *right = BN_bin2bn(tweak, 32, NULL), *order = curve_order();
    if (context == NULL || left == NULL || right == NULL || order == NULL) goto cleanup;
    if (BN_cmp(right, order) >= 0) { status = ZCL_INVALID_CHILD; goto cleanup; }
    if (BN_mod_add(left, left, right, order, context) != 1) goto cleanup;
    if (BN_is_zero(left)) { status = ZCL_INVALID_CHILD; goto cleanup; }
    if (BN_bn2binpad(left, secret, 32) == 32) status = ZCL_OK;
cleanup:
    BN_clear_free(left);
    BN_clear_free(right);
    BN_free(order);
    BN_CTX_free(context);
    return status;
}

zcl_status zcl_test_bip32_master(const uint8_t *seed, size_t length, zcl_extended_private *out)
{
    if (seed == NULL || out == NULL) return ZCL_INVALID_ARGUMENT;
    if (length < 16 || length > 64) return ZCL_OUT_OF_RANGE;
    uint8_t digest[64] = {0};
    unsigned int digest_len = 0;
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (HMAC(EVP_sha512(), "Bitcoin seed", 12, seed, length, digest, &digest_len) == NULL || digest_len != 64)
        goto cleanup;
    if (scalar_status(digest) != ZCL_OK) goto cleanup;
    memcpy(out->secret, digest, 32);
    memcpy(out->chain_code, digest + 32, 32);
    status = ZCL_OK;
cleanup:
    OPENSSL_cleanse(digest, sizeof(digest));
    return status;
}

static int child_data(const zcl_extended_private *parent, uint32_t index, uint8_t *data)
{
    if ((index & UINT32_C(0x80000000)) != 0) {
        data[0] = 0;
        memcpy(data + 1, parent->secret, 32);
    } else if (!zcl_test_bip32_public(parent->secret, 32, data, 33)) return 0;
    /* Explicit bytes keep the reference independent of the wallet's loop. */
    data[33] = (uint8_t)(index >> 24);
    data[34] = (uint8_t)(index >> 16);
    data[35] = (uint8_t)(index >> 8);
    data[36] = (uint8_t)index;
    return 1;
}

zcl_status zcl_test_bip32_child(const zcl_extended_private *parent, uint32_t index, zcl_extended_private *out)
{
    if (parent == NULL || out == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_status status = scalar_status(parent->secret);
    if (status != ZCL_OK) return status;
    uint8_t data[37] = {0}, digest[64] = {0};
    zcl_extended_private result = {0};
    unsigned int length = 0;
    status = ZCL_CRYPTO_FAILURE;
    if (!child_data(parent, index, data)) goto cleanup;
    if (HMAC(EVP_sha512(), parent->chain_code, 32, data, sizeof(data), digest, &length) == NULL || length != 64)
        goto cleanup;
    memcpy(result.secret, parent->secret, 32);
    status = add_tweak(result.secret, digest);
    if (status != ZCL_OK) goto cleanup;
    memcpy(result.chain_code, digest + 32, 32);
    *out = result;
cleanup:
    OPENSSL_cleanse(&result, sizeof(result));
    OPENSSL_cleanse(data, sizeof(data));
    OPENSSL_cleanse(digest, sizeof(digest));
    return status;
}
