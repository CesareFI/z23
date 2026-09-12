/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "recovery confirmation check failed at line %d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    uint8_t entropy[32] = {0}, changed[32] = {0}, phrase[215] = {0};
    for (size_t length = 16; length <= 32; length += 4) {
        size_t phrase_len = 0;
        CHECK(zcl_mnemonic_encode(entropy, length, phrase, sizeof(phrase), &phrase_len) == ZCL_OK);
        CHECK(zcl_mnemonic_confirm(entropy, length, phrase, phrase_len) == ZCL_OK);
        for (size_t byte = 0; byte < length; ++byte) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                memcpy(changed, entropy, sizeof(changed));
                changed[byte] ^= (uint8_t)(1U << bit);
                CHECK(zcl_mnemonic_confirm(changed, length, phrase, phrase_len) == ZCL_INVALID_ENCODING);
            }
        }
        CHECK(zcl_mnemonic_confirm(NULL, length, phrase, phrase_len) == ZCL_INVALID_ARGUMENT);
        CHECK(zcl_mnemonic_confirm(entropy, length, NULL, phrase_len) == ZCL_INVALID_ARGUMENT);
        CHECK(zcl_mnemonic_confirm(entropy, SIZE_MAX, phrase, phrase_len) == ZCL_OUT_OF_RANGE);
        CHECK(zcl_mnemonic_confirm(entropy, length, phrase, SIZE_MAX) != ZCL_OK);
        CHECK(zcl_mnemonic_confirm(entropy, length, phrase, 0) != ZCL_OK);
        CHECK(zcl_mnemonic_confirm(entropy, length == 16 ? 20 : 16, phrase, phrase_len) == ZCL_INVALID_ENCODING);
    }
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(changed, sizeof(changed));
    zcl_secure_zero(phrase, sizeof(phrase));
    puts("recovery confirmation: all entropy sizes, every single-bit mismatch, lengths and invalid inputs passed");
    return 0;
}
