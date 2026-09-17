/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef mbedtls_sha256
#undef zcl_secure_zero
#include "merkle_fixture.h"
#include "zcl_keys.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Merkle retirement at %d\n", __LINE__); abort(); } } while (0)
static merkle_fixture fixture;
static uintptr_t outputs[64];
static unsigned calls, failure, clears;
int zcl_merkle_test_sha(const unsigned char *input, size_t length, unsigned char output[32], int sha224);
void zcl_merkle_test_zero(void *buffer, size_t length);

int zcl_merkle_test_sha(const unsigned char *input, size_t length, unsigned char output[32], int sha224)
{
    CHECK(calls < 64 && clears == 0 && sha224 == 0);
    CHECK(length == (calls % 2 == 0 ? 64U : 32U));
    outputs[calls++] = (uintptr_t)output;
    if (calls == failure) { memset(output, 0xa5, 32); return -1; }
    return mbedtls_sha256(input, length, output, sha224);
}

void zcl_merkle_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && clears++ == 0 && length >= 32);
    CHECK(buffer != &fixture && buffer != fixture.leaf && buffer != fixture.root);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    for (unsigned i = 0; i < calls; ++i) {
        CHECK(outputs[i] >= (uintptr_t)buffer && outputs[i] - (uintptr_t)buffer <= length - 32);
    }
}

static void run(zcl_status status, unsigned expected_calls, unsigned expected_clears)
{
    calls = clears = 0;
    CHECK(zcl_merkle_branch_check(fixture.leaf, &fixture.branch, fixture.root) == status);
    CHECK(calls == expected_calls && clears == expected_clears);
}

int main(void)
{
    CHECK(merkle_fixture_path(UINT32_MAX, UINT32_MAX - 1, &fixture));
    for (unsigned point = 1; point <= 64; ++point) {
        failure = point; run(ZCL_CRYPTO_FAILURE, point, 1);
    }
    failure = 0; run(ZCL_OK, 64, 1);
    fixture.root[0] ^= 1; run(ZCL_INVALID_ENCODING, 64, 1);
    fixture.branch.sibling_count = 31; run(ZCL_INVALID_ENCODING, 0, 0);
    fixture.branch.sibling_count = SIZE_MAX; run(ZCL_RESOURCE_EXHAUSTED, 0, 0);
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    fixture.branch.siblings[0][0] ^= 1; run(ZCL_INVALID_ENCODING, 0, 1);
    CHECK(merkle_fixture_tree(1, 0, &fixture)); run(ZCL_OK, 0, 1);
    fixture.branch.transaction_count = 0; run(ZCL_OUT_OF_RANGE, 0, 0);
    puts("Merkle dirty-provider failures and complete scratch retirement passed");
    return 0;
}
