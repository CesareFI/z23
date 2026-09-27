/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TX_REPLAY_ZIP243_H
#define ZCL_TX_REPLAY_ZIP243_H

#include "zcl_tx_stream.h"
#include "zcl_zip243.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "Replay-bound ZIP-243 requires ISO C23"
#endif

typedef struct {
    void *context;
    bool (*init)(void *context);
    bool (*update)(void *context, const uint8_t *bytes, size_t length);
    bool (*final)(void *context, uint8_t digest[32]);
} zcl_tx_replay_sha256;

typedef bool (*zcl_tx_replay_input_fn)(void *context, uint32_t index,
    const uint8_t outpoint[36]);

typedef struct {
    zcl_tx_stream wire;
    zcl_zip243_hasher blake;
    zcl_tx_replay_sha256 sha;
    uint8_t commitment[32], prevouts[32], sequences[32], selected[40];
    uint32_t expected, selected_index, branch_id;
    zcl_tx_replay_input_fn input_observer;
    void *input_context;
    uint8_t pass;
    bool selected_found;
} zcl_tx_replay_zip243;

typedef bool (*zcl_tx_replay_output_fn)(void *context, uint32_t index,
    uint64_t amount_zat, zcl_tx_stream_output_type type,
    const uint8_t hash160[20]);

/* Upload the identical unsigned transaction three times. Each complete pass
 * is parsed and SHA-256 checked against the first pass before its ZIP-243
 * subhash is accepted. No partial or mismatched pass returns a digest.
 * script_code, spent amount, and branch ID still need trusted provenance. */
bool zcl_tx_replay_zip243_begin(zcl_tx_replay_zip243 *state,
    uint32_t expected_length, uint32_t selected_index, uint32_t branch_id,
    const zcl_zip243_hasher *blake, const zcl_tx_replay_sha256 *sha);
/* The observer receives provisional pass-one outpoints. Their caller must
 * defer use until the full three-pass replay succeeds. */
bool zcl_tx_replay_zip243_observe_inputs(zcl_tx_replay_zip243 *state,
    zcl_tx_replay_input_fn observer, void *context);
bool zcl_tx_replay_zip243_feed(zcl_tx_replay_zip243 *state,
    const uint8_t *bytes, size_t length);
/* The observer sees provisional output facts only during pass three. It must
 * not treat them as authenticated until the final pass succeeds. */
bool zcl_tx_replay_zip243_feed_review(zcl_tx_replay_zip243 *state,
    const uint8_t *bytes, size_t length,
    zcl_tx_replay_output_fn observer, void *observer_context);
bool zcl_tx_replay_zip243_next(zcl_tx_replay_zip243 *state);
bool zcl_tx_replay_zip243_finish(zcl_tx_replay_zip243 *state,
    const uint8_t *script_code, size_t script_code_length,
    uint64_t amount_zat, zcl_tx_stream_facts *facts, uint8_t digest[32]);

#endif
