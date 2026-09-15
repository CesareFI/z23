/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_state_oracle.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <string.h>

/* Extracted from the existing fixed-vector oracle. Use OpenSSL's HKDF engine,
 * independently of the wallet's explicit extract/expand implementation. */
static int oracle_key(const uint8_t *entropy, size_t entropy_len,
    const uint8_t header[80], uint8_t output[64])
{
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

int change_state_oracle_record(const uint8_t *entropy, size_t entropy_len,
    const uint8_t *header, size_t header_len, uint32_t next, uint8_t *record, size_t capacity)
{
    if (entropy == NULL || header == NULL || record == NULL) return 0;
    if (entropy_len < 16 || entropy_len > 32 || entropy_len % 4 != 0) return 0;
    if (header_len != 80 || capacity < 80 || next > UINT32_C(0x80000000)) return 0;
    uint8_t key[64] = {0}, candidate[80] = {0};
    int okay = 0;
    unsigned int tag_length = 0;
    memcpy(candidate, "ZCLI\001\001", 6);
    candidate[8] = (uint8_t)(next & UINT32_C(0xff));
    candidate[9] = (uint8_t)((next >> 8) & UINT32_C(0xff));
    candidate[10] = (uint8_t)((next >> 16) & UINT32_C(0xff));
    candidate[11] = (uint8_t)(next >> 24);
    if (!oracle_key(entropy, entropy_len, header, key)) goto cleanup;
    okay = HMAC(EVP_sha512(), key, 64, candidate, 16, candidate + 16, &tag_length) != NULL && tag_length == 64;
    if (okay) memcpy(record, candidate, sizeof(candidate));
cleanup:
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(candidate, sizeof(candidate));
    return okay;
}
