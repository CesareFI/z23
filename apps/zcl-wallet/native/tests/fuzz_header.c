/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "header_fixture.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 || size > 1489) return 0;
    uint8_t wire[1488] = {0}, before[1488];
    const size_t length = size - 1;
    memcpy(wire, data + 1, length); memcpy(before, wire, sizeof(wire));
    const zcl_network network = (data[0] & 1) != 0 ? ZCL_MAINNET : ZCL_TESTNET;
    const uint32_t fork = network == ZCL_MAINNET ? 585318 : 6350;
    const uint32_t height = (data[0] & 2) != 0 ? fork : fork - 1;
    const size_t solution = (data[0] & 2) != 0 ? 400 : 1344;
    const bool valid = length == 143 + solution && wire[140] == 253 &&
        wire[141] == (uint8_t)(solution % 256) && wire[142] == (uint8_t)(solution / 256);
    zcl_header_view view, unchanged; memset(&view, 0xa5, sizeof(view)); memcpy(&unchanged, &view, sizeof(view));
    const zcl_status status = zcl_header_inspect(wire, length, network, height, &view);
    if ((status == ZCL_OK) != valid || memcmp(wire, before, sizeof(wire)) != 0) abort();
    if (valid) header_fixture_check(wire, length, &view);
    else if (memcmp(&view, &unchanged, sizeof(view)) != 0) abort();
    return 0;
}
