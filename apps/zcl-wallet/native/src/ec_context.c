/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "ec_context.h"

#include <secp256k1_preallocated.h>
#include <stdlib.h>
#include <string.h>

static zcl_status validate_context(const zcl_ec_context *context,
                                   const uint8_t *blinding, size_t blinding_len)
{
    if (context == NULL || blinding == NULL || blinding_len != 32)
        return ZCL_INVALID_ARGUMENT;
    if (context->handle != NULL || context->storage != NULL)
        return ZCL_INVALID_ARGUMENT;
    return ZCL_OK;
}

zcl_status zcl_ec_begin(zcl_ec_context *context, const uint8_t *blinding, size_t blinding_len)
{
    zcl_status status = validate_context(context, blinding, blinding_len);
    if (status != ZCL_OK)
        return status;
    size_t needed = secp256k1_context_preallocated_size(SECP256K1_CONTEXT_NONE);
    if (needed == 0 || needed > 1024)
        return ZCL_CRYPTO_FAILURE;
    /* No size multiplication: provider size is checked before allocation.
     * context owns storage; zcl_ec_end is the only freeing/cleanup path. */
    context->storage = malloc(needed); // raw-alloc-ok:standalone-c17-checked-context
    if (context->storage == NULL)
        return ZCL_RESOURCE_EXHAUSTED;
    context->storage_len = needed;
    context->handle = secp256k1_context_preallocated_create(context->storage, SECP256K1_CONTEXT_NONE);
    if (context->handle == NULL)
        return ZCL_CRYPTO_FAILURE;
    if (secp256k1_context_randomize(context->handle, blinding) != 1)
        return ZCL_CRYPTO_FAILURE;
    return ZCL_OK;
}

void zcl_ec_end(zcl_ec_context *context)
{
    if (context == NULL)
        return;
    if (context->handle != NULL)
        secp256k1_context_preallocated_destroy(context->handle);
    zcl_secure_zero(context->storage, context->storage_len);
    free(context->storage);
    zcl_secure_zero(context, sizeof(*context));
}

zcl_status zcl_ec_public(const secp256k1_context *context,
                        const uint8_t *secret, size_t secret_len,
                        uint8_t *public_key, size_t public_key_capacity)
{
    if (context == NULL || secret == NULL || public_key == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (secret_len != 32 || public_key_capacity < 33)
        return ZCL_OUT_OF_RANGE;
    secp256k1_pubkey point = {{0}};
    uint8_t encoded[33] = {0};
    size_t length = sizeof(encoded);
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (secp256k1_ec_pubkey_create(context, &point, secret) != 1)
        goto cleanup;
    if (secp256k1_ec_pubkey_serialize(context, encoded, &length, &point, SECP256K1_EC_COMPRESSED) != 1)
        goto cleanup;
    if (length != sizeof(encoded))
        goto cleanup;
    memcpy(public_key, encoded, sizeof(encoded));
    status = ZCL_OK;
cleanup:
    zcl_secure_zero(&point, sizeof(point));
    zcl_secure_zero(encoded, sizeof(encoded));
    return status;
}

zcl_status zcl_public_key(const uint8_t *secret, size_t secret_len,
                         const uint8_t *blinding, size_t blinding_len,
                         uint8_t *public_key, size_t public_key_capacity)
{
    zcl_ec_context context = {0};
    zcl_status status = zcl_ec_begin(&context, blinding, blinding_len);
    if (status == ZCL_OK)
        status = zcl_ec_public(context.handle, secret, secret_len, public_key, public_key_capacity);
    zcl_ec_end(&context);
    return status;
}
