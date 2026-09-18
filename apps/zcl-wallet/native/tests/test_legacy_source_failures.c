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
#ifdef ZCL_MIXED_SOURCE_TEST
#include "source_vectors.h"
#define zcl_legacy_source_inspect zcl_source_inspect
#endif
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Legacy retirement at %d\n", __LINE__); abort(); } } while (0)
static unsigned fault, hashes, small_clears, work_clears;
static unsigned expected_work = 1;
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
    } else CHECK(length == sizeof(zcl_source_view) && small_clears == 2 && work_clears++ < expected_work);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}
static void inspect(const uint8_t *wire, size_t length)
{
    for (fault = 0; fault <= 2; ++fault) {
        hashes = small_clears = work_clears = 0;
        zcl_source_view view, before;
        memset(&view, 0xa5, sizeof(view)); memcpy(&before, &view, sizeof(view));
        const zcl_status status = zcl_legacy_source_inspect(wire, length, 0, &view);
        CHECK(status == (fault == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
        CHECK(hashes == (fault == 1 ? 1U : 2U) && small_clears == 2 && work_clears == 1);
        if (fault != 0) CHECK(memcmp(&view, &before, sizeof(view)) == 0);
    }
}

#ifdef ZCL_MIXED_SOURCE_TEST
static void matching(const uint8_t *wire, size_t length, const uint8_t id[32])
{
    zcl_tx_input input = {0}; memcpy(input.previous_txid, id, 32);
    expected_work = 2;
    for (fault = 0; fault <= 2; ++fault) {
        hashes = small_clears = work_clears = 0;
        zcl_tx_output output, before;
        memset(&output, 0xa5, sizeof(output)); memcpy(&before, &output, sizeof(output));
        CHECK(zcl_source_prevout(&input, wire, length, &output) == (fault == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
        CHECK(hashes == (fault == 1 ? 1U : 2U) && small_clears == 2 && work_clears == 2);
        if (fault != 0) CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    }
    expected_work = 1;
}
#endif

int main(void)
{
    for (size_t row = 0; row < sizeof(legacy_vectors) / sizeof(legacy_vectors[0]); ++row) {
        inspect(legacy_vectors[row].wire, legacy_vectors[row].length);
#ifdef ZCL_MIXED_SOURCE_TEST
        matching(legacy_vectors[row].wire, legacy_vectors[row].length, legacy_vectors[row].id);
#endif
    }
#ifdef ZCL_MIXED_SOURCE_TEST
    for (size_t row = 0; row < sizeof(source_vectors) / sizeof(source_vectors[0]); ++row) {
        inspect(source_vectors[row].wire, source_vectors[row].length);
        matching(source_vectors[row].wire, source_vectors[row].length, source_vectors[row].id);
    }
#endif
#ifdef ZCL_MIXED_SOURCE_TEST
    puts("Mixed source inspection/matching dirty-provider retirement passed");
#else
    puts("Legacy source dirty-provider failures and retirement passed");
#endif
    return 0;
}
