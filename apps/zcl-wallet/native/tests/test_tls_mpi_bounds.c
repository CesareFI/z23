/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "tls_mpi_probe.h"
#include <openssl/bn.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LIMBS = 33, MAX_BYTES = (MAX_LIMBS + 1) * sizeof(mbedtls_mpi_uint) };

static size_t encode(uint8_t output[MAX_BYTES], const mbedtls_mpi_uint *value, size_t count)
{
    assert(count <= MAX_LIMBS);
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < sizeof(*value); ++j)
            output[i * sizeof(*value) + j] = (uint8_t)(value[i] >> (j * 8));
    }
    return count * sizeof(*value);
}

static BIGNUM *number(const mbedtls_mpi_uint *value, size_t count)
{
    uint8_t bytes[MAX_BYTES] = {0};
    const size_t length = encode(bytes, value, count);
    assert(length <= INT_MAX);
    BIGNUM *result = BN_lebin2bn(bytes, (int)length, NULL);
    assert(result != NULL);
    return result;
}

static void oracle(uint8_t expected[MAX_BYTES], const mbedtls_mpi_uint *source,
    size_t source_count, const mbedtls_mpi_uint *destination, size_t destination_count,
    mbedtls_mpi_uint multiplier)
{
    assert(destination_count <= MAX_LIMBS);
    BIGNUM *a = number(source, source_count), *b = number(&multiplier, 1);
    BIGNUM *d = number(destination, destination_count);
    BIGNUM *product = BN_new();
    BN_CTX *context = BN_CTX_new();
    assert(product != NULL && context != NULL);
    assert(BN_mul(product, a, b, context) == 1);
    assert(BN_add(product, product, d) == 1);
    const size_t bytes = (destination_count + 1) * sizeof(*destination);
    assert(bytes <= MAX_BYTES && bytes <= INT_MAX);
    assert(BN_bn2lebinpad(product, expected, (int)bytes) == (int)bytes);
    BN_CTX_free(context);
    BN_free(product);
    BN_free(d);
    BN_free(b);
    BN_free(a);
}

static mbedtls_mpi_uint limb(size_t index, unsigned pattern)
{
    switch (pattern) {
        case 0: return 0;
        case 1: return (mbedtls_mpi_uint)-1;
        case 2: return (index & 1u) != 0 ? (mbedtls_mpi_uint)-1 : 0;
        default: return (mbedtls_mpi_uint)(index + 1) * 0x10101u;
    }
}

static void one_case(size_t source_count, size_t extra, unsigned pattern,
    mbedtls_mpi_uint multiplier, bool alias)
{
    assert(source_count <= 31 && extra <= 2 && (!alias || extra == 0));
    const size_t destination_count = source_count + extra;
    /* These exact extents, not a padded maximum array, enforce ASan boundaries.
     * Zero-length operations retain a valid nonnull allocation. */
    mbedtls_mpi_uint *source = calloc(source_count == 0 ? 1 : source_count, sizeof(*source));
    mbedtls_mpi_uint *destination = calloc(destination_count == 0 ? 1 : destination_count,
        sizeof(*destination));
    assert(source != NULL && destination != NULL);
    for (size_t i = 0; i < source_count; ++i) source[i] = limb(i, pattern);
    for (size_t i = 0; i < destination_count; ++i) destination[i] = limb(i, pattern ^ 1u);
    mbedtls_mpi_uint saved[MAX_LIMBS] = {0};
    memcpy(saved, source, source_count * sizeof(*source));
    uint8_t expected[MAX_BYTES] = {0}, actual[MAX_BYTES] = {0};
    const mbedtls_mpi_uint *input = alias ? destination : source;
    oracle(expected, input, source_count, destination, destination_count, multiplier);
    const mbedtls_mpi_uint carry = zcl_test_mpi_mla(destination, destination_count,
        input, source_count, multiplier);
    const size_t used = encode(actual, destination, destination_count);
    uint8_t tail[MAX_BYTES] = {0};
    const size_t tail_length = encode(tail, &carry, 1);
    assert(used <= sizeof(actual) - tail_length);
    memcpy(actual + used, tail, tail_length);
    assert(memcmp(actual, expected, used + tail_length) == 0);
    assert(memcmp(source, saved, source_count * sizeof(*source)) == 0);
    free(destination);
    free(source);
}

int main(void)
{
    _Static_assert(sizeof(mbedtls_mpi_uint) == 4 || sizeof(mbedtls_mpi_uint) == 8,
        "Fixture models 32- and 64-bit limbs");
    static const size_t lengths[] = {0, 1, 2, 7, 8, 9, 15, 16, 17, 31};
    const mbedtls_mpi_uint multipliers[] = {0, 1, (mbedtls_mpi_uint)-1,
        ((mbedtls_mpi_uint)-1) >> 1};
    size_t cases = 0;
    for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); ++n) {
        for (size_t extra = 0; extra <= 2; ++extra) {
            for (unsigned pattern = 0; pattern < 4; ++pattern) {
                for (size_t m = 0; m < sizeof(multipliers) / sizeof(multipliers[0]); ++m) {
                    one_case(lengths[n], extra, pattern, multipliers[m], false);
                    ++cases;
                    if (extra == 0) {
                        one_case(lengths[n], 0, pattern, multipliers[m], true);
                        ++cases;
                    }
                }
            }
        }
    }
    printf("MPI multiply-accumulate: %zu independent bounds/carry/alias cases pass\n", cases);
    return 0;
}
