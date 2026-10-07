/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_alloc.h"
#include "rsa_internal.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A public structural fixture, not a production key or a trusted certificate.
 * Its 256-bit modulus is intentionally below the transport security minimum. */
enum { DER_SIZE = 42, INPUT_MAX = 64 };
static const uint8_t public_der[INPUT_MAX] = {
    [0] = 0x30, [1] = 40, [2] = 0x02, [3] = 33, [5] = 0x80, [36] = 1,
    [37] = 0x02, [38] = 3, [39] = 1, [41] = 1
};
static bool armed;
static size_t allocations, fail_at;
void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
    if (armed) {
        assert(allocations < 16);
        ++allocations;
        if (allocations == fail_at) return NULL;
    }
    return __real_calloc(count, size);
}

static void fixture(uint8_t output[INPUT_MAX])
{
    memcpy(output, public_der, sizeof(public_der));
}

static void check_key(const mbedtls_rsa_context *key)
{
    uint8_t modulus[32] = {0}, exponent[3] = {0};
    assert(mbedtls_rsa_get_bitlen(key) == 256 && mbedtls_rsa_get_len(key) == 32);
    assert(mbedtls_rsa_export_raw(key, modulus, sizeof(modulus), NULL, 0,
        NULL, 0, NULL, 0, exponent, sizeof(exponent)) == 0);
    uint8_t expected[32] = {0x80};
    expected[31] = 1;
    assert(memcmp(modulus, expected, sizeof(expected)) == 0);
    const uint8_t e[3] = {1, 0, 1};
    assert(memcmp(exponent, e, sizeof(e)) == 0);
}

typedef struct { int status; size_t calls; bool denied; } parse_result;
static parse_result parse(const uint8_t *data, size_t length, size_t fault)
{
    assert(length <= INPUT_MAX);
    /* Exact-size allocation puts every truncation boundary next to ASan's
     * redzone. A zero-length parse still receives a valid nonnull pointer. */
    uint8_t *copy = malloc(length == 0 ? 1 : length);
    assert(copy != NULL);
    memcpy(copy, data, length);
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    mbedtls_rsa_context key;
    mbedtls_rsa_init(&key);
    allocations = 0; fail_at = fault; armed = true;
    const int status = mbedtls_rsa_parse_pubkey(&key, copy, length);
    armed = false;
    const parse_result result = {status, allocations, heap.denied};
    assert(memcmp(copy, data, length) == 0);
    if (status == 0) check_key(&key);
    mbedtls_rsa_free(&key);
    assert(heap.first == NULL && heap.used == 0 && heap.count == 0);
    assert(zcl_tls_heap_clear(&heap));
    zcl_tls_heap_leave();
    free(copy);
    return result;
}

static void expect(const uint8_t *data, size_t length, int status)
{
    const parse_result result = parse(data, length, 0);
    assert(result.status == status && !result.denied);
}

static void truncations(void)
{
    for (size_t length = 0; length < DER_SIZE; ++length) {
        const parse_result result = parse(public_der, length, 0);
        assert(result.status != 0 && !result.denied);
    }
    expect(public_der, DER_SIZE, 0);
}

static void malformed(void)
{
    static const struct { size_t at; uint8_t value; int error; } edits[] = {
        {0, 0x31, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG},
        {1, 0x80, MBEDTLS_ERR_ASN1_INVALID_LENGTH},
        {1, 0x7f, MBEDTLS_ERR_ASN1_OUT_OF_DATA},
        {1, 39, MBEDTLS_ERR_RSA_BAD_INPUT_DATA},
        {2, 0x03, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG},
        {3, 0x7f, MBEDTLS_ERR_ASN1_OUT_OF_DATA},
        {37, 0x03, MBEDTLS_ERR_ASN1_UNEXPECTED_TAG},
        {38, 0x7f, MBEDTLS_ERR_ASN1_OUT_OF_DATA},
        {36, 2, MBEDTLS_ERR_RSA_BAD_INPUT_DATA},
        {41, 2, MBEDTLS_ERR_RSA_BAD_INPUT_DATA}
    };
    uint8_t data[INPUT_MAX];
    for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i) {
        fixture(data);
        data[edits[i].at] = edits[i].value;
        expect(data, DER_SIZE, edits[i].error);
    }
    fixture(data);
    expect(data, DER_SIZE + 1, MBEDTLS_ERR_RSA_BAD_INPUT_DATA);
    data[1] = 41;
    expect(data, DER_SIZE + 1, MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
}

static void invalid_components(void)
{
    uint8_t data[INPUT_MAX];
    fixture(data);
    memset(data + 5, 0, 32);
    expect(data, DER_SIZE, MBEDTLS_ERR_RSA_BAD_INPUT_DATA);
    fixture(data);
    memset(data + 39, 0, 3);
    expect(data, DER_SIZE, MBEDTLS_ERR_RSA_BAD_INPUT_DATA);
    data[41] = 1;
    expect(data, DER_SIZE, MBEDTLS_ERR_RSA_BAD_INPUT_DATA);
}

static void allocation_failures(void)
{
    uint8_t data[INPUT_MAX];
    fixture(data);
    const parse_result baseline = parse(data, DER_SIZE, 0);
    assert(baseline.status == 0 && baseline.calls == 2 && !baseline.denied);
    for (size_t fault = 1; fault <= baseline.calls; ++fault) {
        const parse_result result = parse(data, DER_SIZE, fault);
        assert(result.status == MBEDTLS_ERR_RSA_BAD_INPUT_DATA);
        assert(result.denied && result.calls == fault);
    }
    expect(data, DER_SIZE, 0);
}

int main(void)
{
    truncations();
    malformed();
    invalid_components();
    allocation_failures();
    puts("RSA public parser: truncation, format, key checks, both import failures and cleanup pass");
    return 0;
}
