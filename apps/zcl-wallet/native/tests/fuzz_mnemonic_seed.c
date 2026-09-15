/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void check_seed(const uint8_t *text, size_t text_length,
                        const uint8_t *phrase, size_t phrase_length, size_t capacity)
{
    uint8_t actual[66], before[66], expected[64] = {0};
    uint8_t salt[136] = {'m', 'n', 'e', 'm', 'o', 'n', 'i', 'c'};
    if (text_length > ZCL_MNEMONIC_MAX || phrase_length > ZCL_PASSPHRASE_MAX || capacity >= 64)
        abort();
    memset(actual, 0xa5, sizeof(actual));
    memcpy(before, actual, sizeof(before));
    if (zcl_mnemonic_seed(text, text_length, phrase, phrase_length,
        actual + 1, capacity) != ZCL_BUFFER_TOO_SMALL)
        abort();
    if (memcmp(actual, before, sizeof(actual)) != 0)
        abort();
    memcpy(salt + 8, phrase, phrase_length);
    if (PKCS5_PBKDF2_HMAC((const char *)text, (int)text_length, salt,
        (int)(8 + phrase_length), 2048, EVP_sha512(), 64, expected) != 1)
        abort();
    if (zcl_mnemonic_seed(text, text_length, phrase, phrase_length,
        actual + 1, 64) != ZCL_OK)
        abort();
    if (actual[0] != 0xa5 || actual[65] != 0xa5 ||
        memcmp(actual + 1, expected, sizeof(expected)) != 0)
        abort();
    zcl_secure_zero(actual, sizeof(actual));
    zcl_secure_zero(expected, sizeof(expected));
    zcl_secure_zero(salt, sizeof(salt));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 16 || size > 160)
        return 0;
    const size_t entropy_length = 16 + (size_t)(data[0] % 5) * 4;
    if (size < entropy_length)
        return 0;
    size_t phrase_length = size - entropy_length;
    if (phrase_length > ZCL_PASSPHRASE_MAX)
        phrase_length = ZCL_PASSPHRASE_MAX;
    uint8_t text[215] = {0}, phrase[128] = {0};
    size_t text_length = 0;
    if (zcl_mnemonic_encode(data, entropy_length, text, sizeof(text), &text_length) != ZCL_OK)
        abort();
    for (size_t i = 0; i < phrase_length; ++i)
        phrase[i] = (uint8_t)(UINT8_C(0x20) + data[entropy_length + i] % 95);
    check_seed(text, text_length, phrase, phrase_length, (size_t)(data[0] % 64));
    zcl_secure_zero(text, sizeof(text));
    zcl_secure_zero(phrase, sizeof(phrase));
    return 0;
}
