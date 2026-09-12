/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_qr.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 8 || size > ZCL_SCAN_INPUT_MAX + 8)
        return 0;
    zcl_qr_image layout = {
        (size_t)data[0] + (size_t)data[1] * 256,
        (size_t)data[2] + (size_t)data[3] * 256,
        (size_t)data[4] + (size_t)data[5] * 256,
        (size_t)data[6]
    };
    zcl_payment_request output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    const zcl_status status = zcl_scan_qr(data + 8, size - 8, &layout,
                                         (data[7] & 1) ? ZCL_MAINNET : ZCL_TESTNET, &output);
    if (status != ZCL_OK && memcmp(&before, &output, sizeof(output)) != 0)
        abort();
    if (status == ZCL_OK && output.address.network != ((data[7] & 1) ? ZCL_MAINNET : ZCL_TESTNET))
        abort();
    return 0;
}
