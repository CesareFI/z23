/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Deterministic public fuzz inputs; optionally write to a fresh owned corpus
 * directory. Exclusive creation refuses overwriting an existing corpus file. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int exercise(const uint8_t *data, size_t size, const char *directory, size_t number)
{
    if (LLVMFuzzerTestOneInput(data, size) != 0) return 0;
    if (directory == NULL) return 1;
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/bip32-%zu", directory, number);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    FILE *file = fopen(path, "wbx");
    if (file == NULL) return 0;
    int okay = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int scalar_boundaries(const char *directory, size_t *number)
{
    static const uint8_t order[32] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41};
    for (unsigned scalar = 0; scalar < 4; ++scalar) {
        for (unsigned index = 0; index < 2; ++index) {
            uint8_t data[104] = {64, 0, 0, 0};
            data[4] = index == 0 ? 0 : 128;
            if (scalar == 0) data[39] = 1;
            else { memcpy(data + 8, order, sizeof(order)); data[39] = (uint8_t)(0x3fU + scalar); }
            if (!exercise(data, sizeof(data), directory, (*number)++)) return 0;
        }
    }
    return 1;
}

static int cases(const char *directory)
{
    static const uint32_t indices[] = {0, 1, UINT32_C(0x7fffffff), UINT32_C(0x80000000), UINT32_MAX};
    uint8_t data[104];
    size_t number = 0;
    for (unsigned length = 0; length < 68; ++length) {
        for (size_t profile = 0; profile < sizeof(indices) / sizeof(indices[0]); ++profile) {
            for (size_t i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)(i * 13 + length * 37);
            data[0] = length == 67 ? 255 : (uint8_t)length;
            data[1] = 0; data[2] = (uint8_t)profile; data[3] = 0;
            for (size_t i = 0; i < 4; ++i) data[4 + i] = (uint8_t)(indices[profile] >> (24U - (unsigned)i * 8U));
            if (!exercise(data, sizeof(data), directory, number++)) return 0;
        }
    }
    /* Decoupled control bytes allow invalid scalar bytes to reach the oracle. */
    for (unsigned control = 0; control < 256; ++control) {
        memset(data, 0, sizeof(data));
        data[0] = 64; data[1] = (uint8_t)control; data[2] = (uint8_t)control; data[3] = (uint8_t)control;
        if (!exercise(data, sizeof(data), directory, number++)) return 0;
        memset(data + 8, 0xff, 64);
        if (!exercise(data, sizeof(data), directory, number++)) return 0;
    }
    return scalar_boundaries(directory, &number) && number == 860;
}

int main(int argc, char **argv)
{
    if (argc > 2 || !cases(argc == 2 ? argv[1] : NULL)) {
        fprintf(stderr, "BIP32 differential corpus generation/replay failed\n");
        return 1;
    }
    if (puts("BIP32 differential replay: 860 public seed/path/scalar/argument cases passed") < 0) return 1;
    return 0;
}
