/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SIGNED_REVIEW_FIXTURE_H
#define ZCL_SIGNED_REVIEW_FIXTURE_H
#include "transaction_review_internal.h"
/* Large, public synthetic fixture: keep outside production/thread stacks.
 * Each initialization is a NEW isolated lifetime, with no outstanding IDs. */
typedef struct {
    zcl_review_owner owner;
    zcl_review_block block;
    zcl_transparent_tx funding, spending;
    zcl_signature signatures[ZCL_TX_INPUT_MAX];
    uint8_t hashes[ZCL_TX_INPUT_MAX][20], digests[ZCL_TX_INPUT_MAX][32];
    uint8_t funding_wire[ZCL_TX_WIRE_MAX], unsigned_wire[ZCL_TX_WIRE_MAX];
    size_t funding_length, unsigned_length;
    uint64_t id;
} signed_review_fixture;
bool signed_review_fixture_init(signed_review_fixture *fixture, zcl_network network,
    size_t inputs, size_t outputs);
#endif
