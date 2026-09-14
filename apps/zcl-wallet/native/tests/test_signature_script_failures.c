/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Provider definitions retain runtime NULL assertions, like upstream. */
#define SECP256K1_BUILD
#include "signature_internal.h"
#include <mbedtls/ripemd160.h>
#include <mbedtls/sha256.h>
#include <secp256k1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Script fault at %d\n", __LINE__); abort(); } } while (0)
static unsigned failure, calls, wipes;
static zcl_signature source;
static uint8_t digest[32], hash[20];
static const uint8_t *spans[5]; /* key, SHA, HASH160, DER, digest */
static const size_t sizes[5] = {33,32,20,70,32};

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void enter(unsigned stage)
{
    CHECK(stage >= 1 && stage <= 7 && calls == stage - 1);
    CHECK(calls == 0 || failure != calls);
    ++calls;
}

int secp256k1_ec_pubkey_parse(const secp256k1_context *context, secp256k1_pubkey *key,
    const unsigned char *input, size_t length)
{
    enter(1); CHECK(context == secp256k1_context_static && key != NULL && length == 33);
    CHECK(input != source.public_key && input[0] == 2);
    filled(input + 1, 32, 0x22); spans[0] = input;
    memset(key, 0x29, sizeof(*key));
    /* ALL metadata and spans must have copied before the first provider. */
    memset(&source, 0xff, sizeof(source)); source.der_len = SIZE_MAX;
    memset(digest, 0xff, sizeof(digest)); memset(hash, 0xff, sizeof(hash));
    return failure == 1 ? 0 : 1;
}

int mbedtls_sha256(const unsigned char *input, size_t length, unsigned char *output, int is224)
{
    enter(2); CHECK(input == spans[0] && length == 33 && is224 == 0);
    CHECK(input[0] == 2); filled(input + 1, 32, 0x22);
    filled(output, 32, 0); memset(output, 0x31, 32); spans[1] = output;
    return failure == 2 ? -1 : 0;
}

int mbedtls_ripemd160(const unsigned char *input, size_t length, unsigned char output[20])
{
    enter(3); CHECK(input == spans[1] && length == 32);
    filled(input, length, 0x31); filled(output, 20, 0);
    memset(output, failure == 10 ? 0x38 : 0x37, 20); spans[2] = output;
    return failure == 3 ? -1 : 0;
}

int secp256k1_ecdsa_signature_parse_der(const secp256k1_context *context, secp256k1_ecdsa_signature *parsed,
    const unsigned char *input, size_t length)
{
    enter(4); CHECK(context == secp256k1_context_static && parsed != NULL && length == 70);
    CHECK(input != source.der); filled(input, length, 0x6a); spans[3] = input;
    memset(parsed, 0x51, sizeof(*parsed));
    return failure == 4 ? 0 : 1;
}

int secp256k1_ecdsa_signature_normalize(const secp256k1_context *context,
    secp256k1_ecdsa_signature *output, const secp256k1_ecdsa_signature *input)
{
    enter(5); CHECK(context == secp256k1_context_static && output == NULL && input != NULL);
    return failure == 5 ? 1 : 0;
}

int secp256k1_ecdsa_signature_serialize_der(const secp256k1_context *context, unsigned char *output,
    size_t *length, const secp256k1_ecdsa_signature *input)
{
    enter(6); CHECK(context == secp256k1_context_static && input != NULL && length != NULL);
    CHECK(*length == 72); filled(output, *length, 0);
    memset(output, failure == 9 ? 0x6b : 0x6a, 72);
    *length = failure == 8 ? SIZE_MAX : 70;
    return failure == 6 ? 0 : 1;
}

int secp256k1_ecdsa_verify(const secp256k1_context *context, const secp256k1_ecdsa_signature *signature,
    const unsigned char *input, const secp256k1_pubkey *key)
{
    enter(7); CHECK(context == secp256k1_context_static && signature != NULL && key != NULL);
    CHECK(input != digest); filled(input, 32, 0x42); spans[4] = input;
    return failure == 7 ? 0 : 1;
}

void zcl_script_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && length >= sizeof(zcl_signature) + 104 + 2 * 64 + 72 + 107 + sizeof(size_t)
        && length <= 1024 && wipes++ == 0);
    memset(buffer, 0, length);
    for (size_t i = 0; i < 5; ++i) {
        if (spans[i] != NULL) filled(spans[i], sizes[i], 0);
        spans[i] = NULL; /* Inspect and retire only while work is live. */
    }
}

static zcl_status expected(unsigned mode)
{
    if (mode == 0) return ZCL_OK;
    if (mode == 15) return ZCL_BUFFER_TOO_SMALL;
    return mode == 2 || mode == 3 || mode == 6 || mode == 7 || mode == 10
        ? ZCL_CRYPTO_FAILURE : ZCL_INVALID_ENCODING;
}

static void run(unsigned mode)
{
    static const unsigned stages[] = {7,1,2,3,4,5,6,7,6,6,3,0,0,0,0,0};
    CHECK(mode < sizeof(stages) / sizeof(stages[0]));
    failure = mode; calls = wipes = 0; memset(spans, 0, sizeof(spans));
    memset(&source, 0, sizeof(source)); memset(source.der, 0x6a, 72); source.der_len = 70;
    memset(source.public_key, 0x22, 33); source.public_key[0] = 2;
    memset(digest, 0x42, sizeof(digest)); memset(hash, 0x37, sizeof(hash));
    if (mode == 11) source.public_key[0] = 4;
    if (mode == 12) source.der_len = 7;
    if (mode == 13) source.der_len = 72;
    if (mode == 14) source.der_len = SIZE_MAX;
    struct { uint8_t before[8], output[128], after[8]; } box;
    memset(&box, 0xa5, sizeof(box)); size_t length = SIZE_MAX;
    CHECK(zcl_signature_p2pkh(&source, digest, 32, hash, 20, box.output,
        mode == 15 ? 105 : sizeof(box.output), &length) == expected(mode));
    CHECK(calls == stages[mode] && wipes == 1);
    for (size_t i = 0; i < 5; ++i) CHECK(spans[i] == NULL);
    filled(box.before, 8, 0xa5); filled(box.after, 8, 0xa5);
    if (mode != 0) { CHECK(length == SIZE_MAX); filled(box.output, 128, 0xa5); return; }
    CHECK(length == 106 && box.output[0] == 71 && box.output[71] == 1 && box.output[72] == 33);
    filled(box.output + 1, 70, 0x6a); CHECK(box.output[73] == 2);
    filled(box.output + 74, 32, 0x22); filled(box.output + 106, 22, 0xa5);
}

int main(void)
{
    for (unsigned mode = 0; mode < 16; ++mode) run(mode);
    calls = wipes = 0;
    uint8_t output[128]; size_t length = 0;
    CHECK(zcl_signature_p2pkh(NULL, digest, 32, hash, 20, output, 128, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_p2pkh(&source, digest, SIZE_MAX, hash, 20, output, 128, &length) == ZCL_OUT_OF_RANGE);
    CHECK(calls == 0 && wipes == 0);
    CHECK(puts("Script provider failures preserve both outputs, own all inputs and clear live work") >= 0);
    return 0;
}
