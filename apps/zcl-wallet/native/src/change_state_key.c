/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_state_internal.h"
#include <string.h>

static zcl_status expand_key(const uint8_t *header, const uint8_t *entropy, size_t entropy_len,
                              uint8_t *key, size_t capacity)
{
    static const uint8_t salt[] = "Zclassic Android change state v1";
    static const uint8_t label[] = "authenticated index record";
    uint8_t info[sizeof(label) - 1 + ZCL_WALLET_HEADER_BYTES + 1] = {0};
    uint8_t extracted[64] = {0};
    memcpy(info, label, sizeof(label) - 1);
    memcpy(info + sizeof(label) - 1, header, ZCL_WALLET_HEADER_BYTES);
    info[sizeof(info) - 1] = 1; /* RFC5869 T(1), exactly one SHA512 output block. */
    zcl_status status = zcl_hmac_sha512(salt, sizeof(salt) - 1, entropy, entropy_len,
        extracted, sizeof(extracted));
    if (status == ZCL_OK)
        status = zcl_hmac_sha512(extracted, sizeof(extracted), info, sizeof(info), key, capacity);
    zcl_secure_zero(extracted, sizeof(extracted));
    return status;
}

zcl_status zcl_change_state_key(const uint8_t *header, size_t header_len,
    const uint8_t *entropy, size_t entropy_len, const uint8_t *blinding, size_t blinding_len,
    uint8_t *key, size_t capacity)
{
    if (key == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity < 64) return ZCL_BUFFER_TOO_SMALL;
    uint8_t anchor[35] = {0};
    const zcl_status status = zcl_wallet_recovered_address(header, header_len, entropy, entropy_len,
        blinding, blinding_len, anchor, sizeof(anchor));
    zcl_secure_zero(anchor, sizeof(anchor));
    if (status != ZCL_OK) return status;
    return expand_key(header, entropy, entropy_len, key, capacity);
}
