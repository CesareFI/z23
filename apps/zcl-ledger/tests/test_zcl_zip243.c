/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_zip243.h"
#include "crypto/blake2b.h"
#include "zcl_zip243_host.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static size_t read_vector(const char *path, uint8_t wire[8192]) {
    FILE *file = fopen(path, "r");
    assert(file);
    static char line[16384];
    while (fgets(line, sizeof line, file) && line[0] == '#') {}
    assert(!ferror(file) && line[0] != '#');
    size_t length = strcspn(line, "\r\n");
    assert(length % 2 == 0 && length > 0 && length <= 16384);
    for (size_t i = 0; i < length / 2; ++i) {
        int hi = nibble(line[2 * i]), lo = nibble(line[2 * i + 1]);
        assert(hi >= 0 && lo >= 0);
        wire[i] = (uint8_t)((hi << 4) | lo);
    }
    assert(fclose(file) == 0);
    return length / 2;
}

int main(int argc, char **argv) {
    assert(argc == 3);
    static uint8_t wire[8192];
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    size_t length = read_vector(argv[1], wire);
    assert(length == 4118);
    static const uint8_t expected[32] = {
        0x63, 0xd1, 0x85, 0x34, 0xde, 0x5f, 0x2d, 0x1c,
        0x9e, 0x16, 0x9b, 0x73, 0xf9, 0xc7, 0x83, 0x71,
        0x8a, 0xdb, 0xef, 0x5c, 0x8a, 0x7d, 0x55, 0xb5,
        0xe7, 0xa3, 0x7a, 0xff, 0xa1, 0xdd, 0x3f, 0xf3
    };
    uint8_t digest[32];
    assert(zcl_zip243_shielded_digest(wire, length, 0x76b809bb,
                                      &hasher, digest) == 0);
    assert(memcmp(digest, expected, 32) == 0);
    wire[length - 1] ^= 1;
    assert(zcl_zip243_shielded_digest(wire, length, 0x76b809bb,
                                      &hasher, digest) == 0);
    assert(memcmp(digest, expected, 32) == 0);
    wire[20] ^= 1;
    assert(zcl_zip243_shielded_digest(wire, length, 0x76b809bb,
                                      &hasher, digest) == 0);
    assert(memcmp(digest, expected, 32) != 0);
    wire[20] ^= 1;
    assert(zcl_zip243_shielded_digest(wire, length, 0x930b540d,
                                      &hasher, digest) == 0);
    assert(memcmp(digest, expected, 32) != 0);
    assert(zcl_zip243_shielded_digest(wire, length - 1, 0x76b809bb,
                                      &hasher, digest) < 0);
    length = read_vector(argv[2], wire);
    assert(length == 245);
    static const uint8_t script_code[25] = {
        0x76, 0xa9, 0x14, 0x50, 0x71, 0x73, 0x52, 0x7b,
        0x4c, 0x33, 0x18, 0xa2, 0xae, 0xcd, 0x79, 0x3b,
        0xf1, 0xcf, 0xed, 0x70, 0x59, 0x50, 0xcf, 0x88,
        0xac
    };
    static const uint8_t transparent_expected[32] = {
        0xf3, 0x14, 0x8f, 0x80, 0xdf, 0xab, 0x5e, 0x57,
        0x3d, 0x5e, 0xdf, 0xe7, 0xa8, 0x50, 0xf5, 0xfd,
        0x39, 0x23, 0x4f, 0x80, 0xb5, 0x42, 0x9d, 0x3a,
        0x57, 0xed, 0xcc, 0x11, 0xe3, 0x4c, 0x58, 0x5b
    };
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         script_code, sizeof script_code,
                                         50000000, 0x76b809bb,
                                         &hasher, digest) == 0);
    assert(memcmp(digest, transparent_expected, 32) == 0);
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         script_code, sizeof script_code,
                                         50000001, 0x76b809bb,
                                         &hasher, digest) == 0);
    assert(memcmp(digest, transparent_expected, 32) != 0);
    uint8_t changed_script[sizeof script_code];
    memcpy(changed_script, script_code, sizeof changed_script);
    changed_script[3] ^= 1;
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         changed_script, sizeof changed_script,
                                         50000000, 0x76b809bb,
                                         &hasher, digest) == 0);
    assert(memcmp(digest, transparent_expected, 32) != 0);
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         script_code, sizeof script_code,
                                         50000000, 0x930b540d,
                                         &hasher, digest) == 0);
    assert(memcmp(digest, transparent_expected, 32) != 0);
    assert(zcl_zip243_transparent_digest(wire, length, 1,
                                         script_code, sizeof script_code,
                                         50000000, 0x76b809bb,
                                         &hasher, digest) < 0);
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         script_code, sizeof script_code,
                                         2100000000000001ULL, 0x76b809bb,
                                         &hasher, digest) < 0);
    assert(zcl_zip243_transparent_digest(wire, length, 0,
                                         NULL, sizeof script_code,
                                         50000000, 0x76b809bb,
                                         &hasher, digest) < 0);
    return 0;
}
