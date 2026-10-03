/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* This translation unit IMPLEMENTS substituted provider functions. Match the
 * provider's own build mode so caller-only nonnull attributes cannot optimize
 * out its runtime assertions. Production callers retain their normal header. */
#define SECP256K1_BUILD
#include "signature_internal.h"
#include "ec_context.h"
#include <secp256k1_preallocated.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Substituted providers belong only to this test and signature.c. Context
 * construction/destruction remains real, with wrapped allocation failures. */
#undef zcl_secure_zero
void zcl_secure_zero(void *buffer, size_t length);
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Signature fault at %d\n", __LINE__); abort(); } } while (0)
static unsigned failure, calls, random_calls, work_wipes, nonce_calls, nonce_wipes;
static unsigned blinding_wipes, secret_wipes;
static uint8_t caller_secret[32], caller_digest[32];
static const uint8_t *spans[3]; /* blinding, secret, digest in the live work */
static uint8_t *nonce_span;
static void *owned;
static size_t owned_len, allocations, releases;
void *__real_malloc(size_t size);
void __real_free(void *pointer);
size_t __real_secp256k1_context_preallocated_size(unsigned int flags);
secp256k1_context *__real_secp256k1_context_preallocated_create(void *storage, unsigned int flags);
int __real_secp256k1_context_randomize(secp256k1_context *context, const unsigned char *seed);

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

void *__wrap_malloc(size_t length)
{
    CHECK(owned == NULL && length > 0 && length <= 1024);
    if (failure == 12) return NULL;
    void *pointer = __real_malloc(length);
    CHECK(pointer != NULL);
    owned = pointer; owned_len = length; ++allocations;
    return pointer;
}

void __wrap_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(pointer == owned);
        filled(pointer, owned_len, 0);
        owned = NULL; owned_len = 0; ++releases;
    }
    __real_free(pointer);
}

size_t __wrap_secp256k1_context_preallocated_size(unsigned int flags)
{
    return failure == 13 ? SIZE_MAX : __real_secp256k1_context_preallocated_size(flags);
}

secp256k1_context *__wrap_secp256k1_context_preallocated_create(void *storage, unsigned int flags)
{
    return failure == 14 ? NULL : __real_secp256k1_context_preallocated_create(storage, flags);
}

int __wrap_secp256k1_context_randomize(secp256k1_context *context, const unsigned char *seed)
{
    CHECK(seed == spans[0]);
    filled(seed, 32, 0x37);
    return failure == 15 ? 0 : __real_secp256k1_context_randomize(context, seed);
}

zcl_status zcl_random_bytes(uint8_t *output, size_t length)
{
    CHECK(length == 32 && random_calls++ == 0 && spans[0] == NULL);
    filled(output, length, 0);
    spans[0] = output;
    memset(output, 0x37, length); /* Dirty even on failure. */
    /* The wrapper must already own BOTH inputs before its first OS operation. */
    memset(caller_secret, 0xff, sizeof(caller_secret));
    memset(caller_digest, 0xff, sizeof(caller_digest));
    return failure == 11 ? ZCL_IO_FAILURE : ZCL_OK;
}

static void enter(unsigned stage)
{
    CHECK(stage >= 1 && stage <= 10 && calls == stage - 1);
    CHECK(calls == 0 || failure != calls); /* A failed provider must stop here. */
    ++calls;
}

static void private_secret(const uint8_t *secret)
{
    CHECK(secret != caller_secret);
    filled(secret, 31, 0);
    CHECK(secret[31] == 1);
    spans[1] = secret;
}

int secp256k1_ec_pubkey_create(const secp256k1_context *context, secp256k1_pubkey *key,
    const unsigned char *secret)
{
    enter(1); CHECK(context != NULL && key != NULL);
    CHECK(blinding_wipes == 1 && spans[0] == NULL);
    private_secret(secret);
    memset(key, 0x6a, sizeof(*key));
    return failure == 1 ? 0 : 1;
}

static int nonce_provider(unsigned char *nonce, const unsigned char *digest, const unsigned char *secret,
    const unsigned char *algorithm, void *data, unsigned int attempt)
{
    CHECK(algorithm == NULL && data == NULL && attempt < 8 && nonce != NULL);
    CHECK(secret == spans[1] && digest == spans[2]);
    ++nonce_calls;
    memset(nonce, 0x6a, 32);
    return failure == 19 ? 0 : 1;
}

const secp256k1_nonce_function secp256k1_nonce_function_rfc6979 = nonce_provider;

static void nonce_refusals(secp256k1_nonce_function callback, const uint8_t *digest, const uint8_t *secret)
{
    uint8_t nonce[32], algorithm[16] = {0}, extra[32] = {0};
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const unsigned before = nonce_calls;
        CHECK(callback(nonce, digest, secret, NULL, NULL, attempt) == 1);
        CHECK(nonce_calls == before + 1);
        filled(nonce, sizeof(nonce), 0x6a);
        zcl_secure_zero(nonce, sizeof(nonce));
    }
    for (unsigned which = 0; which < 4; ++which) {
        memset(nonce, 0x42, sizeof(nonce));
        nonce_span = nonce;
        const unsigned before = nonce_calls;
        const unsigned attempt = which == 0 ? 8 : which == 1 ? UINT_MAX : 0;
        CHECK(callback(nonce, digest, secret, which == 2 ? algorithm : NULL,
            which == 3 ? extra : NULL, attempt) == 0);
        CHECK(nonce_calls == before && nonce_span == NULL);
        filled(nonce, sizeof(nonce), 0);
    }
}

int secp256k1_ecdsa_sign(const secp256k1_context *context, secp256k1_ecdsa_signature *signature,
    const unsigned char *digest, const unsigned char *secret, secp256k1_nonce_function callback, const void *data)
{
    enter(2); CHECK(context != NULL && signature != NULL && callback != NULL && data == NULL);
    CHECK(secret == spans[1] && digest != caller_digest);
    filled(digest, 32, 0x42); spans[2] = digest;
    memset(signature, 0x73, sizeof(*signature));
    if (failure == 2) return 0;
    uint8_t nonce[32] = {0};
    if (failure == 19) nonce_span = nonce;
    const int result = callback(nonce, digest, secret, NULL, NULL, 0);
    if (failure == 19) {
        CHECK(result == 0 && nonce_span == NULL);
        filled(nonce, sizeof(nonce), 0);
        return 0;
    }
    CHECK(result == 1);
    filled(nonce, sizeof(nonce), 0x6a);
    zcl_secure_zero(nonce, sizeof(nonce));
    if (failure == 0) nonce_refusals(callback, digest, secret);
    return 1;
}

int secp256k1_ecdsa_signature_normalize(const secp256k1_context *context, secp256k1_ecdsa_signature *output,
    const secp256k1_ecdsa_signature *input)
{
    const unsigned stage = calls == 2 ? 3 : 9;
    enter(stage); CHECK(context != NULL && output == NULL && input != NULL);
    CHECK(secret_wipes == 1 && spans[1] == NULL);
    CHECK(owned == NULL && allocations == releases && context == secp256k1_context_static);
    return failure == stage ? 1 : 0;
}

int secp256k1_ec_pubkey_serialize(const secp256k1_context *context, unsigned char *output, size_t *length,
    const secp256k1_pubkey *key, unsigned int flags)
{
    enter(4); CHECK(context != NULL && key != NULL && output != NULL && length != NULL);
    CHECK(*length == 33 && flags == SECP256K1_EC_COMPRESSED);
    filled(output, *length, 0);
    memset(output, 0x22, 33);
    *length = failure == 16 ? SIZE_MAX : 33;
    return failure == 4 ? 0 : 1;
}

int secp256k1_ecdsa_signature_serialize_der(const secp256k1_context *context, unsigned char *output,
    size_t *length, const secp256k1_ecdsa_signature *signature)
{
    enter(5); CHECK(context != NULL && signature != NULL && output != NULL && length != NULL);
    CHECK(*length == 72);
    filled(output, *length, 0);
    memset(output, 0x6a, 72); /* Includes unused bytes; wrapper must clear them. */
    *length = failure == 17 ? 7 : failure == 18 ? SIZE_MAX : 70;
    return failure == 5 ? 0 : 1;
}

int secp256k1_ec_pubkey_parse(const secp256k1_context *context, secp256k1_pubkey *key,
    const unsigned char *input, size_t length)
{
    enter(6); CHECK(context != NULL && key != NULL && length == 33);
    filled(input, length, 0x22);
    memset(key, 0x6a, sizeof(*key));
    return failure == 6 ? 0 : 1;
}

int secp256k1_ec_pubkey_cmp(const secp256k1_context *context, const secp256k1_pubkey *left,
    const secp256k1_pubkey *right)
{
    enter(7); CHECK(context != NULL && left != NULL && right != NULL && left != right);
    return failure == 7 ? 1 : 0;
}

int secp256k1_ecdsa_signature_parse_der(const secp256k1_context *context, secp256k1_ecdsa_signature *signature,
    const unsigned char *input, size_t length)
{
    enter(8); CHECK(context != NULL && signature != NULL && length == 70);
    filled(input, length, 0x6a);
    memset(signature, 0x73, sizeof(*signature));
    return failure == 8 ? 0 : 1;
}

int secp256k1_ecdsa_verify(const secp256k1_context *context, const secp256k1_ecdsa_signature *signature,
    const unsigned char *digest, const secp256k1_pubkey *key)
{
    enter(10); CHECK(context != NULL && signature != NULL && key != NULL && digest == spans[2]);
    filled(digest, 32, 0x42);
    return failure == 10 ? 0 : 1;
}

static void retire_input(void *buffer)
{
    if (buffer == spans[0]) {
        CHECK(blinding_wipes++ == 0 && calls == 0);
        memset(buffer, 0, 32);
        filled(buffer, 32, 0);
        spans[0] = NULL;
        return;
    }
    CHECK(blinding_wipes == 1 && secret_wipes++ == 0);
    /* Early RNG/context failures never expose the copied scalar to a provider.
     * Observe that live argument here, without reconstructing a work offset. */
    CHECK(buffer == spans[1] || (spans[1] == NULL && calls == 0));
    private_secret(buffer);
    memset(buffer, 0, 32);
    filled(buffer, 32, 0);
    spans[1] = NULL;
}

void zcl_signature_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    if (buffer == nonce_span) {
        CHECK(length == 32);
        memset(buffer, 0, length);
        nonce_span = NULL; ++nonce_wipes;
        return;
    }
    if (length == 32) { retire_input(buffer); return; }
    CHECK(length >= sizeof(zcl_ec_context) + 96 + 2 * sizeof(secp256k1_pubkey)
        + 2 * sizeof(secp256k1_ecdsa_signature) + sizeof(zcl_signature) && length <= 1024);
    CHECK(work_wipes++ == 0 && owned == NULL && allocations == releases);
    CHECK(blinding_wipes == 1 && secret_wipes == 1);
    memset(buffer, 0, length);
    for (size_t i = 0; i < 3; ++i) {
        if (spans[i] != NULL) filled(spans[i], 32, 0);
        spans[i] = NULL; /* Retire while the whole work is still live. */
    }
}

static void run(unsigned mode)
{
    static const unsigned stages[] = {10,1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,4,5,5,2};
    CHECK(mode < sizeof(stages) / sizeof(stages[0]) && owned == NULL && nonce_span == NULL);
    failure = mode; calls = 0; random_calls = 0; work_wipes = 0; nonce_calls = 0; nonce_wipes = 0;
    blinding_wipes = secret_wipes = 0;
    allocations = releases = 0;
    memset(spans, 0, sizeof(spans));
    memset(caller_secret, 0, sizeof(caller_secret)); caller_secret[31] = 1;
    memset(caller_digest, 0x42, sizeof(caller_digest));
    zcl_signature output, before;
    memset(&output, 0xa5, sizeof(output)); memcpy(&before, &output, sizeof(before));
    zcl_status expected = mode == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE;
    if (mode == 11) expected = ZCL_IO_FAILURE;
    if (mode == 12) expected = ZCL_RESOURCE_EXHAUSTED;
    CHECK(zcl_signature_create(caller_secret, 32, caller_digest, 32, &output) == expected);
    CHECK(calls == stages[mode] && random_calls == 1 && work_wipes == 1);
    CHECK(blinding_wipes == 1 && secret_wipes == 1);
    CHECK(owned == NULL && allocations == releases && nonce_span == NULL);
    CHECK(nonce_wipes == (mode == 0 ? 4U : mode == 19 ? 1U : 0U));
    for (size_t i = 0; i < 3; ++i) CHECK(spans[i] == NULL);
    if (mode != 0) { CHECK(memcmp(&before, &output, sizeof(output)) == 0); return; }
    CHECK(output.der_len == 70);
    filled(output.der, 70, 0x6a); filled(output.der + 70, 2, 0);
    filled(output.public_key, 33, 0x22);
}

int main(void)
{
    for (unsigned mode = 0; mode < 20; ++mode) run(mode);
    calls = random_calls = work_wipes = blinding_wipes = secret_wipes = 0;
    zcl_signature output;
    CHECK(zcl_signature_create(caller_secret, SIZE_MAX, caller_digest, 32, &output) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_signature_create(NULL, 32, caller_digest, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(calls == 0 && random_calls == 0 && work_wipes == 0 && owned == NULL);
    CHECK(blinding_wipes == 0 && secret_wipes == 0);
    CHECK(puts("Signing provider failures preserve output, bound nonce callbacks and clear live secret/context storage") >= 0);
    return 0;
}
