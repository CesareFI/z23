/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Host-only oracle passes the original mnemonic to OpenSSL's PBKDF2. It does
 * not share the app's HMAC/key normalization/iteration implementation. */
static bool supported_passphrase(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] < 32 || bytes[i] > 126)
            return false;
    }
    return true;
}

static int compare_seed(const uint8_t *text, size_t length,
                        const uint8_t *passphrase, size_t passphrase_len)
{
    if (text == NULL || passphrase == NULL || length > 215 || passphrase_len > 128)
        abort();
    uint8_t salt[136] = {'m', 'n', 'e', 'm', 'o', 'n', 'i', 'c'};
    uint8_t expected[66], actual[66];
    memcpy(salt + 8, passphrase, passphrase_len);
    memset(expected, 0xa5, sizeof(expected));
    memset(actual, 0xa5, sizeof(actual));
    int okay = 1;
    zcl_status expected_status = ZCL_UNSUPPORTED;
    if (supported_passphrase(passphrase, passphrase_len)) {
        okay = PKCS5_PBKDF2_HMAC((const char *)text, (int)length, salt,
            (int)(8 + passphrase_len), 2048, EVP_sha512(), 64, expected + 1) == 1;
        expected_status = ZCL_OK;
    }
    if (okay) {
        okay = zcl_mnemonic_seed(text, length, passphrase, passphrase_len,
            actual + 1, 64) == expected_status;
        okay = okay && memcmp(actual, expected, sizeof(expected)) == 0;
    }
    OPENSSL_cleanse(salt, sizeof(salt));
    OPENSSL_cleanse(expected, sizeof(expected));
    OPENSSL_cleanse(actual, sizeof(actual));
    return okay;
}

#if defined(ZCL_SEED_FUZZ)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == NULL || size < 33 || size > 161)
        return 0;
    uint8_t text[215] = {0};
    size_t length = 0, entropy_len = 16 + (size_t)(data[0] % 5u) * 4;
    const size_t passphrase_len = size - 33;
    if (zcl_mnemonic_encode(data + 1, entropy_len, text, sizeof(text), &length) != ZCL_OK)
        abort();
    if (!compare_seed(text, length, data + 33, passphrase_len))
        abort();
    zcl_secure_zero(text, sizeof(text));
    return 0;
}
#else
typedef struct {
    uint8_t entropy_hex[65]; size_t entropy_len;
    uint8_t mnemonic[216]; size_t mnemonic_len; uint8_t seed_hex[129];
} fixture;
static const fixture fixtures[] = {
#include "bip39_vectors.inc"
#include "seed_boundary_vectors.inc"
};

int main(void)
{
    uint8_t passphrase[128];
    const size_t lengths[] = {0, 6, 128};
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        for (size_t j = 0; j < sizeof(lengths) / sizeof(lengths[0]); ++j) {
            memset(passphrase, '~', sizeof(passphrase));
            if (lengths[j] == 6)
                memcpy(passphrase, "TREZOR", 6);
            if (!compare_seed(fixtures[i].mnemonic, fixtures[i].mnemonic_len,
                passphrase, lengths[j])) {
                OPENSSL_cleanse(passphrase, sizeof(passphrase));
                fputs("mnemonic seed oracle mismatch\n", stderr);
                return 1;
            }
        }
    }
    OPENSSL_cleanse(passphrase, sizeof(passphrase));
    puts("mnemonic seed: 81 independent PBKDF2 comparisons, including HMAC/passphrase boundaries");
    return 0;
}
#endif
