/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef mbedtls_sha256
#undef zcl_secure_zero
#include "transaction_source_internal.h"
#include "zcl_keys.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "legacy_source_vectors.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Legacy retirement at %d\n", __LINE__); abort(); } } while (0)
static unsigned fault, hashes, small_clears, work_clears;
static uintptr_t hash_outputs[2];
int zcl_legacy_test_sha(const unsigned char *, size_t, unsigned char[32], int);
void zcl_legacy_test_zero(void *, size_t);
int zcl_legacy_test_sha(const unsigned char *input, size_t length, unsigned char output[32], int sha224)
{
    CHECK(hashes < 2 && small_clears == 0 && work_clears == 0 && sha224 == 0);
    hash_outputs[hashes++] = (uintptr_t)output;
    if (fault == hashes) { memset(output, 0xa5, 32); return -1; }
    return mbedtls_sha256(input, length, output, sha224);
}
void zcl_legacy_test_zero(void *buffer, size_t length)
{
    if (length == 32) {
        CHECK(small_clears < 2 && work_clears == 0);
        if (small_clears < hashes) CHECK((uintptr_t)buffer == hash_outputs[small_clears]);
        ++small_clears;
    } else CHECK(length == sizeof(zcl_source_view) && small_clears == 2 && work_clears++ == 0);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}
int main(void)
{
    for (size_t row = 0; row < sizeof(legacy_vectors) / sizeof(legacy_vectors[0]); ++row) {
        for (fault = 0; fault <= 2; ++fault) {
            hashes = small_clears = work_clears = 0;
            zcl_source_view view, before;
            memset(&view, 0xa5, sizeof(view)); memcpy(&before, &view, sizeof(view));
            const zcl_status status = zcl_legacy_source_inspect(legacy_vectors[row].wire, legacy_vectors[row].length, 0, &view);
            CHECK(status == (fault == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
            CHECK(hashes == (fault == 1 ? 1U : 2U) && small_clears == 2 && work_clears == 1);
            if (fault != 0) CHECK(memcmp(&view, &before, sizeof(view)) == 0);
        }
    }
    puts("Legacy source dirty-provider failures and retirement passed"); return 0;
}
