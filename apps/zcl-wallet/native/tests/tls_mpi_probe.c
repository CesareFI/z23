/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Internal provider headers use its modular-arithmetic warning profile. Keep
 * them out of the fixture TU, which retains strict conversion/stack checks. */
#include "bignum_core.h"
#include "tls_mpi_probe.h"

bool zcl_test_mpi_less(const mbedtls_mpi_uint *a, const mbedtls_mpi_uint *b, size_t limbs)
{
    return mbedtls_mpi_core_lt_ct(a, b, limbs) == MBEDTLS_CT_TRUE;
}

int zcl_test_mpi_random(mbedtls_mpi_uint *x, mbedtls_mpi_uint min,
    const mbedtls_mpi_uint *n, size_t limbs,
    int (*rng)(void *, unsigned char *, size_t), void *context)
{
    return mbedtls_mpi_core_random(x, min, n, limbs, rng, context);
}

mbedtls_mpi_uint zcl_test_mpi_mla(mbedtls_mpi_uint *destination, size_t destination_count,
    const mbedtls_mpi_uint *source, size_t source_count, mbedtls_mpi_uint multiplier)
{
    return mbedtls_mpi_core_mla(destination, destination_count, source, source_count, multiplier);
}
