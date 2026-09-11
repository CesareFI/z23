/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "secret_hash.h"

#include <mbedtls/platform_util.h>
#include <mbedtls/sha512.h>
#include <string.h>

void zcl_secure_zero(void *buffer, size_t buffer_length)
{
    if (buffer != NULL)
        mbedtls_platform_zeroize(buffer, buffer_length);
}

static zcl_status validate_hash_spans(const uint8_t *first, size_t first_len,
                                      const uint8_t *second, size_t second_len,
                                      const uint8_t *output, size_t output_capacity)
{
    if (first == NULL || second == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (first_len > 512 || second_len > 512 || output_capacity < 64)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status hash_pair(const uint8_t *first, size_t first_len,
                            const uint8_t *second, size_t second_len,
                            uint8_t *output, size_t output_capacity)
{
    zcl_status bounds = validate_hash_spans(first, first_len, second, second_len,
                                           output, output_capacity);
    if (bounds != ZCL_OK)
        return bounds;
    mbedtls_sha512_context context;
    uint8_t result[64] = {0};
    mbedtls_sha512_init(&context);
    int status = mbedtls_sha512_starts(&context, 0);
    if (status == 0)
        status = mbedtls_sha512_update(&context, first, first_len);
    if (status == 0)
        status = mbedtls_sha512_update(&context, second, second_len);
    if (status == 0)
        status = mbedtls_sha512_finish(&context, result);
    if (status == 0)
        memcpy(output, result, sizeof(result));
    mbedtls_sha512_free(&context);
    zcl_secure_zero(result, sizeof(result));
    return status == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

static zcl_status prepare_key(const uint8_t *key, size_t key_len,
                              uint8_t *block, size_t block_capacity)
{
    if (key == NULL || block == NULL || block_capacity < 128 || key_len > 256)
        return ZCL_INVALID_ARGUMENT;
    if (key_len > 128)
        return hash_pair(key, key_len, key, 0, block, block_capacity);
    memcpy(block, key, key_len);
    return ZCL_OK;
}

static zcl_status validate_hmac(const uint8_t *key, size_t key_len,
                                const uint8_t *data, size_t data_len,
                                const uint8_t *output, size_t output_capacity)
{
    if (key == NULL || data == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (key_len > 256 || data_len > 512)
        return ZCL_OUT_OF_RANGE;
    if (output_capacity < 64)
        return ZCL_BUFFER_TOO_SMALL;
    return ZCL_OK;
}

zcl_status zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                          const uint8_t *data, size_t data_len,
                          uint8_t *output, size_t output_capacity)
{
    zcl_status status = validate_hmac(key, key_len, data, data_len, output, output_capacity);
    if (status != ZCL_OK)
        return status;
    uint8_t block[128] = {0}, inner[64] = {0};
    status = prepare_key(key, key_len, block, sizeof(block));
    if (status != ZCL_OK)
        goto cleanup;
    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] ^= UINT8_C(0x36);
    status = hash_pair(block, sizeof(block), data, data_len, inner, sizeof(inner));
    if (status != ZCL_OK)
        goto cleanup;
    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] ^= UINT8_C(0x36) ^ UINT8_C(0x5c);
    status = hash_pair(block, sizeof(block), inner, sizeof(inner), output, output_capacity);
cleanup:
    zcl_secure_zero(block, sizeof(block));
    zcl_secure_zero(inner, sizeof(inner));
    return status;
}
