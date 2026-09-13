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

static void delta_capacity(int64_t delta, uint8_t text[static 20], size_t length)
{
    for (size_t capacity = 0; capacity < length; ++capacity) {
        uint8_t before[20];
        memcpy(before, text, sizeof(before));
        size_t unchanged = SIZE_MAX;
        if (zcl_amount_delta_format(delta, text + 1, capacity, &unchanged) != ZCL_BUFFER_TOO_SMALL)
            abort();
        if (unchanged != SIZE_MAX || memcmp(text, before, sizeof(before)) != 0)
            abort();
    }
}

static void delta_round_trip(int64_t delta)
{
    uint8_t text[20];
    memset(text, 0xa5, sizeof(text));
    size_t length = SIZE_MAX;
    if (zcl_amount_delta_format(delta, text + 1, 18, &length) != ZCL_OK)
        abort();
    if (length == 0 || length > 18 || text[0] != 0xa5 || text[length + 1] != 0xa5 || text[19] != 0xa5)
        abort();
    size_t prefix = (size_t)(delta != 0);
    uint8_t sign = delta < 0 ? (uint8_t)'-' : (uint8_t)'+';
    if (prefix != 0 && text[1] != sign)
        abort();
    uint64_t recovered = UINT64_MAX;
    if (zcl_amount_parse(text + 1 + prefix, length - prefix, &recovered) != ZCL_OK)
        abort();
    uint64_t magnitude = delta < 0 ? (uint64_t)(-delta) : (uint64_t)delta;
    if (recovered != magnitude)
        abort();
    delta_capacity(delta, text, length);
}

static void arbitrary_delta(uint64_t bits)
{
    /* Convert all bit patterns without an implementation-defined unsigned cast. */
    int64_t candidate = (int64_t)(bits & (uint64_t)INT64_MAX);
    if ((bits >> 63) != 0)
        candidate = -candidate - 1;
    if (candidate >= -(int64_t)ZCL_MAX_MONEY && candidate <= (int64_t)ZCL_MAX_MONEY) {
        delta_round_trip(candidate);
        return;
    }
    uint8_t text[18] = {0};
    const uint8_t before[18] = {0};
    size_t unchanged = SIZE_MAX;
    if (zcl_amount_delta_format(candidate, text, sizeof(text), &unchanged) != ZCL_OUT_OF_RANGE)
        abort();
    if (unchanged != SIZE_MAX || memcmp(text, before, sizeof(text)) != 0)
        abort();
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
        uint64_t magnitude = candidate % (ZCL_MAX_MONEY + 1);
        round_trip(magnitude);
        delta_round_trip((int64_t)magnitude);
        delta_round_trip(-(int64_t)magnitude);
        arbitrary_delta(candidate);
    }
    return 0;
}
