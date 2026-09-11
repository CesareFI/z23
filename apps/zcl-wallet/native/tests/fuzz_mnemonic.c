/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void decode_arbitrary(const uint8_t *data, size_t size)
{
    uint8_t entropy[34] = {0}, before[34] = {0}, text[215] = {0};
    memset(entropy, 0xa5, sizeof(entropy));
    memcpy(before, entropy, sizeof(before));
    size_t length = 123, text_len = 0;
    zcl_status status = zcl_mnemonic_decode(data, size, entropy + 1, 32, &length);
    if (entropy[0] != 0xa5 || entropy[33] != 0xa5)
        abort();
    if (status != ZCL_OK) {
        if (length != 123 || memcmp(entropy, before, sizeof(entropy)) != 0)
            abort();
        return;
    }
    if (length < 16 || length > 32 || length % 4 != 0)
        abort();
    if (zcl_mnemonic_encode(entropy + 1, length, text, sizeof(text), &text_len) != ZCL_OK)
        abort();
    if (text_len != size || memcmp(text, data, size) != 0)
        abort();
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(text, sizeof(text));
}

static void round_trip(const uint8_t *data, size_t size)
{
    if (size < 16)
        return;
    size_t entropy_size = 16 + (((size - 16) / 4) % 5) * 4;
    uint8_t text[217] = {0}, entropy[32] = {0};
    size_t length = 0, restored = 0;
    memset(text, 0x5a, sizeof(text));
    if (zcl_mnemonic_encode(data, entropy_size, text + 1, 215, &length) != ZCL_OK)
        abort();
    if (text[0] != 0x5a || text[216] != 0x5a || length > 215)
        abort();
    if (zcl_mnemonic_decode(text + 1, length, entropy, sizeof(entropy), &restored) != ZCL_OK)
        abort();
    if (restored != entropy_size || memcmp(data, entropy, restored) != 0)
        abort();
    zcl_secure_zero(text, sizeof(text));
    zcl_secure_zero(entropy, sizeof(entropy));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    decode_arbitrary(data, size);
    round_trip(data, size);
    return 0;
}
