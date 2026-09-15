/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* These wrappers implement provider entry points. Disable caller-only nonnull
 * assumptions in this test so assertions remain executable under optimization. */
#define SECP256K1_BUILD
#include "ec_context.h"
#include <secp256k1_preallocated.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "EC lifetime check at %d: %s\n", __LINE__, #v); abort(); } } while (0)

typedef enum { NORMAL, NO_MEMORY, EMPTY_SIZE, HUGE_SIZE, OVER_LIMIT, MAX_SIZE,
    NO_CONTEXT, NO_BLINDING, NO_POINT, NO_ENCODING, SHORT_ENCODING, LONG_ENCODING } fault;
static fault failure;
static void *storage;
static size_t storage_len, allocations, releases, creates, destroys, blindings;
static size_t storage_wipes, owner_wipes, points, encodings, point_wipes, encoding_wipes;
/* Integer identities cannot dangle when testing a missing stack cleanup.
 * Inspect bytes only through a live provider/zeroization argument. */
static uintptr_t handle_id, point_id, encoding_id;

void *__real_malloc(size_t);
void __real_free(void *);
void __real_zcl_secure_zero(void *, size_t);
size_t __real_secp256k1_context_preallocated_size(unsigned int);
secp256k1_context *__real_secp256k1_context_preallocated_create(void *, unsigned int);
void __real_secp256k1_context_preallocated_destroy(secp256k1_context *);
int __real_secp256k1_context_randomize(secp256k1_context *, const unsigned char *);
int __real_secp256k1_ec_pubkey_create(const secp256k1_context *, secp256k1_pubkey *, const unsigned char *);
int __real_secp256k1_ec_pubkey_serialize(const secp256k1_context *, unsigned char *, size_t *,
    const secp256k1_pubkey *, unsigned int);

static void filled(const void *span, size_t length, uint8_t value)
{
    const uint8_t *bytes = span;
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

void *__wrap_malloc(size_t length)
{
    CHECK(storage == NULL && length > 0 && length <= 1024);
    if (failure == NO_MEMORY) return NULL;
    storage = __real_malloc(length);
    CHECK(storage != NULL);
    storage_len = length;
    memset(storage, 0x6a, length); /* Include bytes outside provider state. */
    ++allocations;
    return storage;
}

void __wrap_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(pointer == storage && handle_id == 0);
        CHECK(storage_wipes == allocations && destroys == creates);
        filled(pointer, storage_len, 0);
        storage = NULL; storage_len = 0; ++releases;
    }
    __real_free(pointer);
}

size_t __wrap_secp256k1_context_preallocated_size(unsigned int flags)
{
    CHECK(flags == SECP256K1_CONTEXT_NONE);
    if (failure == EMPTY_SIZE) return 0;
    if (failure == HUGE_SIZE) return SIZE_MAX;
    if (failure == OVER_LIMIT) return 1025;
    const size_t needed = __real_secp256k1_context_preallocated_size(flags);
    CHECK(needed > 0 && needed <= 1024);
    return failure == MAX_SIZE ? 1024 : needed;
}

secp256k1_context *__wrap_secp256k1_context_preallocated_create(void *memory, unsigned int flags)
{
    CHECK(memory == storage && storage != NULL && handle_id == 0);
    CHECK(flags == SECP256K1_CONTEXT_NONE && allocations == releases + 1);
    if (failure == NO_CONTEXT) {
        memset(memory, 0x73, storage_len);
        return NULL;
    }
    secp256k1_context *context = __real_secp256k1_context_preallocated_create(memory, flags);
    CHECK(context != NULL);
    handle_id = (uintptr_t)context;
    ++creates;
    return context;
}

int __wrap_secp256k1_context_randomize(secp256k1_context *context, const unsigned char *seed)
{
    CHECK(context != NULL && (uintptr_t)context == handle_id && seed != NULL);
    ++blindings;
    /* Even a refused provider may have written its entire state. */
    const int result = __real_secp256k1_context_randomize(context, seed);
    CHECK(result == 1);
    return failure == NO_BLINDING ? 0 : result;
}

void __wrap_secp256k1_context_preallocated_destroy(secp256k1_context *context)
{
    CHECK(context != NULL && (uintptr_t)context == handle_id && storage != NULL);
    CHECK(point_id == 0 && encoding_id == 0 && creates == destroys + 1);
    CHECK(storage_wipes == releases); /* Destroy before application erasure. */
    __real_secp256k1_context_preallocated_destroy(context);
    handle_id = 0; ++destroys;
    /* Provider destruction does not replace the application's full-allocation
     * wipe. Storage still belongs to malloc until the observed free below. */
    memset(storage, 0x37, storage_len);
}

int __wrap_secp256k1_ec_pubkey_create(const secp256k1_context *context,
    secp256k1_pubkey *point, const unsigned char *secret)
{
    CHECK(context != NULL && (uintptr_t)context == handle_id && point != NULL && secret != NULL);
    CHECK(failure != NO_BLINDING);
    CHECK(point_id == 0 && encoding_id == 0);
    point_id = (uintptr_t)point; ++points;
    if (failure == NO_POINT) { memset(point, 0x6a, sizeof(*point)); return 0; }
    return __real_secp256k1_ec_pubkey_create(context, point, secret);
}

int __wrap_secp256k1_ec_pubkey_serialize(const secp256k1_context *context,
    unsigned char *output, size_t *length, const secp256k1_pubkey *point, unsigned int flags)
{
    CHECK(context != NULL && (uintptr_t)context == handle_id && (uintptr_t)point == point_id);
    CHECK(failure != NO_POINT);
    CHECK(output != NULL && length != NULL && *length == 33 && flags == SECP256K1_EC_COMPRESSED);
    CHECK(encoding_id == 0);
    encoding_id = (uintptr_t)output; ++encodings;
    if (failure == NO_ENCODING) { memset(output, 0x73, 33); return 0; }
    const int result = __real_secp256k1_ec_pubkey_serialize(context, output, length, point, flags);
    if (failure == SHORT_ENCODING) *length = 32;
    if (failure == LONG_ENCODING) *length = SIZE_MAX;
    return result;
}

void __wrap_zcl_secure_zero(void *buffer, size_t length)
{
    const uintptr_t identity = (uintptr_t)buffer;
    if (buffer != NULL && buffer == storage) {
        CHECK(length == storage_len && handle_id == 0 && creates == destroys);
        CHECK(storage_wipes == releases);
        ++storage_wipes;
    }
    if (identity != 0 && identity == point_id) {
        CHECK(length == sizeof(secp256k1_pubkey));
        point_id = 0; ++point_wipes;
    }
    if (identity != 0 && identity == encoding_id) {
        CHECK(length == 33);
        encoding_id = 0; ++encoding_wipes;
    }
    if (length == sizeof(zcl_ec_context)) {
        CHECK(storage == NULL && handle_id == 0 && allocations == releases);
        ++owner_wipes;
    }
    __real_zcl_secure_zero(buffer, length);
    if (buffer != NULL) filled(buffer, length, 0);
}

static void reset(fault mode)
{
    CHECK(storage == NULL && handle_id == 0 && point_id == 0 && encoding_id == 0);
    CHECK(allocations == releases && creates == destroys);
    failure = mode;
    allocations = releases = creates = destroys = blindings = 0;
    storage_wipes = owner_wipes = points = encodings = point_wipes = encoding_wipes = 0;
}

static void finished(void)
{
    CHECK(storage == NULL && handle_id == 0 && point_id == 0 && encoding_id == 0);
    CHECK(allocations == releases && storage_wipes == releases && creates == destroys);
    CHECK(blindings == creates && owner_wipes == 1 && point_wipes == points && encoding_wipes == encodings);
}

static void public_case(fault mode)
{
    static const uint8_t expected[33] = {0x02,0x79,0xbe,0x66,0x7e,0xf9,0xdc,0xbb,0xac,
        0x55,0xa0,0x62,0x95,0xce,0x87,0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,
        0xd9,0x59,0xf2,0x81,0x5b,0x16,0xf8,0x17,0x98}; /* Published generator, scalar1. */
    uint8_t secret[32] = {0}, blinding[32] = {1}, output[35];
    secret[31] = 1;
    memset(output, 0xa5, sizeof(output));
    reset(mode);
    zcl_status wanted = mode == NORMAL || mode == MAX_SIZE ? ZCL_OK : ZCL_CRYPTO_FAILURE;
    if (mode == NO_MEMORY) wanted = ZCL_RESOURCE_EXHAUSTED;
    CHECK(zcl_public_key(secret, 32, blinding, 32, output + 1, 33) == wanted);
    finished();
    if (wanted == ZCL_OK) CHECK(memcmp(output + 1, expected, sizeof(expected)) == 0);
    else filled(output, sizeof(output), 0xa5);
    CHECK(output[0] == 0xa5 && output[34] == 0xa5);
    filled(secret, 31, 0); CHECK(secret[31] == 1);
    CHECK(blinding[0] == 1); filled(blinding + 1, 31, 0);
    __real_zcl_secure_zero(secret, sizeof(secret));
    __real_zcl_secure_zero(blinding, sizeof(blinding));
}

static void context_lifetime(void)
{
    uint8_t blinding[32] = {1};
    zcl_ec_context context = {0};
    reset(NORMAL);
    CHECK(zcl_ec_begin(&context, blinding, 32) == ZCL_OK);
    CHECK(context.storage == storage && context.storage_len == storage_len && (uintptr_t)context.handle == handle_id);
    /* A live owner must refuse a second begin without losing the first owner. */
    CHECK(zcl_ec_begin(&context, blinding, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(allocations == 1 && creates == 1 && blindings == 1);
    zcl_ec_end(&context);
    CHECK(context.storage == NULL && context.handle == NULL && context.storage_len == 0);
    finished();
    /* Cleared-owner cleanup and reuse must not double-destroy or double-free. */
    zcl_ec_end(&context);
    CHECK(owner_wipes == 2 && releases == 1 && destroys == 1);
    reset(NORMAL);
    CHECK(zcl_ec_begin(&context, blinding, 32) == ZCL_OK);
    zcl_ec_end(&context);
    finished();
    __real_zcl_secure_zero(blinding, sizeof(blinding));
}

static void invalid_context(void)
{
    static const size_t lengths[] = {0, 1, 31, 33, SIZE_MAX};
    uint8_t blinding[32] = {1};
    zcl_ec_context context = {0};
    reset(NORMAL);
    CHECK(zcl_ec_begin(NULL, blinding, 32) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_ec_begin(&context, NULL, 32) == ZCL_INVALID_ARGUMENT);
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        CHECK(zcl_ec_begin(&context, blinding, lengths[i]) == ZCL_INVALID_ARGUMENT);
    CHECK(allocations == 0 && creates == 0 && blindings == 0);
    CHECK(context.storage == NULL && context.handle == NULL && context.storage_len == 0);
    zcl_ec_end(NULL);
    CHECK(owner_wipes == 0);
    __real_zcl_secure_zero(blinding, sizeof(blinding));
}

int main(void)
{
    for (fault mode = NORMAL; mode <= LONG_ENCODING; mode = (fault)((unsigned)mode + 1U)) public_case(mode);
    context_lifetime();
    invalid_context();
    CHECK(puts("EC context destruction, full allocation/scratch erasure, refused provider output and owner reuse checked") >= 0);
    return 0;
}
