/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "source_vectors.h"
#undef zcl_secure_zero
void zcl_secure_zero(void *, size_t);
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Source fault at %d\n", __LINE__); abort(); } } while (0)
static unsigned fault, hashes, wipes, work_wipes;

int zcl_source_test_sha(const unsigned char *input, size_t length, unsigned char *output, int is224)
{
    CHECK(input != NULL && output != NULL && is224 == 0 && hashes < 2);
    CHECK(wipes == 0 && work_wipes == 0);
    for (size_t i = 0; i < 32; ++i) CHECK(output[i] == 0);
    if (hashes == 0) CHECK(input == source_vectors[0].wire && length == source_vectors[0].length);
    else {
        CHECK(length == 32);
        for (size_t i = 0; i < length; ++i) CHECK(input[i] == 0x11);
    }
    ++hashes;
    memset(output, hashes == 1 ? 0x11 : 0x22, 32);
    return fault == hashes ? -1 : 0;
}

void zcl_source_test_zero(void *output, size_t length)
{
    CHECK(output != NULL);
    if (length == 32) CHECK(wipes++ < 2 && work_wipes == 0);
    else CHECK(length == sizeof(zcl_v4_source) && wipes == 2 && work_wipes++ == 0);
    zcl_secure_zero(output, length);
    const uint8_t *bytes = output;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}

int main(void)
{
    for (fault = 0; fault <= 2; ++fault) {
        hashes = wipes = work_wipes = 0;
        zcl_v4_source output, before;
        memset(&output, 0xa5, sizeof(output)); memcpy(&before, &output, sizeof(output));
        CHECK(zcl_v4_source_inspect(source_vectors[0].wire, source_vectors[0].length, 0, &output)
            == (fault == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
        CHECK(hashes == (fault == 1 ? 1u : 2u) && wipes == 2 && work_wipes == 1);
        if (fault != 0) CHECK(memcmp(&output, &before, sizeof(output)) == 0);
        else for (size_t i = 0; i < 32; ++i) CHECK(output.transaction_id[i] == 0x22);
    }
    puts("V4 source dirty hash failures preserve output and retire staging");
    return 0;
}
