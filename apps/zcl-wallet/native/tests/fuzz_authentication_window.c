/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_custody.h"
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static uint64_t number(const uint8_t *bytes)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) value |= (uint64_t)bytes[i] << (i * 8);
    return value;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size != 24) return 0;
    const uint64_t start = number(data), now = number(data + 8);
    const uint64_t shift = number(data + 16);
    const zcl_status status = zcl_authentication_window_check(start, now);
    /* Metamorphic properties: moving both timestamps within uint64 bounds
     * must preserve the verdict; an expired forward window cannot reopen. */
    if (start <= UINT64_MAX - shift && now <= UINT64_MAX - shift &&
        zcl_authentication_window_check(start + shift, now + shift) != status) abort();
    if (start >= shift && now >= shift &&
        zcl_authentication_window_check(start - shift, now - shift) != status) abort();
    if (zcl_authentication_window_check(start, start) != ZCL_OK) abort();
    if (now >= start && status != ZCL_OK &&
        zcl_authentication_window_check(start, UINT64_MAX) == ZCL_OK) abort();
    return 0;
}
