/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include "bip32_internal.h"
#include <string.h>

typedef struct {
    zcl_ec_context context;
    uint8_t seed[64], anchor[35], candidate[35];
    size_t length;
} change_work;

static zcl_status change_arguments(const uint8_t *header, const uint8_t *entropy,
                                   uint32_t index, const uint8_t *blinding,
                                   size_t blinding_len, const uint8_t *address, size_t capacity)
{
    if (header == NULL || entropy == NULL || blinding == NULL || address == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (blinding_len != ZCL_WALLET_CHANGE_BLINDING_BYTES)
        return ZCL_INVALID_ARGUMENT;
    if (index >= UINT32_C(0x80000000))
        return ZCL_OUT_OF_RANGE;
    return capacity < 35 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
}

static zcl_status recover_seed(change_work *work, const zcl_wallet_info *info,
    const uint8_t *entropy, size_t entropy_len)
{
    zcl_status status = zcl_entropy_seed(entropy, entropy_len, work->seed, sizeof(work->seed));
    if (status == ZCL_OK)
        status = zcl_seed_address(work->seed, sizeof(work->seed), info->network, 0, 0,
            work->context.handle, work->anchor, sizeof(work->anchor), &work->length);
    if (status != ZCL_OK) return status;
    return work->length == sizeof(work->anchor) && memcmp(work->anchor, info->address, sizeof(work->anchor)) == 0
        ? ZCL_OK : ZCL_INVALID_ENCODING;
}

static zcl_status recovered_change(const zcl_wallet_info *info, const uint8_t *entropy, size_t entropy_len,
    uint32_t index, const uint8_t blinding[64], uint8_t address[35])
{
    change_work work;
    memset(&work, 0, sizeof(work));
    zcl_status status = zcl_ec_begin(&work.context, blinding, 32);
    if (status == ZCL_OK) status = recover_seed(&work, info, entropy, entropy_len);
    /* The binding decision is complete. Change derivation needs only seed. */
    zcl_secure_zero(work.anchor, sizeof(work.anchor));
    if (status == ZCL_OK) {
        /* The matched anchor ends its context before change begins. Preserve
         * independent blinding and one live context while reusing only seed. */
        zcl_ec_end(&work.context);
        status = zcl_ec_begin(&work.context, blinding + 32, 32);
        if (status == ZCL_OK)
            status = zcl_seed_address(work.seed, sizeof(work.seed), info->network, 1, index,
                work.context.handle, work.candidate, sizeof(work.candidate), &work.length);
    }
    /* No subsequent output check, publication or context release needs seed. */
    zcl_secure_zero(work.seed, sizeof(work.seed));
    if (status == ZCL_OK && work.length != sizeof(work.candidate)) status = ZCL_CRYPTO_FAILURE;
    if (status == ZCL_OK) memcpy(address, work.candidate, sizeof(work.candidate));
    zcl_ec_end(&work.context);
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_wallet_recovered_change(const uint8_t *header, size_t header_len,
                                      const uint8_t *entropy, size_t entropy_len,
                                      uint32_t index, const uint8_t *blinding, size_t blinding_len,
                                      uint8_t *address, size_t capacity)
{
    zcl_status status = change_arguments(header, entropy, index, blinding, blinding_len, address, capacity);
    if (status != ZCL_OK) return status;
    zcl_wallet_info info = {0};
    status = zcl_wallet_header_parse(header, header_len, &info);
    if (status != ZCL_OK) return status;
    if (entropy_len != info.entropy_len) return ZCL_INVALID_ENCODING;
    return recovered_change(&info, entropy, entropy_len, index, blinding, address);
}
