/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>
#include <stdio.h>
#include <string.h>

/* Host-only independent implementation. OpenSSL is never linked into the app.
 * All inputs are published fixtures, and every owned provider object has one
 * cleanup path. Shared app mnemonic/HD/hash/address code is not used by oracle. */
typedef struct {
    uint8_t entropy_hex[65]; size_t entropy_len;
    uint8_t mnemonic[216]; size_t mnemonic_len; uint8_t seed_hex[129];
} fixture;
static const fixture fixtures[] = {
#include "bip39_vectors.inc"
};

static int oracle_public(const uint8_t *secret, size_t secret_len, uint8_t *out, size_t capacity)
{
    if (secret_len != 32 || capacity < 33)
        return 0;
    int okay = 0;
    EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX *context = BN_CTX_new();
    BIGNUM *scalar = BN_bin2bn(secret, 32, NULL);
    EC_POINT *point = group == NULL ? NULL : EC_POINT_new(group);
    if (group == NULL || context == NULL || scalar == NULL || point == NULL)
        goto cleanup;
    if (EC_POINT_mul(group, point, scalar, NULL, NULL, context) != 1)
        goto cleanup;
    okay = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, out, capacity, context) == 33;
cleanup:
    EC_POINT_clear_free(point);
    BN_clear_free(scalar);
    BN_CTX_free(context);
    EC_GROUP_free(group);
    return okay;
}

static int oracle_add(uint8_t *secret, size_t secret_len, const uint8_t *tweak, size_t tweak_len)
{
    if (secret_len != 32 || tweak_len != 32)
        return 0;
    int okay = 0;
    BN_CTX *context = BN_CTX_new();
    BIGNUM *left = BN_bin2bn(secret, 32, NULL), *right = BN_bin2bn(tweak, 32, NULL), *order = NULL;
    int digits = BN_hex2bn(&order, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
    if (context == NULL || left == NULL || right == NULL || order == NULL || digits != 64)
        goto cleanup;
    if (BN_cmp(right, order) >= 0 || BN_mod_add(left, left, right, order, context) != 1 || BN_is_zero(left))
        goto cleanup;
    okay = BN_bn2binpad(left, secret, 32) == 32;
cleanup:
    BN_clear_free(left);
    BN_clear_free(right);
    BN_free(order);
    BN_CTX_free(context);
    return okay;
}

static int oracle_derive(const uint8_t *seed, size_t seed_len, const uint32_t *path, size_t depth,
                          uint8_t *secret, size_t capacity)
{
    if (seed_len != 64 || depth != 5 || capacity < 32)
        return 0;
    uint8_t node[64] = {0}, digest[64] = {0}, data[37] = {0};
    unsigned int length = 0;
    int okay = 0;
    if (HMAC(EVP_sha512(), "Bitcoin seed", 12, seed, seed_len, node, &length) == NULL || length != 64)
        goto cleanup;
    for (size_t i = 0; i < depth; ++i) {
        if (path[i] >= UINT32_C(0x80000000)) {
            data[0] = 0;
            memcpy(data + 1, node, 32);
        } else if (!oracle_public(node, 32, data, sizeof(data))) {
            goto cleanup;
        }
        for (size_t j = 0; j < 4; ++j)
            data[33 + j] = (uint8_t)(path[i] >> (24U - (unsigned)j * 8U));
        if (HMAC(EVP_sha512(), node + 32, 32, data, sizeof(data), digest, &length) == NULL || length != 64)
            goto cleanup;
        if (!oracle_add(node, 32, digest, 32))
            goto cleanup;
        memcpy(node + 32, digest + 32, 32);
    }
    memcpy(secret, node, 32);
    okay = 1;
cleanup:
    OPENSSL_cleanse(node, sizeof(node));
    OPENSSL_cleanse(digest, sizeof(digest));
    OPENSSL_cleanse(data, sizeof(data));
    return okay;
}

static int oracle_base58(const uint8_t *payload, size_t payload_len, uint8_t *out, size_t capacity)
{
    if (payload_len != 26 || capacity < 35)
        return 0;
    static const uint8_t alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    uint8_t reversed[35] = {0};
    BIGNUM *number = BN_bin2bn(payload, 26, NULL);
    size_t count = 0;
    int okay = 0;
    if (number == NULL)
        goto cleanup;
    while (!BN_is_zero(number)) {
        BN_ULONG remainder = BN_div_word(number, 58);
        if (remainder >= 58 || count >= sizeof(reversed))
            goto cleanup;
        reversed[count++] = alphabet[remainder];
    }
    if (count != 35)
        goto cleanup;
    for (size_t i = 0; i < count; ++i)
        out[i] = reversed[count - i - 1];
    okay = 1;
cleanup:
    BN_free(number);
    return okay;
}

static int oracle_address(const uint8_t *secret, size_t secret_len,
                           zcl_network network, uint8_t *out, size_t capacity)
{
    uint8_t public_key[33] = {0}, hash[32] = {0}, checksum[32] = {0}, payload[26] = {0};
    unsigned int length = 0;
    if (!oracle_public(secret, secret_len, public_key, sizeof(public_key)))
        return 0;
    payload[0] = network == ZCL_MAINNET ? 0x1c : 0x1d;
    payload[1] = network == ZCL_MAINNET ? 0xb8 : 0x25;
    if (EVP_Digest(public_key, sizeof(public_key), hash, &length, EVP_sha256(), NULL) != 1 || length != 32)
        return 0;
    if (EVP_Digest(hash, sizeof(hash), payload + 2, &length, EVP_ripemd160(), NULL) != 1 || length != 20)
        return 0;
    if (EVP_Digest(payload, 22, hash, &length, EVP_sha256(), NULL) != 1 || length != 32)
        return 0;
    if (EVP_Digest(hash, sizeof(hash), checksum, &length, EVP_sha256(), NULL) != 1 || length != 32)
        return 0;
    memcpy(payload + 22, checksum, 4);
    return oracle_base58(payload, sizeof(payload), out, capacity);
}

static unsigned nibble(uint8_t value)
{
    return value <= '9' ? (unsigned)(value - '0') : (unsigned)(value - 'a') + 10U;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "receive oracle check failed at line %d\n", __LINE__); return 1; } } while (0)

static int check_fixture(size_t fixture_index, zcl_network network, uint32_t index)
{
    const fixture *item = &fixtures[fixture_index];
    uint8_t entropy[32] = {0}, seed[64] = {0}, secret[32] = {0}, blinding[32] = {1};
    uint8_t actual[37] = {0}, expected[35] = {0};
    size_t actual_len = 0;
    CHECK(item->entropy_len <= 32 && item->mnemonic_len <= 215);
    for (size_t i = 0; i < item->entropy_len; ++i)
        entropy[i] = (uint8_t)((nibble(item->entropy_hex[i * 2]) << 4) | nibble(item->entropy_hex[i * 2 + 1]));
    CHECK(PKCS5_PBKDF2_HMAC((const char *)item->mnemonic, (int)item->mnemonic_len,
        (const unsigned char *)"mnemonic", 8, 2048, EVP_sha512(), 64, seed) == 1);
    uint32_t coin = network == ZCL_MAINNET ? 147 : 1;
    uint32_t path[] = {UINT32_C(0x8000002c), UINT32_C(0x80000000) | coin, UINT32_C(0x80000000), 0, index};
    CHECK(oracle_derive(seed, sizeof(seed), path, 5, secret, sizeof(secret)));
    CHECK(oracle_address(secret, sizeof(secret), network, expected, sizeof(expected)));
    memset(actual, 0xa5, sizeof(actual));
    CHECK(zcl_receive_from_entropy(entropy, item->entropy_len, network, index, blinding, sizeof(blinding),
                                    actual + 1, 35, &actual_len) == ZCL_OK);
    CHECK(actual_len == 35 && actual[0] == 0xa5 && actual[36] == 0xa5);
    CHECK(memcmp(actual + 1, expected, sizeof(expected)) == 0);
    OPENSSL_cleanse(entropy, sizeof(entropy));
    OPENSSL_cleanse(seed, sizeof(seed));
    OPENSSL_cleanse(secret, sizeof(secret));
    return 0;
}

int main(void)
{
    static const size_t selected[] = {0, 4, 8, 12, 19, 23};
    static const uint32_t indices[] = {0, 1, 19, UINT32_C(0x7fffffff)};
    for (size_t i = 0; i < sizeof(selected) / sizeof(selected[0]); ++i) {
        for (size_t j = 0; j < sizeof(indices) / sizeof(indices[0]); ++j) {
            CHECK(check_fixture(selected[i], ZCL_MAINNET, indices[j]) == 0);
            CHECK(check_fixture(selected[i], ZCL_TESTNET, indices[j]) == 0);
        }
    }
    puts("receive: 48 independent OpenSSL seed/HD/public-key/hash/address comparisons passed");
    return 0;
}
