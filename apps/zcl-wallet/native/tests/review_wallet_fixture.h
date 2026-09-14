/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_REVIEW_WALLET_FIXTURE_H
#define ZCL_REVIEW_WALLET_FIXTURE_H
#include "assessment_fixture.h"
#include "change_storage_fixture.h"
#include "transaction_review_internal.h"

/* Serial public synthetic fixture; keep this large object outside thread
 * stacks. No funded outpoint, GCM authentication or hardware policy is claimed.
 * It owns one mkdtemp directory and one unsigned-review lifetime. */
typedef struct {
    assessment_fixture funding;
    change_storage_data wallet;
    storage_fixture store;
    zcl_review_owner review;
    uint8_t entropy[32];
    size_t entropy_len;
    zcl_network network;
    uint8_t draft[ZCL_TX_WIRE_MAX];
    size_t draft_length;
    uint64_t id;
} review_wallet_fixture;

int review_wallet_fixture_open(review_wallet_fixture *fixture, zcl_network network,
    size_t entropy_len, bool with_state);
int review_wallet_fixture_review(review_wallet_fixture *fixture, zcl_network network, uint64_t now_ms);
int review_wallet_fixture_close(review_wallet_fixture *fixture);
/* Borrowed pointers remain valid only while the fixture lives and its spans
 * stay stable; the caller does not own another wallet/authorization handle. */
zcl_review_wallet_input review_wallet_fixture_claim(const review_wallet_fixture *fixture,
    uint32_t chain, uint32_t index);
#endif
