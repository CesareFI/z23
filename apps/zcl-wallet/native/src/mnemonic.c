/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "mnemonic_words.h"

#include <mbedtls/sha256.h>
#include <string.h>

zcl_status zcl_mnemonic_encode(const uint8_t *entropy, size_t entropy_len,
                              uint8_t *text, size_t text_capacity, size_t *text_len)
{
    if (entropy == NULL || text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!zcl_entropy_length_valid(entropy_len))
        return ZCL_OUT_OF_RANGE;
    uint8_t bits[33] = {0}, checksum[32] = {0}, result[215] = {0};
    size_t length = 0;
    zcl_status status = ZCL_CRYPTO_FAILURE;
    memcpy(bits, entropy, entropy_len);
    if (mbedtls_sha256(entropy, entropy_len, checksum, 0) != 0)
        goto cleanup;
    bits[entropy_len] = checksum[0];
    size_t words = entropy_len / 4 * 3;
    status = zcl_words_write(bits, sizeof(bits), words, result, sizeof(result), &length);
    if (status != ZCL_OK)
        goto cleanup;
    if (text_capacity < length) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    memcpy(text, result, length);
    *text_len = length;
cleanup:
    zcl_secure_zero(bits, sizeof(bits));
    zcl_secure_zero(checksum, sizeof(checksum));
    zcl_secure_zero(result, sizeof(result));
    return status;
}

static zcl_status checked_entropy_size(size_t words, size_t *size)
{
    if (words < 12 || words > 24 || words % 3 != 0)
        return ZCL_INVALID_ENCODING;
    *size = words / 3 * 4;
    return ZCL_OK;
}

static zcl_status verify_checksum(const uint8_t *bits, size_t bits_len, size_t entropy_len)
{
    if (bits_len != 33 || !zcl_entropy_length_valid(entropy_len))
        return ZCL_INVALID_ARGUMENT;
    uint8_t checksum[32] = {0};
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(bits, entropy_len, checksum, 0) != 0)
        goto cleanup;
    unsigned mask = 0xffU << (8U - (unsigned)(entropy_len / 4));
    status = ((unsigned)(bits[entropy_len] ^ checksum[0]) & mask) == 0
        ? ZCL_OK : ZCL_INVALID_ENCODING;
cleanup:
    zcl_secure_zero(checksum, sizeof(checksum));
    return status;
}

zcl_status zcl_mnemonic_decode(const uint8_t *text, size_t text_len,
                              uint8_t *entropy, size_t entropy_capacity, size_t *entropy_len)
{
    if (text == NULL || entropy == NULL || entropy_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    uint8_t bits[33] = {0};
    size_t words = 0, size = 0;
    zcl_status status = zcl_words_read(text, text_len, bits, sizeof(bits), &words);
    if (status != ZCL_OK)
        goto cleanup;
    status = checked_entropy_size(words, &size);
    if (status != ZCL_OK)
        goto cleanup;
    status = verify_checksum(bits, sizeof(bits), size);
    if (status != ZCL_OK)
        goto cleanup;
    if (entropy_capacity < size) {
        status = ZCL_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    memcpy(entropy, bits, size);
    *entropy_len = size;
cleanup:
    zcl_secure_zero(bits, sizeof(bits));
    return status;
}
