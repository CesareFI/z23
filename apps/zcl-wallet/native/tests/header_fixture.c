/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "header_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ZCL_HEADER_ORACLE
#include <openssl/evp.h>
#endif
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Header fixture at %d\n", __LINE__); abort(); } } while (0)

void header_fixture_decode(const char *hex, uint8_t *wire, size_t length)
{
    static const char digits[] = "0123456789abcdef";
    CHECK(strlen(hex) == length * 2);
    for (size_t i = 0; i < length; ++i) {
        const char *high = strchr(digits, hex[2 * i]);
        const char *low = strchr(digits, hex[2 * i + 1]);
        CHECK(high != NULL && low != NULL);
        wire[i] = (uint8_t)((high - digits) * 16 + (low - digits));
    }
}

static void field(const uint8_t *raw, const uint8_t *display)
{
    for (size_t i = 0; i < 32; ++i) CHECK(raw[i] == display[31 - i]);
}

static void scalar(const uint8_t *wire, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i) {
        CHECK(wire[i] == (uint8_t)(value & 255));
        value >>= 8;
    }
}

void header_fixture_check(const uint8_t *wire, size_t length, const zcl_header_view *view)
{
    CHECK(length >= 143 && view->solution_length == length - 143);
    field(wire + 4, view->previous); field(wire + 36, view->merkle);
    field(wire + 68, view->sapling); field(wire + 108, view->nonce);
    scalar(wire, view->version_bits); scalar(wire + 100, view->timestamp); scalar(wire + 104, view->bits);
#ifdef ZCL_HEADER_ORACLE
    uint8_t first[32], second[32]; unsigned count = 0;
    CHECK(EVP_Digest(wire, length, first, &count, EVP_sha256(), NULL) == 1 && count == 32);
    CHECK(EVP_Digest(first, sizeof(first), second, &count, EVP_sha256(), NULL) == 1 && count == 32);
    field(second, view->hash);
#endif
}
