/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_merkle_internal.h"
#include "zcl_keys.h"
#include "mbedtls/sha256.h"
#include <string.h>

typedef struct {
    uint8_t current[32], sibling[32], pair[64], first[32];
} merkle_work;

static uint32_t parent_width(uint32_t width)
{
    return width / 2 + width % 2; /* ceil(width/2), including UINT32_MAX. */
}

static size_t branch_depth(uint32_t width)
{
    size_t depth = 0;
    while (width > 1) { width = parent_width(width); ++depth; }
    return depth;
}

static zcl_status branch_arguments(const uint8_t *id, const zcl_merkle_branch *branch,
    const uint8_t *root)
{
    if (id == NULL || branch == NULL || root == NULL) return ZCL_INVALID_ARGUMENT;
    if (branch->transaction_count == 0 || branch->transaction_index >= branch->transaction_count)
        return ZCL_OUT_OF_RANGE;
    if (branch->sibling_count > ZCL_MERKLE_BRANCH_MAX) return ZCL_RESOURCE_EXHAUSTED;
    if (branch->sibling_count != branch_depth(branch->transaction_count)) return ZCL_INVALID_ENCODING;
    return ZCL_OK;
}

static void reverse_hash(const uint8_t source[32], uint8_t output[32])
{
    for (size_t i = 0; i < 32; ++i) output[i] = source[31 - i];
}

static zcl_status branch_step(merkle_work *work, const uint8_t sibling[32],
    uint32_t position, uint32_t width)
{
    reverse_hash(sibling, work->sibling);
    const bool duplicate = (position ^ UINT32_C(1)) >= width;
    const bool equal = memcmp(work->current, work->sibling, 32) == 0;
    if (duplicate != equal) return ZCL_INVALID_ENCODING;
    const size_t own_offset = (size_t)(position & UINT32_C(1)) * 32;
    memcpy(work->pair + own_offset, work->current, 32);
    memcpy(work->pair + (32 - own_offset), work->sibling, 32);
    if (mbedtls_sha256(work->pair, sizeof(work->pair), work->first, 0) != 0)
        return ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(work->first, sizeof(work->first), work->current, 0) != 0)
        return ZCL_CRYPTO_FAILURE;
    return ZCL_OK;
}

zcl_status zcl_merkle_branch_check(const uint8_t transaction_id[32],
    const zcl_merkle_branch *branch, const uint8_t expected_root[32])
{
    zcl_status status = branch_arguments(transaction_id, branch, expected_root);
    if (status != ZCL_OK) return status;
    merkle_work work = {0};
    reverse_hash(transaction_id, work.current);
    uint32_t width = branch->transaction_count, position = branch->transaction_index;
    for (size_t i = 0; i < branch->sibling_count; ++i) {
        status = branch_step(&work, branch->siblings[i], position, width);
        if (status != ZCL_OK) break;
        position /= 2;
        width = parent_width(width);
    }
    reverse_hash(expected_root, work.sibling);
    if (status == ZCL_OK && memcmp(work.current, work.sibling, 32) != 0) status = ZCL_INVALID_ENCODING;
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
