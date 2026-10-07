/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "tls_name_compare.h"
#include <assert.h>
#include <stdio.h>

static void strings(const mbedtls_x509_buf *a, const mbedtls_x509_buf *b, int expected)
{
    assert(zcl_test_x509_string_cmp(a, b) == expected);
    assert(zcl_test_x509_string_cmp(b, a) == expected);
}

static void empty_strings(void)
{
    unsigned char byte = 'a';
    mbedtls_x509_buf a = {0}, b = {0};
    strings(&a, &b, 0);
    b.p = &byte; /* Empty lengths, independently NULL/non-NULL storage. */
    strings(&a, &b, 0);
    b.len = 1;
    strings(&a, &b, -1);
    b.len = 0;
    b.tag = MBEDTLS_ASN1_UTF8_STRING;
    strings(&a, &b, -1);
    a.tag = MBEDTLS_ASN1_PRINTABLE_STRING;
    strings(&a, &b, 0);
    a.p = &byte;
    strings(&a, &b, 0);
}

static void nonempty_strings(void)
{
    unsigned char lower[] = "name", upper[] = "NAME", different[] = "namo";
    mbedtls_x509_buf a = {MBEDTLS_ASN1_UTF8_STRING, 4, lower};
    mbedtls_x509_buf b = {MBEDTLS_ASN1_PRINTABLE_STRING, 4, upper};
    strings(&a, &b, 0);
    b.p = different;
    strings(&a, &b, -1);
    b.p = lower;
    b.len = 3;
    strings(&a, &b, -1);
    b.len = 4;
    b.tag = MBEDTLS_ASN1_OCTET_STRING;
    strings(&a, &b, -1);
    a.tag = MBEDTLS_ASN1_OCTET_STRING;
    strings(&a, &b, 0);
    b.p = upper;
    strings(&a, &b, -1); /* No case folding outside the existing text tags. */
}

static void names(const mbedtls_x509_name *a, const mbedtls_x509_name *b, int expected)
{
    assert(zcl_test_x509_name_cmp(a, b) == expected);
    assert(zcl_test_x509_name_cmp(b, a) == expected);
}

static void empty_names(void)
{
    mbedtls_x509_name a = {0}, b = {0};
    unsigned char byte = 0;
    names(NULL, NULL, 0);
    names(&a, NULL, -1);
    names(&a, &b, 0);
    b.oid.p = &byte;
    names(&a, &b, 0);
    b.val.p = &byte;
    names(&a, &b, 0);
    b.oid.len = 1;
    names(&a, &b, -1);
    b.oid.len = 0;
    b.oid.tag = MBEDTLS_ASN1_OID;
    names(&a, &b, -1);
    b.oid.tag = 0;
    b.MBEDTLS_PRIVATE(next_merged) = 1;
    names(&a, &b, -1);
    b.MBEDTLS_PRIVATE(next_merged) = 0;
    mbedtls_x509_name tail = {0};
    b.next = &tail;
    names(&a, &b, -1);
}

static void nonempty_names(void)
{
    unsigned char oid[] = {0x55, 0x04, 0x03}, other_oid[] = {0x55, 0x04, 0x04};
    unsigned char value[] = "name", other_value[] = "nope";
    mbedtls_x509_name a = {0}, b = {0};
    a.oid = (mbedtls_x509_buf){MBEDTLS_ASN1_OID, sizeof(oid), oid};
    a.val = (mbedtls_x509_buf){MBEDTLS_ASN1_UTF8_STRING, 4, value};
    b = a;
    names(&a, &b, 0);
    b.oid.p = other_oid;
    names(&a, &b, -1);
    b = a;
    b.val.p = other_value;
    names(&a, &b, -1);
    b = a;
    b.MBEDTLS_PRIVATE(next_merged) = 1;
    names(&a, &b, -1);
    a.MBEDTLS_PRIVATE(next_merged) = 1;
    names(&a, &b, 0);
    mbedtls_x509_name tail_a = {0}, tail_b = {0};
    a.next = &tail_a;
    b.next = &tail_b;
    names(&a, &b, 0);
    tail_b.val = a.val;
    names(&a, &b, -1);
}

int main(void)
{
    empty_strings();
    nonempty_strings();
    empty_names();
    nonempty_names();
    puts("X509 empty names, typed values, identity and list structure pass");
    return 0;
}
