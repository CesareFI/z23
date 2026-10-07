/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_TLS_MPI_PROBE_H
#define ZCL_TEST_TLS_MPI_PROBE_H
#include "mbedtls/bignum.h"
#include <stdbool.h>

bool zcl_test_mpi_less(const mbedtls_mpi_uint *a, const mbedtls_mpi_uint *b, size_t limbs);
mbedtls_mpi_uint zcl_test_mpi_mla(mbedtls_mpi_uint *destination, size_t destination_count,
    const mbedtls_mpi_uint *source, size_t source_count, mbedtls_mpi_uint multiplier);
int zcl_test_mpi_random(mbedtls_mpi_uint *x, mbedtls_mpi_uint min,
    const mbedtls_mpi_uint *n, size_t limbs,
    int (*rng)(void *, unsigned char *, size_t), void *context);
#endif
