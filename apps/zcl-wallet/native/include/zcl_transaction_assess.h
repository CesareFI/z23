/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_ASSESS_H
#define ZCL_TRANSACTION_ASSESS_H
#include "zcl_transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Borrowed only for the synchronous call; caller owns stable bytes. Entries
 * correspond exactly to transaction input order. Repeating the same previous
 * transaction for different output indexes is permitted. No pointer is kept. */
typedef struct {
    const uint8_t *wire;
    size_t length;
} zcl_previous_transaction;

typedef struct {
    zcl_address destination;
    uint64_t value;
} zcl_assessed_output;

typedef struct {
    zcl_network network;
    uint8_t transaction_id[32];
    size_t serialized_size;
    size_t input_count;
    size_t output_count;
    uint64_t input_total;
    uint64_t output_total;
    uint64_t fee;
    uint64_t maximum_fee;
    zcl_assessed_output inputs[ZCL_TX_INPUT_MAX];
    zcl_assessed_output outputs[ZCL_TX_OUTPUT_MAX];
} zcl_transaction_assessment;

/* Public review DATA, not authorization. Validate the entire bounded current
 * transaction, match every previous transaction hash/index, require exact
 * P2PKH/P2SH destination templates, check totals <=MAX_MONEY and output<=input,
 * then enforce the caller's explicit absolute maximum_fee (0..MAX_MONEY).
 * No implicit fee rate, default ceiling, change label or ownership assertion.
 * The ID binds current serialized bytes, including any current input scripts;
 * it does NOT bind network or fee policy. Size is current bytes, not an estimate
 * of future signed size. Changing any fields requires a new assessment.
 * This does not establish inclusion, unspentness, maturity, key ownership,
 * script validity, chain/branch/height validity or approval to sign/broadcast.
 * Caller owns stable nonoverlapping spans; no allocation or pointer retention.
 * Failure leaves the entire assessment unchanged, including after partial work.
 */
zcl_status zcl_transaction_assess(const zcl_transparent_tx *transaction,
                                  zcl_network network,
                                  const zcl_previous_transaction *previous,
                                  size_t previous_count, uint64_t maximum_fee,
                                  zcl_transaction_assessment *assessment);

#ifdef __cplusplus
}
#endif
#endif
