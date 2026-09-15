/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Public deterministic boundary inputs, optionally saved with exclusive create. */
#include <stdint.h>
#include <stdio.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int exercise(const uint8_t *data, size_t size, const char *directory, size_t number)
{
    if (LLVMFuzzerTestOneInput(data, size) != 0) return 0;
    if (directory == NULL) return 1;
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/hmac-%zu", directory, number);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    FILE *file = fopen(path, "wbx");
    if (file == NULL) return 0;
    int okay = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int cases(const char *directory)
{
    static const size_t keys[] = {0, 1, 63, 64, 127, 128, 129, 255, 256};
    static const size_t messages[] = {0, 1, 63, 64, 111, 112, 127, 128, 129, 255, 256, 511, 512};
    uint8_t data[774];
    size_t number = 0;
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); ++k) {
        for (size_t m = 0; m < sizeof(messages) / sizeof(messages[0]); ++m) {
            for (unsigned pattern = 0; pattern < 2; ++pattern) {
                for (size_t i = 0; i < sizeof(data); ++i)
                    data[i] = pattern == 0 ? 0 : (uint8_t)((i * 37 + number) & 255);
                data[0] = (uint8_t)(keys[k] & 255); data[1] = (uint8_t)(keys[k] >> 8);
                data[2] = (uint8_t)(messages[m] & 255); data[3] = (uint8_t)(messages[m] >> 8);
                data[4] = (uint8_t)(number % 64); data[5] = (uint8_t)(number % 10);
                if (!exercise(data, sizeof(data), directory, number++)) return 0;
            }
        }
    }
    for (unsigned capacity = 0; capacity < 64; ++capacity) {
        data[4] = (uint8_t)capacity; data[5] = 9;
        if (!exercise(data, sizeof(data), directory, number++)) return 0;
    }
    return number == 298;
}

int main(int argc, char **argv)
{
    if (argc > 2 || !cases(argc == 2 ? argv[1] : NULL)) {
        fprintf(stderr, "HMAC differential corpus generation/replay failed\n");
        return 1;
    }
    return puts("HMAC differential replay: 298 public boundary/refusal cases passed") < 0 ? 1 : 0;
}
