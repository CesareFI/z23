/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "bip32_oracle.h"
#include "zcl_wallet_record.h"
#include "zcl_recovery.h"

#include <openssl/bn.h>
#include <openssl/evp.h>
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

static int oracle_derive(const uint8_t *seed, size_t seed_len, const uint32_t *path, size_t depth,
                          uint8_t *secret, size_t capacity)
{
    if (seed_len != 64 || depth != 5 || capacity < 32)
        return 0;
    zcl_extended_private node = {0}, child = {0};
    int okay = 0;
    if (zcl_test_bip32_master(seed, seed_len, &node) != ZCL_OK)
        goto cleanup;
    for (size_t i = 0; i < depth; ++i) {
        if (zcl_test_bip32_child(&node, path[i], &child) != ZCL_OK)
            goto cleanup;
        node = child;
        OPENSSL_cleanse(&child, sizeof(child));
    }
    memcpy(secret, node.secret, 32);
    okay = 1;
cleanup:
    OPENSSL_cleanse(&node, sizeof(node));
    OPENSSL_cleanse(&child, sizeof(child));
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
    if (!zcl_test_bip32_public(secret, secret_len, public_key, sizeof(public_key)))
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

static int recovered_change(const uint8_t *entropy, size_t length, zcl_network network,
    uint32_t index, const uint8_t expected[35])
{
    uint8_t header[80] = {0}, blinding[64] = {0}, output[37];
    memset(blinding, 1, 32); memset(blinding + 32, 2, 32);
    memset(output, 0xa5, sizeof(output));
    CHECK(zcl_wallet_header_create(entropy, length, network, blinding, 32, header, sizeof(header)) == ZCL_OK);
    CHECK(zcl_wallet_recovered_change(header, sizeof(header), entropy, length, index,
        blinding, sizeof(blinding), output + 1, 35) == ZCL_OK);
    CHECK(output[0] == 0xa5 && output[36] == 0xa5 && memcmp(output + 1, expected, 35) == 0);
    zcl_secure_zero(blinding, sizeof(blinding));
    return 0;
}

static int check_fixture(size_t fixture_index, zcl_network network, uint32_t chain, uint32_t index)
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
    uint32_t path[] = {UINT32_C(0x8000002c), UINT32_C(0x80000000) | coin, UINT32_C(0x80000000), chain, index};
    CHECK(oracle_derive(seed, sizeof(seed), path, 5, secret, sizeof(secret)));
    CHECK(oracle_address(secret, sizeof(secret), network, expected, sizeof(expected)));
    memset(actual, 0xa5, sizeof(actual));
    const zcl_status status = chain == 0
        ? zcl_receive_from_entropy(entropy, item->entropy_len, network, index, blinding, sizeof(blinding),
                                    actual + 1, 35, &actual_len)
        : zcl_change_from_entropy(entropy, item->entropy_len, network, index, blinding, sizeof(blinding),
                                    actual + 1, 35, &actual_len);
    CHECK(status == ZCL_OK);
    CHECK(actual_len == 35 && actual[0] == 0xa5 && actual[36] == 0xa5);
    CHECK(memcmp(actual + 1, expected, sizeof(expected)) == 0);
    memset(actual, 0xa5, sizeof(actual));
    CHECK(zcl_recovery_address_batch(entropy, item->entropy_len, network, chain, index, 1,
        blinding, sizeof(blinding), actual + 1, 35) == ZCL_OK);
    CHECK(actual[0] == 0xa5 && actual[36] == 0xa5);
    CHECK(memcmp(actual + 1, expected, sizeof(expected)) == 0);
    if (chain == 1) CHECK(recovered_change(entropy, item->entropy_len, network, index, expected) == 0);
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
            for (uint32_t chain = 0; chain < 2; ++chain) {
                CHECK(check_fixture(selected[i], ZCL_MAINNET, chain, indices[j]) == 0);
                CHECK(check_fixture(selected[i], ZCL_TESTNET, chain, indices[j]) == 0);
            }
        }
    }
    puts("receive/change: 96 independent single and batch comparisons, 48 recovered change bindings passed");
    return 0;
}
