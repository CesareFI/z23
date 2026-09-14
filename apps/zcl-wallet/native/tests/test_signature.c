/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_internal.h"
#include "ec_context.h"
#include "zcl_keys.h"
#ifdef ZCL_SIGNATURE_ORACLE
#include "signature_oracle.h"
#endif
#include <secp256k1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Signature check at %d\n", __LINE__); abort(); } } while (0)
static const uint8_t order[32] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
    0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41
};
static const uint8_t generator[33] = {
    0x02,0x79,0xbe,0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,0x0b,0x07,
    0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,0x81,0x5b,0x16,0xf8,0x17,0x98
};

static size_t der_integer(const uint8_t *der, size_t length, size_t offset)
{
    CHECK(offset <= length && length - offset >= 3);
    CHECK(der[offset] == 2);
    const size_t count = der[offset + 1];
    CHECK(count >= 1 && count <= 33 && count <= length - offset - 2);
    const uint8_t *number = der + offset + 2;
    CHECK((number[0] & 0x80U) == 0);
    if (count > 1 && number[0] == 0) CHECK((number[1] & 0x80U) != 0);
    return offset + 2 + count;
}

static void default_signature(const uint8_t *secret, const uint8_t *digest, const zcl_signature *expected)
{
    zcl_ec_context context = {0};
    uint8_t blinding[32] = {1}, encoded[72] = {0};
    secp256k1_ecdsa_signature signature = {{0}};
    size_t length = sizeof(encoded);
    int okay = 0;
    const zcl_status status = zcl_ec_begin(&context, blinding, sizeof(blinding));
    if (status == ZCL_OK && secp256k1_ecdsa_sign(context.handle, &signature, digest, secret, NULL, NULL) == 1)
        okay = secp256k1_ecdsa_signature_serialize_der(context.handle, encoded, &length, &signature);
    zcl_ec_end(&context);
    CHECK(status == ZCL_OK && okay == 1 && length <= sizeof(encoded));
    CHECK(length == expected->der_len && memcmp(encoded, expected->der, length) == 0);
}

static void verify(const uint8_t *secret, const uint8_t *digest, const zcl_signature *signature)
{
    CHECK(signature->der_len >= 8 && signature->der_len <= 71); /* Low-S needs at most32 bytes. */
    CHECK(signature->der[0] == 0x30 && signature->der[1] == signature->der_len - 2);
    const size_t end_r = der_integer(signature->der, signature->der_len, 2);
    CHECK(der_integer(signature->der, signature->der_len, end_r) == signature->der_len);
    for (size_t i = signature->der_len; i < 72; ++i) CHECK(signature->der[i] == 0);
    secp256k1_pubkey key = {{0}};
    secp256k1_ecdsa_signature parsed = {{0}};
    CHECK(secp256k1_ec_pubkey_parse(secp256k1_context_static, &key, signature->public_key, 33) == 1);
    CHECK(secp256k1_ecdsa_signature_parse_der(secp256k1_context_static, &parsed, signature->der, signature->der_len) == 1);
    CHECK(secp256k1_ecdsa_signature_normalize(secp256k1_context_static, NULL, &parsed) == 0);
    CHECK(secp256k1_ecdsa_verify(secp256k1_context_static, &parsed, digest, &key) == 1);
    uint8_t blinding[32] = {1}, expected[33] = {0}, altered[32];
    CHECK(zcl_public_key(secret, 32, blinding, 32, expected, 33) == ZCL_OK);
    CHECK(memcmp(expected, signature->public_key, 33) == 0);
    memcpy(altered, digest, sizeof(altered));
    altered[0] ^= 1;
    CHECK(secp256k1_ecdsa_verify(secp256k1_context_static, &parsed, altered, &key) == 0);
    default_signature(secret, digest, signature);
#ifdef ZCL_SIGNATURE_ORACLE
    CHECK(zcl_test_signature_oracle(secret, 32, digest, 32, signature) == 1);
    CHECK(zcl_test_signature_oracle(secret, 32, altered, 32, signature) == 0);
#endif
}

static void repeated(uint8_t scalar, uint8_t pattern)
{
    uint8_t secret[32] = {0}, digest[32];
    secret[31] = scalar;
    memset(digest, pattern, sizeof(digest));
    struct { uint8_t before[8]; zcl_signature value; uint8_t after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    CHECK(zcl_signature_create(secret, 32, digest, 32, &box.value) == ZCL_OK);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    verify(secret, digest, &box.value);
    if (scalar == 1) CHECK(memcmp(box.value.public_key, generator, sizeof(generator)) == 0);
    zcl_signature again;
    memset(&again, 0x42, sizeof(again));
    CHECK(zcl_signature_create(secret, 32, digest, 32, &again) == ZCL_OK);
    CHECK(memcmp(&again, &box.value, sizeof(again)) == 0); /* Fresh OS blinding, same RFC6979 result. */
    for (size_t i = 0; i < 31; ++i) CHECK(secret[i] == 0);
    CHECK(secret[31] == scalar);
    for (size_t i = 0; i < 32; ++i) CHECK(digest[i] == pattern);
    zcl_secure_zero(secret, sizeof(secret));
}

static void scalar_edges(void)
{
    uint8_t secret[32] = {0}, digest[32] = {0};
    zcl_signature signature, saved;
    memset(&signature, 0xa5, sizeof(signature));
    memcpy(&saved, &signature, sizeof(saved));
    CHECK(zcl_signature_create(secret, 32, digest, 32, &signature) == ZCL_CRYPTO_FAILURE);
    CHECK(memcmp(&signature, &saved, sizeof(saved)) == 0);
    memcpy(secret, order, sizeof(secret));
    CHECK(zcl_signature_create(secret, 32, digest, 32, &signature) == ZCL_CRYPTO_FAILURE);
    CHECK(memcmp(&signature, &saved, sizeof(saved)) == 0);
    memset(secret, 0xff, sizeof(secret));
    CHECK(zcl_signature_create(secret, 32, digest, 32, &signature) == ZCL_CRYPTO_FAILURE);
    CHECK(memcmp(&signature, &saved, sizeof(saved)) == 0);
    memcpy(secret, order, sizeof(secret));
    --secret[31];
    CHECK(zcl_signature_create(secret, 32, digest, 32, &signature) == ZCL_OK);
    verify(secret, digest, &signature);
    memcpy(&saved, &signature, sizeof(saved));
    memcpy(digest, order, sizeof(digest)); /* RFC6979/ECDSA reduce the message modulo n. */
    CHECK(zcl_signature_create(secret, 32, digest, 32, &signature) == ZCL_OK);
    CHECK(memcmp(&signature, &saved, sizeof(saved)) == 0);
    verify(secret, digest, &signature);
    zcl_secure_zero(secret, sizeof(secret));
}

static void arguments(void)
{
    uint8_t secret[32] = {0}, digest[32] = {0};
    secret[31] = 1;
    zcl_signature output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    CHECK(zcl_signature_create(NULL, 32, digest, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_create(secret, 32, NULL, 32, &output) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_create(secret, 32, digest, 32, NULL) == ZCL_INVALID_ARGUMENT);
    for (size_t length = 0; length <= 64; ++length) {
        if (length == 32) continue;
        CHECK(zcl_signature_create(secret, length, digest, 32, &output) == ZCL_OUT_OF_RANGE);
        CHECK(zcl_signature_create(secret, 32, digest, length, &output) == ZCL_OUT_OF_RANGE);
        CHECK(memcmp(&output, &before, sizeof(before)) == 0);
    }
    CHECK(zcl_signature_create(secret, SIZE_MAX, digest, 32, &output) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_signature_create(secret, 32, digest, SIZE_MAX, &output) == ZCL_OUT_OF_RANGE);
    CHECK(memcmp(&output, &before, sizeof(before)) == 0);
    zcl_secure_zero(secret, sizeof(secret));
}

int main(void)
{
    for (uint8_t scalar = 1; scalar <= 16; ++scalar) {
        repeated(scalar, 0);
        repeated(scalar, 1);
        repeated(scalar, 0x7f);
        repeated(scalar, 0xff);
    }
    scalar_edges();
    arguments();
    CHECK(puts("Synthetic ECDSA output is deterministic, verified, strict DER/low-S and failure-atomic") >= 0);
    return 0;
}
