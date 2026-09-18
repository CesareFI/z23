/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SOURCE_COMMITMENT_INTERNAL_H
#define ZCL_SOURCE_COMMITMENT_INTERNAL_H
#include "transaction_source_internal.h"
#include "transaction_merkle_internal.h"
#include "header_internal.h"

typedef struct {
    const uint8_t *source;
    size_t source_length;
    uint8_t transaction_id[32];
    uint32_t output_index;
    const uint8_t *header;
    size_t header_length;
    uint8_t header_id[32];
    zcl_network network;
    uint32_t height;
    const zcl_merkle_branch *branch;
} zcl_source_commitment_request;

typedef struct {
    zcl_source_view source;
    zcl_header_view header;
} zcl_source_commitment;

/* Bind complete v4 source bytes, selected output and expected source/header IDs
 * through the supplied Merkle path. IDs use displayed order. Reject64-byte
 * source preimages, the same length as internal Merkle-node hash inputs.
 * This is a conservative composition policy, NOT a consensus predicate.
 *
 * OK is ONLY byte/hash/path consistency. Caller supplies every expected ID,
 * height, count and index. No accepted chain, PoW, off-path uniqueness, fresh
 * state, maturity, unspentness, ownership, consent or signing authority follows.
 * Opaque source proofs/signatures remain unverified. Stable nonoverlapping
 * spans are caller-owned; no pointers survive. Whole output stays unchanged
 * on failure. No heap, I/O, network access or trust-status mutation. */
zcl_status zcl_v4_source_commitment_check(const zcl_source_commitment_request *request,
    zcl_source_commitment *output);

/* Explicit mixed historical/v4 source profile; otherwise the identical
 * consistency-only contract above, including the64-byte preimage refusal. */
zcl_status zcl_source_commitment_check(const zcl_source_commitment_request *request,
    zcl_source_commitment *output);
#endif
