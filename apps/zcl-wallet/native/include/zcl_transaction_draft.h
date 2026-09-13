/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_DRAFT_H
#define ZCL_TRANSACTION_DRAFT_H
#include "zcl_transaction_assess.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    zcl_previous_transaction previous; /* Borrowed canonical public bytes. */
    uint32_t output_index;
    uint32_t sequence;
} zcl_draft_funding;

typedef struct {
    zcl_address destination;
    uint64_t value;
} zcl_draft_output;

/* Explicit selections, NOT verified funding or change/key ownership. Initialize
 * {0}, then supply each counted row and policy field. No automatic coin choice,
 * fee rate/ceiling, output sorting, change, lock/expiry or sequence default.
 * Previous transaction bytes have the same bounded v4 transparent limitations
 * as zcl_transaction_parse; larger/legacy/v3/shielded sources are unsupported.
 */
typedef struct {
    zcl_network network;
    uint32_t lock_time;
    uint32_t expiry_height;
    uint64_t maximum_fee;
    size_t input_count;
    size_t output_count;
    zcl_draft_funding inputs[ZCL_TX_INPUT_MAX];
    zcl_draft_output outputs[ZCL_TX_OUTPUT_MAX];
} zcl_draft_request;

/* Build an owned unsigned transaction only after canonical source/index checks,
 * exact destination/network checks and the existing complete fee assessment.
 * Derive each outpoint ID from its supplied bytes. Preserve input/output order;
 * input scripts and unused rows are zero. Duplicate outpoints refuse, but the
 * same previous transaction may fund distinct output indexes. No allocation,
 * secret material, retained pointer, authentication, signing or broadcast.
 * Raw lock/expiry/sequence do NOT establish current-chain finality or validity.
 * Caller owns stable request/source bytes, nonoverlapping with the output for
 * the synchronous call. Source spans may share bytes. Any failure leaves the
 * whole output unchanged. Success does not prove inclusion/unspentness/maturity.
 */
zcl_status zcl_transaction_draft(const zcl_draft_request *request,
                                zcl_transparent_tx *transaction);

#ifdef __cplusplus
}
#endif
#endif
