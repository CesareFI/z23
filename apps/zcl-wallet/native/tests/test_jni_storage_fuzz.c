/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

int main(void)
{
    /* The original crash truncated /tmp/... to /tm. Replay only through the
     * corrected harness, which admits no locator other than its own fixture. */
    uint8_t input[8] = {7, 0, 0, 3, 0, 250, 0, 0};
    if (LLVMFuzzerTestOneInput(input, sizeof(input)) != 0) return 1;
    for (unsigned selector = 0; selector <= UINT8_MAX; ++selector) {
        input[3] = (uint8_t)selector;
        if (LLVMFuzzerTestOneInput(input, sizeof(input)) != 0) return 1;
    }
    /* Also require a real paired write and each partial-array exception path. */
    for (uint8_t fault = 0; fault <= 6; ++fault) {
        const uint8_t valid[8] = {0, 0, 0, 0, 0, 0, fault, 0};
        if (LLVMFuzzerTestOneInput(valid, sizeof(valid)) != 0) return 1;
    }
    return 0;
}
