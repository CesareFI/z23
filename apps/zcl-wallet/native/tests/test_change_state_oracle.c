/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_change_state.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <stdio.h>
#include <string.h>

/* Independent host HKDF/HMAC backend and record bytes. The supplied wallet
 * header is public test context; this is not another address-derivation oracle.
 * OpenSSL is never linked into Android. No secret diagnostic output. */
static int oracle_key(const uint8_t *entropy, size_t entropy_len,
                       const uint8_t *header, uint8_t *output)
{
    if (entropy_len > 32) return 0;
    static const uint8_t salt[] = "Zclassic Android change state v1";
    static const uint8_t label[] = "authenticated index record";
    uint8_t info[sizeof(label) - 1 + 80];
    memcpy(info, label, sizeof(label) - 1);
    memcpy(info + sizeof(label) - 1, header, 80);
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    int okay = 0;
    size_t length = 64;
    if (context == NULL) goto cleanup;
    if (EVP_PKEY_derive_init(context) != 1) goto cleanup;
    if (EVP_PKEY_CTX_set_hkdf_md(context, EVP_sha512()) != 1) goto cleanup;
    if (EVP_PKEY_CTX_set1_hkdf_salt(context, salt, (int)(sizeof(salt) - 1)) != 1) goto cleanup;
    if (EVP_PKEY_CTX_set1_hkdf_key(context, entropy, (int)entropy_len) != 1) goto cleanup;
    if (EVP_PKEY_CTX_add1_hkdf_info(context, info, (int)sizeof(info)) != 1) goto cleanup;
    okay = EVP_PKEY_derive(context, output, &length) == 1 && length == 64;
cleanup:
    EVP_PKEY_CTX_free(context);
    return okay;
}

static int oracle_record(const uint8_t *entropy, size_t entropy_len,
                          const uint8_t *header, uint32_t next, uint8_t *record)
{
    uint8_t key[64] = {0};
    int okay = 0;
    unsigned int tag_length = 0;
    memset(record, 0, 80);
    memcpy(record, "ZCLI\001\001", 6);
    for (size_t i = 0; i < 4; ++i) record[8 + i] = (uint8_t)((next >> (8 * i)) & 0xffU);
    if (!oracle_key(entropy, entropy_len, header, key)) goto cleanup;
    okay = HMAC(EVP_sha512(), key, 64, record, 16, record + 16, &tag_length) != NULL && tag_length == 64;
cleanup:
    OPENSSL_cleanse(key, sizeof(key));
    return okay;
}

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Change state oracle failed at %d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    uint8_t entropy[32], blinding[32] = {1};
    const uint32_t values[] = {0, 1, UINT32_C(0x7fffffff), ZCL_CHANGE_INDEX_EXHAUSTED};
    for (size_t size = 16; size <= 32; size += 4) for (int network = 0; network < 2; ++network) {
        uint8_t header[80];
        for (size_t i = 0; i < sizeof(entropy); ++i) entropy[i] = (uint8_t)(i + size);
        CHECK(zcl_wallet_header_create(entropy, size, (zcl_network)network, blinding, sizeof(blinding),
            header, sizeof(header)) == ZCL_OK);
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
            uint8_t expected[80], actual[80];
            uint32_t decoded = UINT32_MAX;
            CHECK(oracle_record(entropy, size, header, values[i], expected));
            CHECK(zcl_change_state_encode(header, sizeof(header), entropy, size, blinding, sizeof(blinding),
                values[i], actual, sizeof(actual)) == ZCL_OK);
            CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
            CHECK(zcl_change_state_decode(header, sizeof(header), entropy, size, blinding, sizeof(blinding),
                expected, sizeof(expected), &decoded) == ZCL_OK && decoded == values[i]);
        }
    }
    zcl_secure_zero(entropy, sizeof(entropy));
    puts("Change state: 40 OpenSSL HKDF-SHA512/HMAC record comparisons passed");
    return 0;
}
