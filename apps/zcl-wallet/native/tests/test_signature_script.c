/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_internal.h"
#include "zcl_keys.h"
#ifdef ZCL_SIGNATURE_ORACLE
#include "signature_oracle.h"
#endif
#include <mbedtls/ripemd160.h>
#include <mbedtls/sha256.h>
#include <secp256k1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Signature script at %d\n", __LINE__); abort(); } } while (0)
static const uint8_t generator_hash[20] = {
    0x75,0x1e,0x76,0xe8,0x19,0x91,0x96,0xd4,0x54,0x94,
    0x1c,0x45,0xd1,0xb3,0xa3,0x23,0xf1,0x43,0x3b,0xd6
};
static uint8_t digest[32], key_hash[20];
static zcl_signature signature;

static void setup(uint8_t scalar, uint8_t pattern)
{
    uint8_t secret[32] = {0}, sha[32] = {0};
    secret[31] = scalar;
    memset(digest, pattern, sizeof(digest));
    CHECK(zcl_signature_create(secret, sizeof(secret), digest, sizeof(digest), &signature) == ZCL_OK);
    zcl_secure_zero(secret, sizeof(secret));
    CHECK(mbedtls_sha256(signature.public_key, 33, sha, 0) == 0);
    CHECK(mbedtls_ripemd160(sha, sizeof(sha), key_hash) == 0);
    if (scalar == 1) CHECK(memcmp(key_hash, generator_hash, sizeof(key_hash)) == 0);
}

/* Parse the two direct pushes independently, including exact consumption and
 * hash-type byte. Script length includes neither a count nor a terminator. */
static void script_matches(const uint8_t *script, size_t length)
{
    CHECK(length >= 44 && length <= 107);
    const size_t first = script[0];
    CHECK(first >= 9 && first <= 72 && first < length);
    CHECK(script[first] == 1 && first - 1 == signature.der_len);
    CHECK(memcmp(script + 1, signature.der, first - 1) == 0);
    const size_t second = first + 1;
    CHECK(second < length && script[second] == 33 && length - second - 1 == 33);
    CHECK(memcmp(script + second + 1, signature.public_key, 33) == 0);
#ifdef ZCL_SIGNATURE_ORACLE
    CHECK(zcl_test_signature_script_oracle(&signature, digest, 32, key_hash, 20) == 1);
#endif
}

static void call(size_t capacity, zcl_status expected)
{
    struct { uint8_t before[8], script[128], after[8]; } box;
    memset(&box, 0xa5, sizeof(box));
    size_t length = SIZE_MAX;
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, 20, box.script, capacity, &length) == expected);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    if (expected == ZCL_OK) script_matches(box.script, length);
    else CHECK(length == SIZE_MAX);
    for (size_t i = expected == ZCL_OK ? length : 0; i < 128; ++i) CHECK(box.script[i] == 0xa5);
}

static void profiles(void)
{
    static const uint8_t patterns[] = {0,1,0x7f,0xff};
    bool shortest = false, longest = false;
    for (uint8_t scalar = 1; scalar <= 16; ++scalar) {
        for (size_t i = 0; i < sizeof(patterns); ++i) {
            setup(scalar, patterns[i]);
            if (signature.der_len == 70) shortest = true;
            if (signature.der_len == 71) longest = true;
            for (size_t capacity = 0; capacity <= 128; ++capacity)
                call(capacity, capacity < signature.der_len + 36 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK);
            /* Only meaningful DER/key fields participate; ignored tail bytes
             * cannot be smuggled into the script or change verification. */
            memset(signature.der + signature.der_len, 0xee, 72 - signature.der_len);
            call(SIZE_MAX, ZCL_OK);
        }
    }
    CHECK(shortest && longest);
}

static void invalid(void)
{
    uint8_t output[128], saved[128];
    memset(output, 0xa5, sizeof(output)); memcpy(saved, output, sizeof(saved));
    size_t length = SIZE_MAX;
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, 20,
        output, sizeof(output), &length) != ZCL_OK);
    CHECK(length == SIZE_MAX && memcmp(output, saved, sizeof(output)) == 0);
#ifdef ZCL_SIGNATURE_ORACLE
    CHECK(zcl_test_signature_script_oracle(&signature, digest, 32, key_hash, 20) == 0);
#endif
}

static void corruption(void)
{
    setup(1, 0);
    for (size_t i = 0; i < signature.der_len; ++i) { signature.der[i] ^= 1; invalid(); signature.der[i] ^= 1; }
    for (size_t i = 0; i < 33; ++i) { signature.public_key[i] ^= 1; invalid(); signature.public_key[i] ^= 1; }
    for (size_t i = 0; i < 32; ++i) { digest[i] ^= 1; invalid(); digest[i] ^= 1; }
    for (size_t i = 0; i < 20; ++i) { key_hash[i] ^= 1; invalid(); key_hash[i] ^= 1; }
    const size_t original = signature.der_len;
    for (size_t length = 0; length <= 73; ++length) {
        if (length == original) continue;
        signature.der_len = length; invalid();
    }
    signature.der_len = SIZE_MAX; invalid();
}

static void high_s(void)
{
    static const uint8_t order[32] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x41
    };
    setup(1, 0);
    uint8_t compact[64];
    secp256k1_ecdsa_signature parsed = {{0}};
    const secp256k1_context *context = secp256k1_context_static;
    CHECK(secp256k1_ecdsa_signature_parse_der(context, &parsed, signature.der, signature.der_len) == 1);
    CHECK(secp256k1_ecdsa_signature_serialize_compact(context, compact, &parsed) == 1);
    unsigned borrow = 0;
    for (size_t i = 32; i > 0; --i) {
        const unsigned sub = (unsigned)compact[31 + i] + borrow;
        compact[31 + i] = (uint8_t)(((unsigned)order[i - 1] - sub) & 255U);
        borrow = (unsigned)order[i - 1] < sub ? 1U : 0U;
    }
    CHECK(borrow == 0);
    CHECK(secp256k1_ecdsa_signature_parse_compact(context, &parsed, compact) == 1);
    CHECK(secp256k1_ecdsa_signature_normalize(context, NULL, &parsed) == 1);
    signature.der_len = sizeof(signature.der);
    CHECK(secp256k1_ecdsa_signature_serialize_der(context, signature.der, &signature.der_len, &parsed) == 1);
    invalid();
}

static void malformed_der(void)
{
    setup(1, 0);
    CHECK(signature.der_len <= 71);
    /* Sequence long-form length, redundant integer padding and a trailing
     * byte all refuse, even when the numerical signature could be recovered. */
    memmove(signature.der + 3, signature.der + 2, signature.der_len - 2);
    signature.der[2] = signature.der[1]; signature.der[1] = 0x81; ++signature.der_len;
    invalid();
    setup(1, 0);
    memmove(signature.der + 5, signature.der + 4, signature.der_len - 4);
    signature.der[4] = 0; ++signature.der[1]; ++signature.der[3]; ++signature.der_len;
    invalid();
    setup(1, 0); signature.der[signature.der_len++] = 0; invalid();
    setup(1, 0);
    static const uint8_t zero[] = {0x30,6,2,1,0,2,1,0};
    memcpy(signature.der, zero, sizeof(zero)); signature.der_len = sizeof(zero);
    invalid();
}

static void arguments(void)
{
    setup(1, 0);
    uint8_t output[128]; memset(output, 0xa5, sizeof(output));
    size_t length = SIZE_MAX;
    CHECK(zcl_signature_p2pkh(NULL, digest, 32, key_hash, 20, output, 128, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_p2pkh(&signature, NULL, 32, key_hash, 20, output, 128, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, NULL, 20, output, 128, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, 20, NULL, 128, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, 20, output, 128, NULL) == ZCL_INVALID_ARGUMENT);
    for (size_t size = 0; size <= 64; ++size) {
        if (size != 32) CHECK(zcl_signature_p2pkh(&signature, digest, size, key_hash, 20, output, 128, &length) == ZCL_OUT_OF_RANGE);
        if (size != 20) CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, size, output, 128, &length) == ZCL_OUT_OF_RANGE);
    }
    CHECK(zcl_signature_p2pkh(&signature, digest, SIZE_MAX, key_hash, 20, output, 128, &length) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_signature_p2pkh(&signature, digest, 32, key_hash, SIZE_MAX, output, 128, &length) == ZCL_OUT_OF_RANGE);
    CHECK(length == SIZE_MAX);
    for (size_t i = 0; i < sizeof(output); ++i) CHECK(output[i] == 0xa5);
}

int main(void)
{
    profiles(); corruption(); high_s(); malformed_der(); arguments();
    CHECK(puts("Canonical P2PKH scripts verify the exact digest and key hash, with strict DER and bounded atomic output") >= 0);
    return 0;
}
