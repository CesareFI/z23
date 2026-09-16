/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int exercise(const uint8_t *data, size_t size, const char *directory, size_t number)
{
    if (LLVMFuzzerTestOneInput(data, size) != 0) return 0;
    if (directory == NULL) return 1;
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/framing-%zu", directory, number);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    FILE *file = fopen(path, "wbx");
    if (file == NULL) return 0;
    int okay = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int cases(const char *directory)
{
    /* Fixed host scratch: no large stack frame, allocation or retained input. */
    static uint8_t bytes[ZCL_ELECTRUM_FRAME_MAX + 1];
    static const size_t small[] = {0, 1, 2, 255, 256, 257};
    size_t number = 0;
    for (unsigned byte = 0; byte <= 255; ++byte) {
        memset(bytes, (int)byte, sizeof(bytes));
        for (size_t i = 0; i < sizeof(small) / sizeof(small[0]); ++i)
            if (!exercise(bytes, small[i], directory, number++)) return 0;
    }
    static const uint8_t chunks[] = {0, 1, 9, 10, 31, 127, 255};
    for (size_t chunk = 0; chunk < sizeof(chunks); ++chunk) {
        for (size_t length = ZCL_ELECTRUM_FRAME_MAX - 1; length <= sizeof(bytes); ++length) {
            for (unsigned newline = 0; newline < 3; ++newline) {
                memset(bytes, 'A', sizeof(bytes));
                bytes[0] = chunks[chunk];
                if (newline == 1) bytes[length - 1] = '\n';
                if (newline == 2) bytes[length / 2] = '\n';
                if (!exercise(bytes, length, directory, number++)) return 0;
            }
        }
    }
    static const uint8_t mixed[] = "\n{}\r\n{\"id\":1,\"result\":[\"fixture\",\"1.2\"]}\n\0tail\nremaining";
    if (!exercise(mixed, sizeof(mixed) - 1, directory, number++)) return 0;
    return number == 1600;
}

int main(int argc, char **argv)
{
    if (argc > 2 || !cases(argc == 2 ? argv[1] : NULL)) {
        fprintf(stderr, "Electrum framing corpus replay/generation failed\n");
        return 1;
    }
    return puts("Electrum framing: 1600 byte/chunk/boundary cases, immutable terminal states and exact reset passed") < 0 ? 1 : 0;
}
