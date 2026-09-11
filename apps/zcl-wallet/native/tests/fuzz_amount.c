/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* abort is confined to this host test harness and reports a broken invariant. */
static void round_trip(uint64_t amount)
{
    uint8_t text[19];
    memset(text, 0xa5, sizeof(text));
    size_t length = SIZE_MAX;
    if (zcl_amount_format(amount, text + 1, 17, &length) != ZCL_OK)
        abort();
    if (length == 0 || length > 17 || text[0] != 0xa5 || text[18] != 0xa5)
        abort();
    uint64_t recovered = UINT64_MAX;
    if (zcl_amount_parse(text + 1, length, &recovered) != ZCL_OK || recovered != amount)
        abort();
    for (size_t capacity = 0; capacity < length; ++capacity) {
        size_t unchanged = SIZE_MAX;
        if (zcl_amount_format(amount, text + 1, capacity, &unchanged) != ZCL_BUFFER_TOO_SMALL)
            abort();
        if (unchanged != SIZE_MAX)
            abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint64_t amount = UINT64_MAX;
    zcl_status status = zcl_amount_parse(data, size, &amount);
    if (status == ZCL_OK) {
        if (amount > ZCL_MAX_MONEY)
            abort();
        round_trip(amount);
    } else if (amount != UINT64_MAX) {
        abort();
    }
    if (size >= 8) {
        uint64_t candidate = 0;
        for (size_t index = 0; index < 8; ++index)
            candidate = (candidate << 8) | (uint64_t)data[index];
        round_trip(candidate % (ZCL_MAX_MONEY + 1));
    }
    return 0;
}
