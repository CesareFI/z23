/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "mnemonic_words.h"

zcl_status zcl_mnemonic_confirm(const uint8_t *entropy, size_t entropy_len,
                               const uint8_t *text, size_t text_len)
{
    if (entropy == NULL || text == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!zcl_entropy_length_valid(entropy_len))
        return ZCL_OUT_OF_RANGE;
    uint8_t recovered[32] = {0};
    size_t length = 0;
    volatile uint8_t difference = 0;
    zcl_status status = zcl_mnemonic_decode(text, text_len, recovered, sizeof(recovered), &length);
    if (status != ZCL_OK)
        goto cleanup;
    status = ZCL_INVALID_ENCODING;
    if (length != entropy_len)
        goto cleanup;
    /* Volatile accumulation prevents replacing the loop with an early-exit
     * comparison. Length is public (the backup's word count). */
    for (size_t i = 0; i < entropy_len; ++i)
        difference = (uint8_t)(difference | (uint8_t)(entropy[i] ^ recovered[i]));
    if (difference == 0)
        status = ZCL_OK;
cleanup:
    difference = 0;
    zcl_secure_zero(recovered, sizeof(recovered));
    return status;
}
