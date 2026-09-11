/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <string.h>

static zcl_status validate_passphrase(const uint8_t *phrase, size_t length)
{
    if (phrase == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (length > ZCL_PASSPHRASE_MAX)
        return ZCL_OUT_OF_RANGE;
    for (size_t i = 0; i < length; ++i) {
        if (phrase[i] < 0x20 || phrase[i] > 0x7e)
            return ZCL_UNSUPPORTED;
    }
    return ZCL_OK;
}

static zcl_status validate_mnemonic(const uint8_t *text, size_t text_len)
{
    uint8_t entropy[32] = {0};
    size_t length = 0;
    zcl_status status = zcl_mnemonic_decode(text, text_len, entropy, sizeof(entropy), &length);
    zcl_secure_zero(entropy, sizeof(entropy));
    return status;
}

static zcl_status pbkdf2(const uint8_t *text, size_t text_len,
                         const uint8_t *salt, size_t salt_len,
                         uint8_t *seed, size_t capacity)
{
    uint8_t current[64] = {0}, next[64] = {0}, result[64] = {0};
    zcl_status status = ZCL_BUFFER_TOO_SMALL;
    if (capacity < sizeof(result))
        goto cleanup;
    status = zcl_hmac_sha512(text, text_len, salt, salt_len, current, sizeof(current));
    if (status != ZCL_OK)
        goto cleanup;
    memcpy(result, current, sizeof(result));
    for (size_t iteration = 1; iteration < 2048; ++iteration) {
        status = zcl_hmac_sha512(text, text_len, current, sizeof(current), next, sizeof(next));
        if (status != ZCL_OK)
            goto cleanup;
        for (size_t i = 0; i < sizeof(result); ++i)
            result[i] ^= next[i];
        memcpy(current, next, sizeof(current));
    }
    memcpy(seed, result, sizeof(result));
cleanup:
    zcl_secure_zero(current, sizeof(current));
    zcl_secure_zero(next, sizeof(next));
    zcl_secure_zero(result, sizeof(result));
    return status;
}

zcl_status zcl_mnemonic_seed(const uint8_t *text, size_t text_len,
                            const uint8_t *passphrase, size_t passphrase_len,
                            uint8_t *seed, size_t seed_capacity)
{
    if (text == NULL || seed == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (seed_capacity < ZCL_SEED_BYTES)
        return ZCL_BUFFER_TOO_SMALL;
    zcl_status status = validate_passphrase(passphrase, passphrase_len);
    if (status != ZCL_OK)
        return status;
    status = validate_mnemonic(text, text_len);
    if (status != ZCL_OK)
        return status;
    uint8_t salt[140] = {'m', 'n', 'e', 'm', 'o', 'n', 'i', 'c'};
    memcpy(salt + 8, passphrase, passphrase_len);
    /* The only PBKDF2 block is numbered one, encoded as uint32 big-endian. */
    salt[8 + passphrase_len + 3] = 1;
    status = pbkdf2(text, text_len, salt, 8 + passphrase_len + 4, seed, seed_capacity);
    zcl_secure_zero(salt, sizeof(salt));
    return status;
}
