/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blake2_hash.h"
#include "zcl_keys.h"
#include <blake2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Provider names and zeroization are remapped only for this fixture target. */
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Hash failure check failed at %d\n", __LINE__); abort(); } } while (0)
static unsigned failure, calls, cleared;
static const uint8_t message[5] = {1, 2, 3, 4, 5};
static const uint8_t personal[16] = {0x11, 0, 0x22, 0, 0x33, 0, 0x44, 0,
                                     0x55, 0, 0x66, 0, 0x77, 0, 0x88, 0xff};

static void filled(const void *buffer, size_t length, uint8_t value)
{
    const uint8_t *bytes = buffer;
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

int blake2b_init_param(blake2b_state *state, const blake2b_param *parameters)
{
    CHECK(calls++ == 0 && cleared == 0 && parameters != NULL);
    filled(state, sizeof(*state), 0);
    uint8_t expected[64] = {0};
    expected[0] = 32; expected[2] = 1; expected[3] = 1;
    memcpy(expected + 48, personal, sizeof(personal));
    CHECK(sizeof(*parameters) == sizeof(expected));
    CHECK(memcmp(parameters, expected, sizeof(expected)) == 0);
    memset(state, 0x5a, sizeof(*state));
    return failure == 1 ? -1 : 0;
}

int blake2b_update(blake2b_state *state, const void *input, size_t length)
{
    CHECK(calls++ == 1 && cleared == 0 && input != NULL);
    CHECK(length == sizeof(message) && memcmp(input, message, length) == 0);
    filled(state, sizeof(*state), 0x5a);
    memset(state, 0x6b, sizeof(*state));
    return failure == 2 ? -1 : 0;
}

int blake2b_final(blake2b_state *state, void *output, size_t capacity)
{
    CHECK(calls++ == 2 && cleared == 0 && capacity == 32);
    filled(state, sizeof(*state), 0x6b);
    filled(output, capacity, 0);
    memset(output, 0x3a, capacity); /* A failed final may partially publish scratch. */
    return failure == 3 ? -1 : 0;
}

void zcl_secure_zero(void *buffer, size_t length)
{
    _Static_assert(sizeof(blake2b_state) != 64 && sizeof(blake2b_state) != 32,
                   "Fixture scratch sizes must identify three distinct objects");
    CHECK(buffer != NULL);
    unsigned bit = length == sizeof(blake2b_state) ? 1U : length == 64 ? 2U : 4U;
    CHECK(length == sizeof(blake2b_state) || length == 64 || length == 32);
    CHECK((cleared & bit) == 0);
    memset(buffer, 0, length);
    filled(buffer, length, 0); /* Observe only while the caller's object is alive. */
    cleared |= bit;
}

int main(void)
{
    for (failure = 0; failure <= 3; ++failure) {
        struct { uint8_t before[8], bytes[32], after[8]; } output;
        memset(&output, 0xa5, sizeof(output));
        calls = 0; cleared = 0;
        const zcl_status result = zcl_blake2b256(message, sizeof(message), personal,
            sizeof(personal), output.bytes, sizeof(output.bytes));
        CHECK(result == (failure == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
        CHECK(calls == (failure == 0 ? 3 : failure) && cleared == 7);
        filled(output.before, sizeof(output.before), 0xa5);
        filled(output.after, sizeof(output.after), 0xa5);
        filled(output.bytes, sizeof(output.bytes), failure == 0 ? 0x3a : 0xa5);
        calls = 0; cleared = 0;
        CHECK(zcl_blake2b256(message, sizeof(message), personal, sizeof(personal),
            output.bytes, 31) == ZCL_BUFFER_TOO_SMALL);
        CHECK(calls == 0 && cleared == 0);
    }
    CHECK(puts("Provider init/update/final failures preserve output and clear all scratch") >= 0);
    return 0;
}
