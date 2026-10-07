/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Reuse the real codec fixture, including its cleanup and status propagation. */
#define main zcl_codec_fixture_main
#include "test_codecs.c"
#undef main
#include "zcl_tls_alloc.h"
#include <assert.h>

int __real_mbedtls_sha256_self_test(int verbose);
int __wrap_mbedtls_sha256_self_test(int verbose);
enum { REAL_SELFTEST, FAILED_SELFTEST, LEAKED_SCRATCH, DENIED_SCRATCH };
static unsigned mode;

int __wrap_mbedtls_sha256_self_test(int verbose)
{
    switch (mode) {
        case FAILED_SELFTEST: return 1;
        case LEAKED_SCRATCH:
            /* Deliberately leave public scratch to the emergency cleanup.
             * Reclamation must not promote an incomplete selftest to PASS. */
            assert(zcl_tls_calloc(1, 1) != NULL);
            return 0;
        case DENIED_SCRATCH:
            assert(zcl_tls_calloc(SIZE_MAX, 2) == NULL);
            return 0;
        default: return __real_mbedtls_sha256_self_test(verbose);
    }
}

static void scope_closed(void)
{
    assert(zcl_tls_calloc(1, 1) == NULL);
    zcl_tls_heap next = {0};
    assert(zcl_tls_heap_enter(&next));
    assert(zcl_tls_heap_clear(&next));
    zcl_tls_heap_leave();
}

int main(void)
{
    scope_closed();
    for (mode = REAL_SELFTEST; mode <= DENIED_SCRATCH; ++mode) {
        const int expected = mode == REAL_SELFTEST ? 0 : 1;
        assert(zcl_codec_fixture_main() == expected);
        scope_closed();
    }
    mode = REAL_SELFTEST;
    assert(zcl_codec_fixture_main() == 0);
    scope_closed();
    puts("TLS codec scope: failures, leftover/denied scratch and recovery pass");
    return 0;
}
