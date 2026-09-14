/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blake2_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ZCL_BLAKE2_ORACLE
#include <sodium.h>
#endif

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Public hash check failed at %d\n", __LINE__); abort(); } } while (0)
typedef struct { uint8_t before[8], bytes[64], after[8]; } digest_box;
/* Serial host/device public fixtures only, never wallet storage or secrets. */
static uint8_t saved[4097];

static void filled(const void *buffer, size_t length, uint8_t value)
{
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void guards(const digest_box *box)
{
    filled(box->before, sizeof(box->before), 0xa5);
    filled(box->after, sizeof(box->after), 0xa5);
    filled(box->bytes + 32, sizeof(box->bytes) - 32, 0xa5);
}

static void compare(const uint8_t *input, size_t length, const uint8_t *personal,
                     size_t capacity, const uint8_t *expected)
{
    CHECK(length <= sizeof(saved) && capacity <= 64);
    memcpy(saved, input, length);
    uint8_t original_personal[16];
    memcpy(original_personal, personal, sizeof(original_personal));
    digest_box box;
    memset(&box, 0xa5, sizeof(box));
    const zcl_status result = zcl_blake2b256(input, length, personal, 16, box.bytes, capacity);
    const zcl_status wanted = length > 4096 ? ZCL_OUT_OF_RANGE :
                              capacity < 32 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK;
    CHECK(result == wanted);
    CHECK(memcmp(input, saved, length) == 0 && memcmp(personal, original_personal, 16) == 0);
    guards(&box);
    if (result != ZCL_OK) {
        filled(&box, sizeof(box), 0xa5);
        return;
    }
    if (expected != NULL) CHECK(memcmp(box.bytes, expected, 32) == 0);
#ifdef ZCL_BLAKE2_ORACLE
    uint8_t salt[16] = {0}, oracle[32] = {0};
    CHECK(crypto_generichash_blake2b_salt_personal(oracle, sizeof(oracle), input,
        (unsigned long long)length, NULL, 0, salt, personal) == 0);
    CHECK(memcmp(box.bytes, oracle, sizeof(oracle)) == 0);
#endif
}

#ifndef ZCL_BLAKE2_FUZZ
#include "blake2_vectors.h"
static uint8_t input[4097];

static void known_answers(void)
{
    for (size_t i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)((17 * i + 3) & 255);
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i)
        for (size_t capacity = 0; capacity <= 64; ++capacity)
            compare(input, vectors[i].length, vectors[i].personal, capacity, vectors[i].digest);
}

static void refusals(void)
{
    uint8_t byte = 0, personal[16] = {0}, output[32];
    memset(output, 0xa5, sizeof(output));
    CHECK(zcl_blake2b256(NULL, 0, personal, 16, output, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_blake2b256(&byte, 0, NULL, 16, output, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_blake2b256(&byte, 0, personal, 16, NULL, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_blake2b256(&byte, 4097, personal, 16, output, 32) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_blake2b256(&byte, SIZE_MAX, personal, 16, output, 32) == ZCL_OUT_OF_RANGE);
    const size_t invalid[] = {0, 1, 15, 17, 32, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        CHECK(zcl_blake2b256(&byte, 0, &byte, invalid[i], output, 32) == ZCL_OUT_OF_RANGE);
    filled(output, sizeof(output), 0xa5);
    CHECK(byte == 0);
    filled(personal, sizeof(personal), 0);
    compare(input, sizeof(input), personal, 64, NULL);
}

int main(void)
{
#ifdef ZCL_BLAKE2_ORACLE
    CHECK(sodium_init() >= 0);
#endif
    known_answers();
    refusals();
    CHECK(puts("56 independent public BLAKE2b-256 vectors, all 65 capacities and span refusals passed") >= 0);
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 17 || size > 4114) return 0;
#ifdef ZCL_BLAKE2_ORACLE
    CHECK(sodium_init() >= 0);
#endif
    compare(data + 17, size - 17, data, (size_t)(data[16] % 65), NULL);
    return 0;
}
#endif
