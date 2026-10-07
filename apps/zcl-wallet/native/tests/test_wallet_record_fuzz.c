/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

int main(void)
{
    int argc = 0;
    char **argv = NULL;
    if (LLVMFuzzerInitialize(&argc, &argv) != 0) return 1;
    /* Public zero-XOR data preserves the harness's valid header at80 bytes,
     * alongside every truncation and lengths beyond either record bound. */
    const uint8_t input[142] = {0};
    for (size_t length = 0; length <= sizeof(input); ++length)
        if (LLVMFuzzerTestOneInput(input, length) != 0) return 1;
    return 0;
}
