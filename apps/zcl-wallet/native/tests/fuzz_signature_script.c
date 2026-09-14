/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signature_oracle.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
typedef struct { uint8_t before[8], output[128], after[8]; } script_box;

static void script_valid(const zcl_signature *signature, const uint8_t *script, size_t length)
{
    if (length < 44 || length > 107 || length != signature->der_len + 36) abort();
    const size_t first = script[0];
    if (first != signature->der_len + 1 || first >= length || script[first] != 1) abort();
    if (memcmp(script + 1, signature->der, signature->der_len) != 0) abort();
    if (script[first + 1] != 33 || length - first - 2 != 33) abort();
    if (memcmp(script + first + 2, signature->public_key, 33) != 0) abort();
}

static bool accepts(const zcl_signature *signature, const uint8_t *digest, size_t digest_len,
    const uint8_t *hash, size_t hash_len, const uint8_t *output, size_t capacity, const size_t *length)
{
    if (signature == NULL || output == NULL || length == NULL) return false;
    if (signature->der_len < 8 || signature->der_len > 71) return false;
    if (!zcl_test_signature_script_oracle(signature, digest, digest_len, hash, hash_len)) return false;
    return capacity >= signature->der_len + 36;
}

static void result_checked(const zcl_signature *signature, const script_box *box,
    bool expected, zcl_status status, size_t length)
{
    if ((status == ZCL_OK) != expected) abort();
    for (size_t i = 0; i < 8; ++i) if (box->before[i] != 0xa5 || box->after[i] != 0xa5) abort();
    if (expected) script_valid(signature, box->output, length);
    else if (length != SIZE_MAX) abort();
    for (size_t i = expected ? length : 0; i < 128; ++i) if (box->output[i] != 0xa5) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size != 160) return 0;
    zcl_signature signature;
    memset(&signature, 0, sizeof(signature));
    signature.der_len = data[0] == 255 ? SIZE_MAX : data[0];
    memcpy(signature.der, data + 3, 72); memcpy(signature.public_key, data + 75, 33);
    const unsigned null = (data[2] & 7U) % 6;
    const zcl_signature *input = null == 1 ? NULL : &signature;
    const uint8_t *digest = null == 2 ? NULL : data + 108;
    const uint8_t *hash = null == 3 ? NULL : data + 140;
    const size_t digest_len = (data[2] & 128U) != 0 ? data[1] % 65 : 32;
    const size_t hash_len = (data[2] & 64U) != 0 ? data[1] % 33 : 20;
    const size_t capacity = data[1] % 129;
    script_box box;
    memset(&box, 0xa5, sizeof(box));
    uint8_t *output = null == 4 ? NULL : box.output;
    size_t length = SIZE_MAX;
    size_t *written = null == 5 ? NULL : &length;
    const bool expected = accepts(input, digest, digest_len, hash, hash_len, output, capacity, written);
    const zcl_status status = zcl_signature_p2pkh(input, digest, digest_len, hash, hash_len, output, capacity, written);
    result_checked(input, &box, expected, status, length);
    return 0;
}
