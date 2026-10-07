/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_TLS_NAME_COMPARE_H
#define ZCL_TEST_TLS_NAME_COMPARE_H
#include "mbedtls/x509_crt.h"

int zcl_test_x509_name_cmp(const mbedtls_x509_name *a, const mbedtls_x509_name *b);
int zcl_test_x509_string_cmp(const mbedtls_x509_buf *a, const mbedtls_x509_buf *b);
#endif
