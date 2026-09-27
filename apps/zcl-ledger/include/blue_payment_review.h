/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_REVIEW_H
#define ZCL_BLUE_PAYMENT_REVIEW_H

#include "zcl_tx_replay_zip243.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "Blue payment review requires ISO C23"
#endif

enum { BLUE_PAYMENT_REVIEW_MAX_OUTPUTS = 16 };

typedef struct {
    uint32_t index;
    uint64_t amount_zat;
    zcl_tx_stream_output_type type;
    uint8_t hash160[20];
} blue_payment_output;

typedef struct {
    zcl_tx_replay_zip243 replay;
    blue_payment_output output;
    uint32_t total_outputs, acknowledged;
    bool pending, verified;
} blue_payment_review;

/* These calls do not use keys or authorize signing. During the third pass,
 * feed refuses further bytes while an output awaits a physical touch. The
 * pending output is provisional until finish checks the full wire replay. */
bool blue_payment_review_begin(blue_payment_review *review,
    uint32_t expected_length, uint32_t selected_index, uint32_t branch_id,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha);
bool blue_payment_review_feed(blue_payment_review *review,
    const uint8_t *bytes, size_t length);
bool blue_payment_review_next_pass(blue_payment_review *review);
const blue_payment_output *blue_payment_review_pending(
    const blue_payment_review *review);
bool blue_payment_review_acknowledge(blue_payment_review *review);
bool blue_payment_review_finish(blue_payment_review *review,
    const uint8_t *script_code, size_t script_code_length,
    uint64_t amount_zat, zcl_tx_stream_facts *facts, uint8_t digest[32]);
void blue_payment_review_abort(blue_payment_review *review);

#endif
