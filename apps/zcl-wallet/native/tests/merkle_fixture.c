/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef mbedtls_sha256
#include "merkle_fixture.h"
#include <string.h>
#ifdef ZCL_MERKLE_ORACLE
#include <openssl/evp.h>
#else
#include "mbedtls/sha256.h"
#endif

static void flipped(const uint8_t source[32], uint8_t target[32])
{
    for (size_t i = 0; i < 32; ++i) target[i] = source[31 - i];
}

static bool pair_hash(const uint8_t left[32], const uint8_t right[32], uint8_t output[32])
{
    uint8_t bytes[64], first[32];
    memcpy(bytes, left, 32); memcpy(bytes + 32, right, 32);
#ifdef ZCL_MERKLE_ORACLE
    unsigned length = 0;
    if (EVP_Digest(bytes, sizeof(bytes), first, &length, EVP_sha256(), NULL) != 1 || length != 32) return false;
    return EVP_Digest(first, sizeof(first), output, &length, EVP_sha256(), NULL) == 1 && length == 32;
#else
    return mbedtls_sha256(bytes, sizeof(bytes), first, 0) == 0 && mbedtls_sha256(first, sizeof(first), output, 0) == 0;
#endif
}

static void opaque_hash(uint32_t label, uint8_t output[32])
{
    /* Public deterministic input, explicitly not a transaction hash oracle. */
    for (size_t i = 0; i < 32; ++i) output[i] = (uint8_t)(((uint64_t)label * 37 + i * 13 + 7) & 255);
    for (unsigned i = 0; i < 4; ++i) output[i] = (uint8_t)((label >> (8 * i)) & 255);
}

bool merkle_fixture_tree(uint32_t count, uint32_t index, merkle_fixture *fixture)
{
    if (fixture == NULL || count == 0 || count > 65 || index >= count) return false;
    memset(fixture, 0, sizeof(*fixture));
    fixture->branch.transaction_count = count; fixture->branch.transaction_index = index;
    uint8_t nodes[65][32] = {{0}};
    for (uint32_t i = 0; i < count; ++i) opaque_hash(i + 1, nodes[i]);
    flipped(nodes[index], fixture->leaf);
    while (count > 1) {
        const uint32_t other = (index ^ UINT32_C(1)) < count ? (index ^ UINT32_C(1)) : index;
        flipped(nodes[other], fixture->branch.siblings[fixture->branch.sibling_count++]);
        for (uint32_t i = 0; i < count; i += 2) {
            const uint32_t right = i + 1 < count ? i + 1 : i;
            if (!pair_hash(nodes[i], nodes[right], nodes[i / 2])) return false;
        }
        count = (count + 1) / 2; index /= 2;
    }
    flipped(nodes[0], fixture->root);
    return true;
}

bool merkle_fixture_path(uint32_t count, uint32_t index, merkle_fixture *fixture)
{
    if (fixture == NULL || count == 0 || index >= count) return false;
    memset(fixture, 0, sizeof(*fixture));
    fixture->branch.transaction_count = count; fixture->branch.transaction_index = index;
    uint8_t node[32], other[32];
    opaque_hash(UINT32_C(0x10203040), node); flipped(node, fixture->leaf);
    uint64_t width = count, position = index;
    while (width > 1) {
        const size_t depth = fixture->branch.sibling_count++;
        opaque_hash((uint32_t)depth + 9000, other);
        if ((position ^ UINT64_C(1)) >= width) memcpy(other, node, 32);
        flipped(other, fixture->branch.siblings[depth]);
        if (position % 2 != 0) { if (!pair_hash(other, node, node)) return false; }
        else if (!pair_hash(node, other, node)) return false;
        width = (width + 1) / 2; position /= 2;
    }
    flipped(node, fixture->root);
    return true;
}

bool merkle_fixture_check(const merkle_fixture *fixture)
{
    const zcl_merkle_branch *proof = &fixture->branch;
    if (proof->transaction_count == 0 || proof->transaction_index >= proof->transaction_count ||
        proof->sibling_count > 32) return false;
    uint8_t current[32], other[32], root[32];
    flipped(fixture->leaf, current);
    uint64_t width = proof->transaction_count, index = proof->transaction_index;
    size_t level = 0;
    while (width > 1) {
        if (level >= proof->sibling_count) return false;
        flipped(proof->siblings[level++], other);
        if ((index ^ UINT64_C(1)) >= width) { if (memcmp(current, other, 32) != 0) return false; }
        else if (memcmp(current, other, 32) == 0) return false;
        if (index % 2 != 0) { if (!pair_hash(other, current, current)) return false; }
        else if (!pair_hash(current, other, current)) return false;
        index /= 2; width = (width + 1) / 2;
    }
    flipped(current, root);
    return level == proof->sibling_count && memcmp(root, fixture->root, 32) == 0;
}
