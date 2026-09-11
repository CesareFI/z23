/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "mnemonic check failed at line %d\n", __LINE__); return 1; } } while (0)

/* Published fixture secrets only. Never use these values for funds. */
typedef struct {
    uint8_t entropy_hex[65];
    size_t entropy_len;
    uint8_t mnemonic[216];
    size_t mnemonic_len;
    uint8_t seed_hex[129];
} mnemonic_fixture;
static const mnemonic_fixture fixtures[] = {
#include "bip39_vectors.inc"
};

static unsigned hex_digit(uint8_t value)
{
    return value <= '9' ? (unsigned)(value - '0') : (unsigned)(value - 'a') + 10U;
}

static int read_hex(const uint8_t *text, size_t length, uint8_t *out, size_t capacity)
{
    if (length % 2 != 0 || capacity < length / 2)
        return 1;
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned high = hex_digit(text[i * 2]), low = hex_digit(text[i * 2 + 1]);
        if (high > 15 || low > 15)
            return 1;
        out[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int known_vectors(void)
{
    static const uint8_t passphrase[] = "TREZOR";
    CHECK(sizeof(fixtures) / sizeof(fixtures[0]) == 24);
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        const mnemonic_fixture *fixture = &fixtures[i];
        uint8_t entropy[32] = {0}, restored[32] = {0}, text[215] = {0};
        uint8_t seed[64] = {0}, expected[64] = {0};
        size_t text_len = 0, entropy_len = 0;
        CHECK(read_hex(fixture->entropy_hex, fixture->entropy_len * 2, entropy, sizeof(entropy)) == 0);
        CHECK(read_hex(fixture->seed_hex, 128, expected, sizeof(expected)) == 0);
        CHECK(zcl_mnemonic_encode(entropy, fixture->entropy_len, text, sizeof(text), &text_len) == ZCL_OK);
        CHECK(text_len == fixture->mnemonic_len);
        CHECK(memcmp(text, fixture->mnemonic, text_len) == 0);
        CHECK(zcl_mnemonic_decode(text, text_len, restored, sizeof(restored), &entropy_len) == ZCL_OK);
        CHECK(entropy_len == fixture->entropy_len);
        CHECK(memcmp(entropy, restored, entropy_len) == 0);
        CHECK(zcl_mnemonic_seed(text, text_len, passphrase, sizeof(passphrase) - 1, seed, sizeof(seed)) == ZCL_OK);
        CHECK(memcmp(seed, expected, sizeof(seed)) == 0);
    }
    return 0;
}

static int all_entropy_sizes(void)
{
    for (size_t size = 16; size <= 32; size += 4) {
        for (unsigned sample = 0; sample < 256; ++sample) {
            uint8_t entropy[32] = {0}, restored[34] = {0}, text[217] = {0};
            size_t text_len = 0, restored_len = 0;
            for (size_t i = 0; i < size; ++i)
                entropy[i] = (uint8_t)((sample + (unsigned)i * 17U) & 0xffU);
            memset(text, 0xa5, sizeof(text));
            memset(restored, 0x5a, sizeof(restored));
            CHECK(zcl_mnemonic_encode(entropy, size, text + 1, 215, &text_len) == ZCL_OK);
            CHECK(text[0] == 0xa5 && text[216] == 0xa5 && text_len <= 215);
            CHECK(zcl_mnemonic_decode(text + 1, text_len, restored + 1, 32, &restored_len) == ZCL_OK);
            CHECK(restored[0] == 0x5a && restored[33] == 0x5a && restored_len == size);
            CHECK(memcmp(restored + 1, entropy, size) == 0);
        }
    }
    return 0;
}

static int capacity_failures(void)
{
    uint8_t entropy[32] = {0}, output[216] = {0}, before[216] = {0};
    const mnemonic_fixture *fixture = &fixtures[0];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    size_t length = 123;
    for (size_t capacity = 0; capacity < fixture->mnemonic_len; ++capacity) {
        CHECK(zcl_mnemonic_encode(entropy, 16, output, capacity, &length) == ZCL_BUFFER_TOO_SMALL);
        CHECK(length == 123 && memcmp(output, before, sizeof(output)) == 0);
    }
    for (size_t capacity = 0; capacity < 16; ++capacity) {
        CHECK(zcl_mnemonic_decode(fixture->mnemonic, fixture->mnemonic_len,
                                  output, capacity, &length) == ZCL_BUFFER_TOO_SMALL);
        CHECK(length == 123 && memcmp(output, before, sizeof(output)) == 0);
    }
    for (size_t capacity = 0; capacity < 64; ++capacity) {
        CHECK(zcl_mnemonic_seed(fixture->mnemonic, fixture->mnemonic_len,
                                entropy, 0, output, capacity) == ZCL_BUFFER_TOO_SMALL);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
    }
    return 0;
}

static int invalid_inputs(void)
{
    uint8_t output[64] = {0}, before[64] = {0}, text[216] = {0};
    size_t size = 777;
    const mnemonic_fixture *fixture = &fixtures[0];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    CHECK(zcl_mnemonic_encode(NULL, 16, output, sizeof(output), &size) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_mnemonic_encode(output, SIZE_MAX, text, sizeof(text), &size) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_mnemonic_decode(NULL, 0, output, sizeof(output), &size) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_mnemonic_decode(output, SIZE_MAX, output, sizeof(output), &size) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_mnemonic_decode(output, 0, output, sizeof(output), &size) == ZCL_OUT_OF_RANGE);
    for (size_t length = 0; length <= 33; ++length) {
        if (length >= 16 && length <= 32 && length % 4 == 0)
            continue;
        CHECK(zcl_mnemonic_encode(output, length, text, sizeof(text), &size) == ZCL_OUT_OF_RANGE);
    }
    memcpy(text, fixture->mnemonic, fixture->mnemonic_len);
    static const uint8_t invalid[] = {0, '\n', '\t', 'A', 0x80, 0xff, ' '};
    for (size_t i = 0; i < sizeof(invalid); ++i) {
        text[0] = invalid[i];
        CHECK(zcl_mnemonic_decode(text, fixture->mnemonic_len, output, sizeof(output), &size) != ZCL_OK);
        CHECK(size == 777 && memcmp(output, before, sizeof(output)) == 0);
    }
    memcpy(text, fixture->mnemonic, fixture->mnemonic_len);
    text[fixture->mnemonic_len] = ' ';
    CHECK(zcl_mnemonic_decode(text, fixture->mnemonic_len + 1, output, sizeof(output), &size) != ZCL_OK);
    static const uint8_t bad_checksum[] = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon";
    CHECK(zcl_mnemonic_decode(bad_checksum, sizeof(bad_checksum) - 1,
                              output, sizeof(output), &size) == ZCL_INVALID_ENCODING);
    CHECK(zcl_mnemonic_seed(fixture->mnemonic, fixture->mnemonic_len,
                            text, SIZE_MAX, output, sizeof(output)) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_mnemonic_seed(fixture->mnemonic, fixture->mnemonic_len,
                            NULL, 0, output, sizeof(output)) == ZCL_INVALID_ARGUMENT);
    text[0] = 0xc3;
    CHECK(zcl_mnemonic_seed(fixture->mnemonic, fixture->mnemonic_len,
                            text, 1, output, sizeof(output)) == ZCL_UNSUPPORTED);
    CHECK(size == 777 && memcmp(output, before, sizeof(output)) == 0);
    zcl_secure_zero(output, sizeof(output));
    for (size_t i = 0; i < sizeof(output); ++i)
        CHECK(output[i] == 0);
    zcl_secure_zero(NULL, 0);
    return 0;
}

int main(void)
{
    if (known_vectors() || all_entropy_sizes() || capacity_failures() || invalid_inputs())
        return 1;
    puts("mnemonic: 24 published vectors and 4 test sections passed");
    return 0;
}
