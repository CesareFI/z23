/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Compile the actual provider once in this test executable. The archive's
 * x509_crt object is not pulled in. No production symbol/API is exposed. */
#include "../../../../vendor/android-mbedtls/library/x509_crt.c"
#include "tls_name_compare.h"

int zcl_test_x509_name_cmp(const mbedtls_x509_name *a, const mbedtls_x509_name *b)
{
    return x509_name_cmp(a, b);
}

int zcl_test_x509_string_cmp(const mbedtls_x509_buf *a, const mbedtls_x509_buf *b)
{
    return x509_string_cmp(a, b);
}
