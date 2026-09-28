/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_sapling_ock.h"
#include "zcl_zip243_host.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { WIRE_BYTES = 1425, OUTPUT_OFFSET = 412 };

static int hex_digit(int ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static void read_fixture(const char *path, uint8_t wire[WIRE_BYTES]) {
    FILE *file = fopen(path, "r");
    assert(file);
    char line[WIRE_BYTES * 2 + 2];
    do {
        assert(fgets(line, sizeof line, file));
    } while (line[0] == '#');
    assert(strlen(line) == WIRE_BYTES * 2 + 1);
    for (size_t i = 0; i < WIRE_BYTES; ++i) {
        int high = hex_digit(line[i * 2]);
        int low = hex_digit(line[i * 2 + 1]);
        assert(high >= 0 && low >= 0);
        wire[i] = (uint8_t)((high << 4) | low);
    }
    assert(line[WIRE_BYTES * 2] == '\n');
    assert(fclose(file) == 0);
}

static bool fail_final(void *context, uint8_t digest[32]) {
    (void)context;
    memset(digest, 0xa5, 32);
    return false;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    static const uint8_t ovk[32] = {
        0xce,0x03,0x88,0x0a,0x87,0x50,0x48,0xf6,
        0x17,0x8e,0xbd,0x84,0x2e,0xdb,0xcb,0xd2,
        0x26,0xee,0x7f,0xe5,0x48,0x9d,0x64,0x8a,
        0xec,0x56,0xea,0x8f,0xba,0xf5,0x0f,0x17
    };
    static const uint8_t expected[32] = {
        0xd9,0x53,0x84,0xf8,0x1b,0x92,0xc9,0x07,
        0xc1,0x32,0x79,0x86,0xcb,0x66,0xb7,0x65,
        0x42,0x83,0x73,0x72,0x59,0x5e,0xa0,0x5f,
        0xff,0xec,0x20,0x6d,0x4d,0x44,0x40,0xbb
    };
    uint8_t wire[WIRE_BYTES], key[32];
    read_fixture(argv[1], wire);
    const uint8_t *cv = wire + OUTPUT_OFFSET;
    const uint8_t *cm = cv + 32;
    const uint8_t *epk = cm + 32;
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    assert(blue_sapling_ock(key, ovk, cv, cm, epk, &hasher));
    assert(memcmp(key, expected, sizeof key) == 0);
    uint8_t changed[32];
    memcpy(changed, cv, sizeof changed);
    changed[0] ^= 1u;
    assert(blue_sapling_ock(key, ovk, changed, cm, epk, &hasher));
    assert(memcmp(key, expected, sizeof key) != 0);
    hasher.final = fail_final;
    assert(!blue_sapling_ock(key, ovk, cv, cm, epk, &hasher));
    for (unsigned i = 0; i < sizeof key; ++i) assert(key[i] == 0);
    assert(!blue_sapling_ock(key, ovk, cv, cm, epk, NULL));
    for (unsigned i = 0; i < sizeof key; ++i) assert(key[i] == 0);
    memset(&context, 0, sizeof context);
    puts("Blue Sapling outgoing key: passed");
    return 0;
}
