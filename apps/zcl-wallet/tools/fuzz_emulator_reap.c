/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "emulator_reap.h"
#include <stdio.h>
#include <stdlib.h>

static uintptr_t word(const uint8_t *bytes)
{
    uintptr_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uintptr_t)bytes[i] << (8u * i);
    return value;
}

static bool path_matches(const uint8_t *bytes, size_t length)
{
    const char suffix[] = "/libandroid-emu-metrics.so";
    if (bytes == NULL || length >= 4096 || length < sizeof(suffix) - 1) return false;
    for (size_t i = 0; i < length; ++i) if (bytes[i] == 0) return false;
    for (size_t i = 1; i < sizeof(suffix); ++i)
        if (bytes[length - i] != (uint8_t)suffix[sizeof(suffix) - 1 - i]) return false;
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    _Static_assert(sizeof(uintptr_t) == 8, "Qualified Linux x86-64 host");
    if (size < 17 || size > 4113) return 0;
    const uintptr_t base = word(data + 1);
    uintptr_t caller = word(data + 9);
    const uintptr_t offset = 0x52b40eu;
    if ((data[0] & 128u) != 0 && base <= UINTPTR_MAX - offset) caller = base + offset;
    size_t length = size - 17;
    if ((data[0] & 2u) != 0) length = SIZE_MAX;
    if ((data[0] & 4u) != 0) length = 4096;
    const uint8_t *module = (data[0] & 1u) != 0 ? NULL : data + 17;
    const bool expected = base != 0 && base <= UINTPTR_MAX - offset && caller == base + offset
        && path_matches(module, length);
    const bool actual = zcl_emulator_reap_site(caller, base, (const char *)module, length);
    if (actual != expected) { fputs("Emulator call-site reference mismatch\n", stderr); abort(); }
    return 0;
}
