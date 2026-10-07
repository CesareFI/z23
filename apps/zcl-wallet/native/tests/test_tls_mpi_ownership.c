/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_alloc.h"
#include "mbedtls/bignum.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Public small-integer fixtures only. The production allocator is unmodified.
 * This executable is single-threaded; fault state is armed only for one call. */
static bool armed;
static size_t allocations, fail_at;
void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
    if (armed) {
        assert(allocations < 128);
        ++allocations;
        if (allocations == fail_at) return NULL;
    }
    return __real_calloc(count, size);
}

static void invariant(const mbedtls_mpi *value)
{
    assert(value->MBEDTLS_PRIVATE(n) == 0 || value->MBEDTLS_PRIVATE(p) != NULL);
    assert(value->MBEDTLS_PRIVATE(s) == 1 || value->MBEDTLS_PRIVATE(s) == -1);
}

static void empty_heap(zcl_tls_heap *heap)
{
    assert(heap->first == NULL && heap->count == 0 && heap->used == 0);
    assert(zcl_tls_heap_clear(heap)); /* No emergency cleanup may hide a leak. */
    zcl_tls_heap_leave();
}

static int inverse(int value, int modulus)
{
    const int reduced = ((value % modulus) + modulus) % modulus;
    for (int x = 1; x < modulus; ++x) {
        if ((reduced * x) % modulus == 1) return x;
    }
    return -1;
}

static void prepare(mbedtls_mpi values[3], int value, int modulus, bool padded)
{
    for (size_t i = 0; i < 3; ++i) mbedtls_mpi_init(&values[i]);
    /* Retain the legitimate zero-limb representation when input is zero. */
    if (value != 0) assert(mbedtls_mpi_lset(&values[0], value) == 0);
    assert(mbedtls_mpi_lset(&values[1], modulus) == 0);
    if (padded) {
        assert(mbedtls_mpi_grow(&values[0], 4) == 0);
        assert(mbedtls_mpi_grow(&values[2], 5) == 0);
        assert(mbedtls_mpi_lset(&values[2], 42) == 0);
    }
}

static void retire(mbedtls_mpi values[3])
{
    for (size_t i = 0; i < 3; ++i) {
        invariant(&values[i]);
        mbedtls_mpi_free(&values[i]);
        assert(values[i].MBEDTLS_PRIVATE(p) == NULL);
        assert(values[i].MBEDTLS_PRIVATE(n) == 0);
    }
}

static void check_result(const mbedtls_mpi *output, int status, int value, int modulus)
{
    const int expected = inverse(value, modulus);
    if (expected < 0) {
        assert(status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE);
    } else {
        assert(status == 0);
        assert(mbedtls_mpi_cmp_int(output, expected) == 0);
    }
}

static void inverse_case(int value, int modulus, bool alias, bool padded)
{
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    mbedtls_mpi values[3];
    prepare(values, value, modulus, padded);
    mbedtls_mpi *output = alias ? &values[0] : &values[2];
    const int status = mbedtls_mpi_inv_mod(output, &values[0], &values[1]);
    check_result(output, status, value, modulus);
    assert(mbedtls_mpi_cmp_int(&values[1], modulus) == 0);
    if (!alias) assert(mbedtls_mpi_cmp_int(&values[0], value) == 0);
    retire(values);
    assert(!heap.denied);
    empty_heap(&heap);
}

static size_t fault_case(size_t fault, bool alias, bool padded)
{
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    mbedtls_mpi values[3];
    prepare(values, 17, 101, padded);
    mbedtls_mpi *output = alias ? &values[0] : &values[2];
    allocations = 0;
    fail_at = fault;
    armed = true;
    const int status = mbedtls_mpi_inv_mod(output, &values[0], &values[1]);
    armed = false;
    const size_t calls = allocations;
    if (fault == 0) {
        assert(!heap.denied);
        check_result(output, status, 17, 101);
    } else {
        assert(calls >= fault && heap.denied);
        assert(status == MBEDTLS_ERR_MPI_ALLOC_FAILED);
    }
    assert(mbedtls_mpi_cmp_int(&values[1], 101) == 0);
    if (!alias) assert(mbedtls_mpi_cmp_int(&values[0], 17) == 0);
    retire(values);
    empty_heap(&heap);
    return calls;
}

static size_t faults(void)
{
    size_t sites = 0;
    for (unsigned mode = 0; mode < 4; ++mode) {
        const bool alias = (mode & 1u) != 0, padded = (mode & 2u) != 0;
        const size_t calls = fault_case(0, alias, padded);
        assert(calls > 0 && calls < 128);
        for (size_t at = 1; at <= calls; ++at) (void)fault_case(at, alias, padded);
        assert(fault_case(0, alias, padded) == calls);
        sites += calls;
    }
    return sites;
}

static void invalid_moduli(void)
{
    static const int moduli[] = {-3, -1, 0, 1};
    for (size_t i = 0; i < sizeof(moduli) / sizeof(moduli[0]); ++i) {
        zcl_tls_heap heap = {0};
        assert(zcl_tls_heap_enter(&heap));
        mbedtls_mpi values[3];
        prepare(values, 17, moduli[i], true);
        assert(mbedtls_mpi_inv_mod(&values[2], &values[0], &values[1]) ==
            MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
        assert(mbedtls_mpi_cmp_int(&values[0], 17) == 0);
        assert(mbedtls_mpi_cmp_int(&values[1], moduli[i]) == 0);
        retire(values);
        assert(!heap.denied);
        empty_heap(&heap);
    }
}

static void growth_failure(void)
{
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    mbedtls_mpi value;
    mbedtls_mpi_init(&value);
    assert(mbedtls_mpi_lset(&value, 17) == 0);
    const mbedtls_mpi_uint *const original = value.MBEDTLS_PRIVATE(p);
    const size_t limbs = value.MBEDTLS_PRIVATE(n);
    allocations = 0;
    fail_at = 1;
    armed = true;
    const int status = mbedtls_mpi_grow(&value, 4);
    armed = false;
    assert(status == MBEDTLS_ERR_MPI_ALLOC_FAILED && allocations == 1 && heap.denied);
    assert(value.MBEDTLS_PRIVATE(p) == original && value.MBEDTLS_PRIVATE(n) == limbs);
    assert(mbedtls_mpi_cmp_int(&value, 17) == 0);
    invariant(&value);
    mbedtls_mpi_free(&value);
    empty_heap(&heap);
}

int main(void)
{
    size_t cases = 0;
    for (int modulus = 2; modulus <= 65; ++modulus) {
        for (int value = -65; value <= 65; ++value) {
            for (unsigned mode = 0; mode < 4; ++mode) {
                inverse_case(value, modulus, (mode & 1u) != 0, (mode & 2u) != 0);
                ++cases;
            }
        }
    }
    invalid_moduli();
    growth_failure();
    const size_t sites = faults();
    printf("MPI ownership: %zu inverse cases; %zu allocation failures and retries pass\n", cases, sites);
    return 0;
}
