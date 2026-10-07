/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_alloc.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Entirely synthetic public sessions: never persist actual TLS secrets here.
 * The selected provider format has a five-byte version/configuration header,
 * four protocol bytes, then optional time before the TLS 1.2 session ID. */
enum { INPUT_MAX = 256, HEADER_SIZE = 5, PROTOCOL_SIZE = 4 };
#if defined(MBEDTLS_HAVE_TIME)
enum { ID_OFFSET = HEADER_SIZE + PROTOCOL_SIZE + 8 };
#else
enum { ID_OFFSET = HEADER_SIZE + PROTOCOL_SIZE };
#endif
enum { DIGEST_OFFSET = ID_OFFSET + 1 + 32 + 48 + 4 };
static bool armed;
static size_t allocations, fail_at;
void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
    if (armed) {
        assert(allocations < 8);
        ++allocations;
        if (allocations == fail_at) return NULL;
    }
    return __real_calloc(count, size);
}

static size_t fixture(uint8_t output[INPUT_MAX], bool digest)
{
    mbedtls_ssl_session session;
    mbedtls_ssl_session_init(&session);
    session.MBEDTLS_PRIVATE(tls_version) = MBEDTLS_SSL_VERSION_TLS1_2;
    session.MBEDTLS_PRIVATE(endpoint) = MBEDTLS_SSL_IS_CLIENT;
    session.MBEDTLS_PRIVATE(ciphersuite) = MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256;
#if defined(MBEDTLS_HAVE_TIME)
    session.MBEDTLS_PRIVATE(start) = 1234;
#endif
    session.MBEDTLS_PRIVATE(id_len) = 1;
    memset(session.MBEDTLS_PRIVATE(id), 0x41, sizeof(session.MBEDTLS_PRIVATE(id)));
    memset(session.MBEDTLS_PRIVATE(master), 0x42, sizeof(session.MBEDTLS_PRIVATE(master)));
    size_t length = 0;
    memset(output, 0, INPUT_MAX);
    assert(mbedtls_ssl_session_save(&session, output, INPUT_MAX, &length) == 0);
    mbedtls_ssl_session_free(&session);
    assert(length == DIGEST_OFFSET + 2 && output[ID_OFFSET] == 1);
    assert(output[DIGEST_OFFSET] == MBEDTLS_MD_NONE && output[DIGEST_OFFSET + 1] == 0);
    if (digest) {
        output[DIGEST_OFFSET] = MBEDTLS_MD_SHA256;
        output[DIGEST_OFFSET + 1] = 32;
        memset(output + length, 0x43, 32);
        length += 32;
    }
    return length;
}

static void check_loaded(const mbedtls_ssl_session *session,
                         const uint8_t *data, size_t length)
{
    assert(session->MBEDTLS_PRIVATE(tls_version) == MBEDTLS_SSL_VERSION_TLS1_2);
    assert(session->MBEDTLS_PRIVATE(endpoint) == MBEDTLS_SSL_IS_CLIENT);
    assert(session->MBEDTLS_PRIVATE(ciphersuite) == MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
    assert(session->MBEDTLS_PRIVATE(id_len) == data[ID_OFFSET]);
    assert(memcmp(session->MBEDTLS_PRIVATE(id), data + ID_OFFSET + 1, 32) == 0);
    assert(memcmp(session->MBEDTLS_PRIVATE(master), data + ID_OFFSET + 33, 48) == 0);
    uint8_t saved[INPUT_MAX] = {0};
    size_t saved_length = 0;
    assert(mbedtls_ssl_session_save(session, saved, sizeof(saved), &saved_length) == 0);
    assert(saved_length == length && memcmp(saved, data, length) == 0);
}

typedef struct { int status; size_t calls; bool denied; } load_result;
static load_result load(const uint8_t *data, size_t length, size_t fault)
{
    assert(length <= INPUT_MAX);
    /* Exact-size input places truncation boundaries next to an ASan redzone.
     * A zero-length call still owns a valid, nonnull pointer. */
    uint8_t *copy = malloc(length == 0 ? 1 : length);
    assert(copy != NULL);
    memcpy(copy, data, length);
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    mbedtls_ssl_session session;
    mbedtls_ssl_session_init(&session);
    allocations = 0; fail_at = fault; armed = true;
    const int status = mbedtls_ssl_session_load(&session, copy, length);
    armed = false;
    const load_result result = {status, allocations, heap.denied};
    assert(memcmp(copy, data, length) == 0);
    if (status == 0) check_loaded(&session, data, length);
    if (status != 0) {
        /* The public loader must retire partial state before returning. */
        assert(heap.first == NULL && heap.used == 0 && heap.count == 0);
    }
    mbedtls_ssl_session_free(&session);
    assert(heap.first == NULL && heap.used == 0 && heap.count == 0);
    assert(zcl_tls_heap_clear(&heap));
    zcl_tls_heap_leave();
    free(copy);
    return result;
}

static void expect(const uint8_t *data, size_t length, int status)
{
    const load_result result = load(data, length, 0);
    if (result.status != status) {
        fprintf(stderr, "synthetic session: ID length=%u, bytes=%zu, expected=%d, actual=%d\n",
                (unsigned)data[ID_OFFSET], length, status, result.status);
    }
    assert(result.status == status && !result.denied);
}

static void id_lengths(bool digest)
{
    uint8_t data[INPUT_MAX];
    const size_t length = fixture(data, digest);
    for (unsigned id = 0; id <= UINT8_MAX; ++id) {
        data[ID_OFFSET] = (uint8_t)id;
        expect(data, length, id <= 32 ? 0 : MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
    }
}

static void truncations(bool digest)
{
    uint8_t data[INPUT_MAX];
    const size_t length = fixture(data, digest);
    for (size_t size = 0; size < length; ++size) {
        expect(data, size, MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
    }
    expect(data, length, 0);
    expect(data, length + 1, MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
}

static void malformed(void)
{
    static const struct { size_t at; uint8_t value; int error; } edits[] = {
        {0, 0, MBEDTLS_ERR_SSL_VERSION_MISMATCH},
        {HEADER_SIZE, 2, MBEDTLS_ERR_SSL_BAD_INPUT_DATA},
        {DIGEST_OFFSET, 255, MBEDTLS_ERR_SSL_BAD_INPUT_DATA},
        {DIGEST_OFFSET + 1, 31, MBEDTLS_ERR_SSL_BAD_INPUT_DATA},
        {DIGEST_OFFSET + 1, 33, MBEDTLS_ERR_SSL_BAD_INPUT_DATA}
    };
    uint8_t data[INPUT_MAX];
    for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i) {
        const size_t length = fixture(data, true);
        data[edits[i].at] = edits[i].value;
        expect(data, length, edits[i].error);
    }
}

static void allocation_failure(void)
{
    uint8_t data[INPUT_MAX];
    const size_t length = fixture(data, true);
    const load_result baseline = load(data, length, 0);
    assert(baseline.status == 0 && baseline.calls == 1 && !baseline.denied);
    const load_result failed = load(data, length, 1);
    assert(failed.status == MBEDTLS_ERR_SSL_ALLOC_FAILED && failed.calls == 1 && failed.denied);
    data[ID_OFFSET] = 33;
    const load_result rejected = load(data, length, 1);
    assert(rejected.status == MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
    assert(rejected.calls == 0 && !rejected.denied);
    data[ID_OFFSET] = 32;
    expect(data, length, 0);
}

int main(void)
{
    id_lengths(false);
    id_lengths(true);
    truncations(false);
    truncations(true);
    malformed();
    allocation_failure();
    puts("TLS sessions: 512 ID cases, truncations, round trips, allocation failure and cleanup pass");
    return 0;
}
