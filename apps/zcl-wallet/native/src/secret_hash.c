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

typedef struct {
    mbedtls_sha512_context inner;
    mbedtls_sha512_context outer;
} prepared_hmac;

static int start_pad(mbedtls_sha512_context *context, const uint8_t block[128])
{
    int status = mbedtls_sha512_starts(context, 0);
    if (status == 0)
        status = mbedtls_sha512_update(context, block, 128);
    return status;
}

/* Both provider contexts are initialized even when key preparation fails;
 * the caller always frees them. They never escape their owning C call. */
static zcl_status prepare_hmac(prepared_hmac *prepared,
                               const uint8_t *key, size_t key_len)
{
    uint8_t block[128] = {0};
    mbedtls_sha512_init(&prepared->inner);
    mbedtls_sha512_init(&prepared->outer);
    zcl_status status = prepare_key(key, key_len, block, sizeof(block));
    if (status != ZCL_OK)
        goto cleanup;
    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] ^= UINT8_C(0x36);
    status = ZCL_CRYPTO_FAILURE;
    if (start_pad(&prepared->inner, block) != 0)
        goto cleanup;
    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] ^= UINT8_C(0x36) ^ UINT8_C(0x5c);
    if (start_pad(&prepared->outer, block) == 0)
        status = ZCL_OK;
cleanup:
    zcl_secure_zero(block, sizeof(block));
    return status;
}

static void free_hmac(prepared_hmac *prepared)
{
    mbedtls_sha512_free(&prepared->inner);
    mbedtls_sha512_free(&prepared->outer);
}

/* Only called after successful preparation and span validation. Cloning the
 * pad states leaves them reusable; digest/working state is cleared each round.
 * A provider may modify its output before failing: publish only on success. */
static zcl_status hmac_digest(const prepared_hmac *prepared,
                              const uint8_t *data, size_t data_len,
                              uint8_t output[64])
{
    mbedtls_sha512_context working;
    uint8_t digest[64] = {0};
    mbedtls_sha512_init(&working);
    mbedtls_sha512_clone(&working, &prepared->inner);
    int status = mbedtls_sha512_update(&working, data, data_len);
    if (status == 0)
        status = mbedtls_sha512_finish(&working, digest);
    if (status == 0) {
        mbedtls_sha512_clone(&working, &prepared->outer);
        status = mbedtls_sha512_update(&working, digest, sizeof(digest));
    }
    if (status == 0)
        status = mbedtls_sha512_finish(&working, digest);
    if (status == 0)
        memcpy(output, digest, sizeof(digest));
    mbedtls_sha512_free(&working);
    zcl_secure_zero(digest, sizeof(digest));
    return status == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}

zcl_status zcl_hmac_sha512(const uint8_t *key, size_t key_len,
                          const uint8_t *data, size_t data_len,
                          uint8_t *output, size_t output_capacity)
{
    zcl_status status = validate_hmac(key, key_len, data, data_len, output, output_capacity);
    if (status != ZCL_OK)
        return status;
    prepared_hmac prepared;
    status = prepare_hmac(&prepared, key, key_len);
    if (status == ZCL_OK)
        status = hmac_digest(&prepared, data, data_len, output);
    free_hmac(&prepared);
    return status;
}

zcl_status zcl_pbkdf2_sha512_block(const uint8_t *key, size_t key_len,
                                  const uint8_t *salt, size_t salt_len,
                                  uint8_t *output, size_t output_capacity)
{
    zcl_status status = validate_hmac(key, key_len, salt, salt_len, output, output_capacity);
    if (status != ZCL_OK)
        return status;
    prepared_hmac prepared;
    uint8_t current[64] = {0}, next[64] = {0}, result[64] = {0};
    status = prepare_hmac(&prepared, key, key_len);
    if (status != ZCL_OK)
        goto cleanup;
    status = hmac_digest(&prepared, salt, salt_len, current);
    if (status != ZCL_OK)
        goto cleanup;
    memcpy(result, current, sizeof(result));
    for (size_t iteration = 1; iteration < 2048; ++iteration) {
        status = hmac_digest(&prepared, current, sizeof(current), next);
        if (status != ZCL_OK)
            goto cleanup;
        for (size_t i = 0; i < sizeof(result); ++i)
            result[i] ^= next[i];
        memcpy(current, next, sizeof(current));
    }
    memcpy(output, result, sizeof(result));
cleanup:
    free_hmac(&prepared);
    zcl_secure_zero(current, sizeof(current));
    zcl_secure_zero(next, sizeof(next));
    zcl_secure_zero(result, sizeof(result));
    return status;
}
