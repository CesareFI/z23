/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_reference.h"
#include "context_reference.h"
#include "signature_oracle.h"
#include "sighash_oracle.h"
#include <stdlib.h>
#include <string.h>

static void append(uint8_t *wire, size_t *used, const uint8_t *bytes, size_t length)
{
    if (*used > ZCL_TX_WIRE_MAX || length > ZCL_TX_WIRE_MAX - *used) abort();
    memcpy(wire + *used, bytes, length); *used += length;
}

static void push_signature(uint8_t *wire, size_t *used, const zcl_signature *signature)
{
    if (signature->der_len < 8 || signature->der_len > 71) abort();
    uint8_t byte = (uint8_t)(signature->der_len + 36);
    append(wire, used, &byte, 1); /* Input script vector length. */
    byte = (uint8_t)(signature->der_len + 1); append(wire, used, &byte, 1);
    append(wire, used, signature->der, signature->der_len);
    byte = 1; append(wire, used, &byte, 1); /* SIGHASH_ALL */
    byte = 33; append(wire, used, &byte, 1); append(wire, used, signature->public_key, 33);
}

static bool verified(const signed_review_fixture *baseline, const zcl_signature *signature,
    size_t index, uint32_t branch)
{
    uint8_t script[25] = {0x76,0xa9,0x14}, digest[32] = {0};
    memcpy(script + 3, baseline->hashes[index], 20); script[23] = 0x88; script[24] = 0xac;
    zcl_test_sighash_all(baseline->unsigned_wire, baseline->unsigned_length, index,
        script, sizeof(script), 10000, branch, digest, sizeof(digest));
    return zcl_test_signature_script_oracle(signature, digest, 32, baseline->hashes[index], 20) == 1;
}

static bool context_matches(const signed_review_fixture *baseline, const zcl_review_block *block,
    size_t count, uint32_t *branch)
{
    if (count == 0 || count > 8 || count != baseline->spending.input_count) return false;
    if (block->network != baseline->block.network || block->lock_time_cutoff > INT64_MAX) return false;
    if (zcl_test_context_branch(block->network, block->height, branch) != ZCL_OK) return false;
    if (block->height > baseline->spending.expiry_height) return false; /* Fixture always has nonzero expiry. */
    if (baseline->spending.lock_time != 0 || baseline->spending.expiry_height == 0) abort();
    return true;
}

bool signed_review_reference(const signed_review_fixture *baseline, const zcl_review_block *block,
    const zcl_signature *signatures, size_t count, uint8_t *wire, size_t *length)
{
    if (baseline == NULL || block == NULL || signatures == NULL || wire == NULL || length == NULL) abort();
    uint32_t branch = 0;
    if (!context_matches(baseline, block, count, &branch)) return false;
    size_t used = 0;
    append(wire, &used, baseline->unsigned_wire, 9); /* Fixed header/group/count. */
    for (size_t i = 0; i < count; ++i) {
        const size_t start = 9 + 41 * i;
        if (start + 41 > baseline->unsigned_length || baseline->unsigned_wire[start + 36] != 0) abort();
        if (!verified(baseline, &signatures[i], i, branch)) return false;
        append(wire, &used, baseline->unsigned_wire + start, 36);
        push_signature(wire, &used, &signatures[i]);
        append(wire, &used, baseline->unsigned_wire + start + 37, 4);
    }
    const size_t rest = 9 + 41 * count;
    if (rest > baseline->unsigned_length || baseline->unsigned_length > ZCL_TX_WIRE_MAX) abort();
    append(wire, &used, baseline->unsigned_wire + rest, baseline->unsigned_length - rest);
    *length = used;
    return true;
}
