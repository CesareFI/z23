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

static void setup_remaining(uint64_t start, uint64_t now)
{
    uint64_t remaining = UINT64_MAX;
    const zcl_status status = zcl_setup_window_remaining(start, now, &remaining);
    if (now < start) {
        if (status != ZCL_IO_UNCERTAIN || remaining != UINT64_MAX) abort();
    } else if (now - start >= UINT64_C(600000)) {
        if (status != ZCL_TIMED_OUT || remaining != UINT64_MAX) abort();
    } else if (status != ZCL_OK || remaining != UINT64_C(600000) - (now - start)) abort();
}

static void setup_translations(uint64_t start, uint64_t now, uint64_t shift)
{
    setup_remaining(start, now);
    setup_remaining(start, start);
    if (start <= UINT64_MAX - shift && now <= UINT64_MAX - shift)
        setup_remaining(start + shift, now + shift);
    if (start >= shift && now >= shift) setup_remaining(start - shift, now - shift);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size != 24) return 0;
    const uint64_t start = number(data), now = number(data + 8);
    const uint64_t shift = number(data + 16);
    setup_translations(start, now, shift);
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
