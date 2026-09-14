/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blake2_hash.h"
#include "zcl_keys.h"
#include <blake2.h>
#include <string.h>

_Static_assert(sizeof(blake2b_param) == 64 && offsetof(blake2b_param, personal) == 48,
               "BLAKE2b parameter bytes must use the specified wire layout");

static zcl_status validate_spans(const uint8_t *input, size_t input_len,
                                 const uint8_t *personal, size_t personal_len,
                                 const uint8_t *output, size_t output_capacity)
{
    if (input == NULL || personal == NULL || output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (input_len > ZCL_BLAKE2_INPUT_MAX || personal_len != ZCL_BLAKE2_PERSONAL_BYTES)
        return ZCL_OUT_OF_RANGE;
    if (output_capacity < ZCL_BLAKE2_DIGEST_BYTES)
        return ZCL_BUFFER_TOO_SMALL;
    return ZCL_OK;
}

zcl_status zcl_blake2b256(const uint8_t *input, size_t input_len,
                         const uint8_t *personal, size_t personal_len,
                         uint8_t *output, size_t output_capacity)
{
    const zcl_status bounds = validate_spans(input, input_len, personal, personal_len,
                                            output, output_capacity);
    if (bounds != ZCL_OK)
        return bounds;
    blake2b_param parameters;
    blake2b_state state;
    uint8_t digest[32] = {0};
    memset(&parameters, 0, sizeof(parameters));
    memset(&state, 0, sizeof(state));
    parameters.digest_length = 32;
    parameters.fanout = 1;
    parameters.depth = 1;
    memcpy(parameters.personal, personal, ZCL_BLAKE2_PERSONAL_BYTES);
    int result = blake2b_init_param(&state, &parameters);
    if (result == 0)
        result = blake2b_update(&state, input, input_len);
    if (result == 0)
        result = blake2b_final(&state, digest, sizeof(digest));
    if (result == 0)
        memcpy(output, digest, sizeof(digest));
    zcl_secure_zero(&state, sizeof(state));
    zcl_secure_zero(&parameters, sizeof(parameters));
    zcl_secure_zero(digest, sizeof(digest));
    return result == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
}
