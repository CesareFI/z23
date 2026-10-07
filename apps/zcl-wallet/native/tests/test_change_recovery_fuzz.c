/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

static void exercise(const uint8_t bytes[static 246], size_t length)
{
    uint8_t before[246];
    memcpy(before, bytes, sizeof(before));
    assert(LLVMFuzzerTestOneInput(bytes, length) == 0);
    assert(memcmp(before, bytes, sizeof(before)) == 0);
}

int main(void)
{
    uint8_t bytes[246] = {0};
    for (size_t index = 1; index < sizeof(bytes); ++index)
        bytes[index] = (uint8_t)(index & 255U);
    /* Nonzero ignored fields prove the success control depends on selectors,
     * not on the whole packet coincidentally being zero. */
    static const size_t lengths[] = {6, 7, 16, 80, 160, 246};
    for (size_t index = 0; index < sizeof(lengths) / sizeof(lengths[0]); ++index)
        exercise(bytes, lengths[index]);
    for (unsigned bit = 0; bit < 8; ++bit) {
        bytes[0] = (uint8_t)(1U << bit);
        exercise(bytes, sizeof(bytes));
    }
    assert(LLVMFuzzerTestOneInput(NULL, 0) == 0);
    return puts("Change recovery fuzz: six live control lengths and eight selector variations passed") == EOF ? 1 : 0;
}
