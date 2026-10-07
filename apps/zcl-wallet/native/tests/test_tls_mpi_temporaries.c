/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "tls_mpi_probe.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Public fixed fixtures only. Never install this RNG in wallet/TLS code. */
typedef struct {
    uint8_t rejected[8], accepted[8];
    size_t width, calls, accept_at, fail_at;
} fixture_rng;

static int public_rng(void *context, unsigned char *output, size_t length)
{
    fixture_rng *rng = context;
    assert(length == rng->width && length <= sizeof(rng->accepted));
    assert(rng->calls < 256);
    ++rng->calls;
    if (rng->calls == rng->fail_at) return -73;
    const uint8_t *bytes = rng->calls == rng->accept_at ? rng->accepted : rng->rejected;
    memcpy(output, bytes, length);
    return 0;
}

static bool reference_less(const mbedtls_mpi_uint *a, const mbedtls_mpi_uint *b, size_t count)
{
    for (size_t i = count; i != 0; --i) {
        if (a[i - 1] != b[i - 1]) return a[i - 1] < b[i - 1];
    }
    return false;
}

static void compare(const mbedtls_mpi_uint a[4], const mbedtls_mpi_uint b[4], size_t count)
{
    mbedtls_mpi_uint saved_a[4], saved_b[4];
    memcpy(saved_a, a, sizeof(saved_a));
    memcpy(saved_b, b, sizeof(saved_b));
    const bool expected = reference_less(a, b, count);
    assert(zcl_test_mpi_less(a, b, count) == expected);
    assert(memcmp(saved_a, a, sizeof(saved_a)) == 0);
    assert(memcmp(saved_b, b, sizeof(saved_b)) == 0);
}

static void comparisons(void)
{
    mbedtls_mpi_uint a[4] = {0}, b[4] = {0};
    for (unsigned left = 0; left < 256; ++left) {
        for (unsigned right = 0; right < 256; ++right) {
            a[0] = left;
            b[0] = right;
            compare(a, b, 1);
        }
    }
    const mbedtls_mpi_uint values[] = {0, 1, 255, 256,
        (mbedtls_mpi_uint)-1, (mbedtls_mpi_uint)-2};
    for (size_t index = 0; index < 4; ++index) {
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
            for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
                for (size_t k = 0; k < 4; ++k) a[k] = b[k] = values[(i + k) % 6];
                a[index] = values[i];
                b[index] = values[j];
                compare(a, b, 4);
                compare(a, a, 4); /* Input aliasing is read-only. */
            }
        }
    }
}

static mbedtls_mpi_uint upper_limb(uint64_t value)
{
    const size_t half = sizeof(mbedtls_mpi_uint) * 4;
    /* Two half shifts are defined for both 32- and 64-bit limbs. */
    return (mbedtls_mpi_uint)((value >> half) >> half);
}

static void encode(uint64_t value, uint8_t output[8], size_t width)
{
    assert(width != 0 && width <= 8);
    for (size_t i = width; i != 0; --i) {
        output[i - 1] = (uint8_t)value;
        value >>= 8;
    }
    assert(value == 0);
}

typedef struct {
    uint64_t modulus, rejected, accepted, expected;
    mbedtls_mpi_uint minimum;
    size_t width, accept_at, fail_at, calls;
    int status;
} sample_case;

static void sampling(const sample_case *test)
{
    const mbedtls_mpi_uint canary = (mbedtls_mpi_uint)0xa5a5a5a5;
    mbedtls_mpi_uint guarded[4] = {canary, canary, canary, canary};
    mbedtls_mpi_uint modulus[2] = {(mbedtls_mpi_uint)test->modulus, upper_limb(test->modulus)};
    fixture_rng rng = {.width = test->width, .accept_at = test->accept_at,
        .fail_at = test->fail_at};
    encode(test->rejected, rng.rejected, test->width);
    encode(test->accepted, rng.accepted, test->width);
    const int result = zcl_test_mpi_random(guarded + 1, test->minimum, modulus, 2,
        public_rng, &rng);
    assert(result == test->status && rng.calls == test->calls);
    assert(guarded[0] == canary && guarded[3] == canary);
    assert(modulus[0] == (mbedtls_mpi_uint)test->modulus);
    assert(modulus[1] == upper_limb(test->modulus));
    if (result == 0) {
        assert(guarded[1] == (mbedtls_mpi_uint)test->expected);
        assert(guarded[2] == upper_limb(test->expected));
    }
}

static void sample_boundaries(void)
{
    /* Preserve the existing rejection of the last allowed draw even when its
     * candidate is in range. This patch does not redefine sampling semantics. */
    static const sample_case cases[] = {
        {11, 0x20, 0x30, 3, 3, 1, 1, 0, 1, 0},
        {11, 0x20, 0xa0, 10, 3, 1, 2, 0, 2, 0},
        {11, 0xb0, 0x30, 3, 3, 1, 2, 0, 2, 0},
        {11, 0x20, 0x30, 3, 3, 1, 249, 0, 249, 0},
        {11, 0x20, 0x30, 0, 3, 1, 250, 0, 250, MBEDTLS_ERR_MPI_NOT_ACCEPTABLE},
        {11, 0x20, 0x30, 0, 3, 1, 0, 0, 250, MBEDTLS_ERR_MPI_NOT_ACCEPTABLE},
        {11, 0x20, 0x30, 0, 3, 1, 0, 1, 1, -73},
        {11, 0x20, 0x30, 0, 3, 1, 0, 2, 2, -73},
        {11, 0x20, 0x30, 0, 3, 1, 0, 250, 250, -73},
        {UINT64_C(0x100000001), 0, 0x180, 3, 3, 5, 29, 0, 29, 0},
        {UINT64_C(0x100000001), 0, 0x180, 0, 3, 5, 30, 0, 30, MBEDTLS_ERR_MPI_NOT_ACCEPTABLE},
        {UINT64_C(0x100000001), 0, 0x180, 0, 3, 5, 0, 30, 30, -73},
        {65536, 0, 0x180, 3, 3, 3, 1, 0, 1, 0},
        {1, 0x80, 0, 0, 0, 1, 2, 0, 2, 0}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) sampling(&cases[i]);
}

int main(void)
{
    _Static_assert(sizeof(mbedtls_mpi_uint) == 4 || sizeof(mbedtls_mpi_uint) == 8,
        "Fixture models 32- and 64-bit limbs");
    comparisons();
    sample_boundaries();
    puts("MPI temporary lifetimes: comparison, sampling, retry and RNG failures pass");
    return 0;
}
