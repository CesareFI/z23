/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_storage_fixture.h"
#include <assert.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

static void exercise(const uint8_t bytes[static 246], size_t length)
{
    uint8_t before[246];
    memcpy(before, bytes, sizeof(before));
    assert(LLVMFuzzerTestOneInput(bytes, length) == 0);
    assert(memcmp(before, bytes, sizeof(before)) == 0);
}

static void authenticated_controls(void)
{
    change_storage_data data = {0};
    assert(change_data_init(&data) == 0);
    uint8_t bytes[246] = {4};
    memcpy(bytes + 6, data.state[0], 80);
    memcpy(bytes + 86, data.state[1], 80);
    static const uint8_t ignored[] = {0, 1, 254, 255};
    for (size_t i = 0; i < sizeof(ignored); ++i) {
        bytes[3] = ignored[i]; bytes[5] = ignored[i];
        exercise(bytes, 166);
    }
    /* These fields affect only reconstruction, not reservation. */
    bytes[4] = 255; exercise(bytes, 166); bytes[4] = 0;
    bytes[1] = 1; exercise(bytes, 166); bytes[1] = 0;
    bytes[2] = 255; exercise(bytes, 166); bytes[2] = 0;
    /* The success control must not mistake a damaged authentic head for valid. */
    bytes[165] ^= 1;
    exercise(bytes, 166);
}

int main(void)
{
    uint8_t bytes[246] = {0};
    for (size_t i = 1; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    static const size_t lengths[] = {6, 7, 16, 80, 160, 246};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        exercise(bytes, lengths[i]);
    for (unsigned bit = 0; bit < 8; ++bit) {
        bytes[0] = (uint8_t)(1U << bit);
        exercise(bytes, sizeof(bytes));
    }
    authenticated_controls();
    assert(LLVMFuzzerTestOneInput(NULL, 0) == 0);
    return puts("Change-address fuzz: live authentic controls and selector boundaries passed") == EOF ? 1 : 0;
}
