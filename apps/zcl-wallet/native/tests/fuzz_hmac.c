/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Host-only differential test. All key/message bytes are public fuzz input. */
#include "secret_hash.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void require(int okay, const char *message)
{
    if (!okay) { fprintf(stderr, "HMAC differential failure: %s\n", message); abort(); }
}

static void refused(const uint8_t *key, size_t key_len, const uint8_t *message,
    size_t message_len, uint8_t control, size_t capacity)
{
    uint8_t output[66], before[66];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    uint8_t *destination = output + 1;
    zcl_status expected = ZCL_OUT_OF_RANGE;
    switch (control % 10) {
    case 0: key = NULL; expected = ZCL_INVALID_ARGUMENT; break;
    case 1: message = NULL; expected = ZCL_INVALID_ARGUMENT; break;
    case 2: destination = NULL; expected = ZCL_INVALID_ARGUMENT; break;
    case 3: key_len = 257; break;
    case 4: message_len = 513; break;
    case 5: key_len = SIZE_MAX; break;
    case 6: message_len = SIZE_MAX; break;
    default: expected = ZCL_BUFFER_TOO_SMALL; break;
    }
    require(zcl_hmac_sha512(key, key_len, message, message_len, destination, capacity) == expected,
        "incorrect refusal status");
    require(memcmp(output, before, sizeof(output)) == 0, "refusal changed output");
}

static void compare(const uint8_t *key, size_t key_len, const uint8_t *message, size_t message_len)
{
    uint8_t output[66], expected[64] = {0};
    unsigned int length = 0;
    memset(output, 0xa5, sizeof(output));
    require(HMAC(EVP_sha512(), key, (int)key_len, message, message_len, expected, &length) != NULL
        && length == 64, "OpenSSL reference failed");
    require(zcl_hmac_sha512(key, key_len, message, message_len, output + 1, 64) == ZCL_OK,
        "valid input refused");
    require(output[0] == 0xa5 && output[65] == 0xa5, "output guard changed");
    require(memcmp(output + 1, expected, sizeof(expected)) == 0, "digest mismatch");
    zcl_secure_zero(output, sizeof(output));
    OPENSSL_cleanse(expected, sizeof(expected));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 6 || size > 774) return 0;
    uint8_t key[256], saved_key[256], message[512], saved_message[512];
    const size_t key_len = ((size_t)data[0] + (size_t)data[1] * 256) % 257;
    const size_t message_len = ((size_t)data[2] + (size_t)data[3] * 256) % 513;
    for (size_t i = 0; i < sizeof(key); ++i) key[i] = data[(6 + i) % size];
    for (size_t i = 0; i < sizeof(message); ++i) message[i] = data[(262 + i) % size];
    memcpy(saved_key, key, sizeof(key));
    memcpy(saved_message, message, sizeof(message));
    /* Every admitted case reaches real cryptography before argument refusals. */
    compare(key, key_len, message, message_len);
    refused(key, key_len, message, message_len, data[5], data[4] % 64);
    require(memcmp(key, saved_key, sizeof(key)) == 0, "key changed");
    require(memcmp(message, saved_message, sizeof(message)) == 0, "message changed");
    zcl_secure_zero(key, sizeof(key));
    zcl_secure_zero(saved_key, sizeof(saved_key));
    zcl_secure_zero(message, sizeof(message));
    zcl_secure_zero(saved_message, sizeof(saved_message));
    return 0;
}
