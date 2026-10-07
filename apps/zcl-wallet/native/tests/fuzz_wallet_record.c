/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static uint8_t fixture[140];
static size_t fixture_len;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    uint8_t entropy[16] = {0}, blinding[32] = {1}, header[80] = {0};
    uint8_t iv[12] = {0}, ciphertext[32] = {0};
    if (zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_TESTNET, blinding, sizeof(blinding),
                                 header, sizeof(header)) != ZCL_OK)
        abort();
    if (zcl_wallet_record_pack(header, sizeof(header), iv, sizeof(iv), ciphertext, sizeof(ciphertext),
                               fixture, sizeof(fixture), &fixture_len) != ZCL_OK)
        abort();
    return 0;
}

static void check_record(const uint8_t *data, size_t size)
{
    zcl_wallet_record result, before;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&before, &result, sizeof(before));
    zcl_status status = zcl_wallet_record_parse(data, size, &result);
    if (status != ZCL_OK) {
        if (memcmp(&result, &before, sizeof(result)) != 0)
            abort();
        return;
    }
    uint8_t packed[142];
    memset(packed, 0xa5, sizeof(packed));
    size_t packed_len = 0;
    if (zcl_wallet_record_pack(result.header, sizeof(result.header), result.iv, sizeof(result.iv),
                               result.ciphertext, result.ciphertext_len, packed + 1, 140, &packed_len) != ZCL_OK)
        abort();
    if (packed_len != size || memcmp(packed + 1, data, size) != 0 || packed[0] != 0xa5 || packed[141] != 0xa5)
        abort();
}

static void check_header(const uint8_t *data, size_t size)
{
    struct { uint64_t before; zcl_wallet_info info; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_wallet_info previous;
    memcpy(&previous, &box.info, sizeof(previous));
    const zcl_status status = zcl_wallet_header_parse(data, size, &box.info);
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (status != ZCL_OK) {
        if (memcmp(&previous, &box.info, sizeof(previous)) != 0) {
            fputs("Wallet header output changed after refusal\n", stderr);
            abort();
        }
        return;
    }
    /* Metadata parsing is not authentication. Check its exact bounded copy
     * contract, with the length guard before any source-field access. */
    if (size != ZCL_WALLET_HEADER_BYTES || box.info.entropy_len != data[7] ||
        box.info.network != (zcl_network)data[5] ||
        memcmp(box.info.address, data + 44, sizeof(box.info.address)) != 0) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    check_record(data, size);
    check_header(data, size);
    if (size > sizeof(fixture))
        return 0;
    uint8_t changed[140] = {0};
    memcpy(changed, fixture, sizeof(changed));
    for (size_t i = 0; i < size; ++i)
        changed[i] ^= data[i];
    check_record(changed, fixture_len);
    check_header(changed, size);
    return 0;
}
