/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "commitment_fixture.h"
#include <string.h>
#ifdef ZCL_COMMITMENT_ORACLE
#include <openssl/evp.h>
#else
#include "mbedtls/sha256.h"
#endif

static bool digest(const uint8_t *bytes, size_t size, uint8_t output[32])
{
    uint8_t first[32];
#ifdef ZCL_COMMITMENT_ORACLE
    unsigned count = 0;
    if (EVP_Digest(bytes, size, first, &count, EVP_sha256(), NULL) != 1 || count != 32) return false;
    return EVP_Digest(first, sizeof(first), output, &count, EVP_sha256(), NULL) == 1 && count == 32;
#else
    return mbedtls_sha256(bytes, size, first, 0) == 0 && mbedtls_sha256(first, sizeof(first), output, 0) == 0;
#endif
}

static void reverse(const uint8_t *input, uint8_t *output)
{
    for (size_t i = 0; i < 32; ++i) output[i] = input[31 - i];
}

static bool path(commitment_fixture *fixture, uint8_t current[32])
{
    zcl_merkle_branch *branch = &fixture->branch;
    uint64_t width = branch->transaction_count, index = branch->transaction_index;
    if (width == 0 || index >= width) return false;
    branch->sibling_count = 0;
    while (width > 1) {
        const size_t level = branch->sibling_count++;
        uint8_t other[32], pair[64];
        for (size_t i = 0; i < 32; ++i) other[i] = (uint8_t)((level * 73 + i * 19 + 7) % 256);
        if ((index ^ UINT64_C(1)) >= width) memcpy(other, current, 32);
        reverse(other, branch->siblings[level]);
        if (index % 2 == 0) { memcpy(pair, current, 32); memcpy(pair + 32, other, 32); }
        else { memcpy(pair, other, 32); memcpy(pair + 32, current, 32); }
        if (!digest(pair, sizeof(pair), current)) return false;
        index /= 2; width = (width + 1) / 2;
    }
    return true;
}

bool commitment_fixture_bind(commitment_fixture *fixture)
{
    uint8_t current[32];
    zcl_source_commitment_request *request = &fixture->request;
    if (!digest(request->source, request->source_length, current)) return false;
    reverse(current, request->transaction_id);
    if (!path(fixture, current)) return false;
    memcpy(fixture->header + 36, current, 32);
    if (!digest(fixture->header, sizeof(fixture->header), current)) return false;
    reverse(current, request->header_id);
    return true;
}

bool commitment_fixture_init(commitment_fixture *fixture, unsigned tail, uint32_t width, uint32_t index)
{
    memset(fixture, 0, sizeof(*fixture));
    if (!source_assessment_init(&fixture->funding, tail)) return false;
    fixture->request.source = fixture->funding.base.sources[0].wire;
    fixture->request.source_length = fixture->funding.base.sources[0].length;
    fixture->request.header = fixture->header;
    fixture->request.header_length = sizeof(fixture->header);
    fixture->request.network = ZCL_MAINNET; fixture->request.height = 585318;
    fixture->request.branch = &fixture->branch;
    fixture->branch.transaction_count = width; fixture->branch.transaction_index = index;
    for (size_t i = 0; i < 140; ++i) fixture->header[i] = (uint8_t)((i * 31 + 19) % 256);
    fixture->header[140] = 253; fixture->header[141] = 144; fixture->header[142] = 1;
    return commitment_fixture_bind(fixture);
}
