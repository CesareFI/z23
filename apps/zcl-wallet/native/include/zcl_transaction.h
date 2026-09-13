/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_H
#define ZCL_TRANSACTION_H
#include "zcl_wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded wallet codec for existing Zclassic v4/Sapling wire transactions with
 * transparent fields only. These are local resource limits, not consensus
 * limits. Scripts are opaque bytes: decoding is NOT script/signature checking,
 * proof of funding/ownership, fee calculation or authorization to broadcast.
 * Legacy, Overwinter v3, shielded and larger transactions are unsupported.
 */
#define ZCL_TX_INPUT_MAX ((size_t)8)
#define ZCL_TX_OUTPUT_MAX ((size_t)16)
#define ZCL_TX_INPUT_SCRIPT_MAX ((size_t)128)
#define ZCL_TX_OUTPUT_SCRIPT_MAX ((size_t)25)
#define ZCL_TX_WIRE_MAX ((size_t)1925)
#define ZCL_TX_EXPIRY_LIMIT UINT32_C(500000000)

typedef struct {
    uint8_t previous_txid[32]; /* Displayed big-endian order, reversed on wire. */
    uint32_t previous_index;
    uint32_t sequence;
    size_t script_len;
    uint8_t script[ZCL_TX_INPUT_SCRIPT_MAX];
} zcl_tx_input;

typedef struct {
    uint64_t value;
    size_t script_len;
    uint8_t script[ZCL_TX_OUTPUT_SCRIPT_MAX];
} zcl_tx_output;

typedef struct {
    uint32_t lock_time;
    uint32_t expiry_height;
    size_t input_count;
    size_t output_count;
    zcl_tx_input inputs[ZCL_TX_INPUT_MAX];
    zcl_tx_output outputs[ZCL_TX_OUTPUT_MAX];
} zcl_transparent_tx;

/* Fully owned public data; no heap, retained pointers or global state. Caller
 * owns stable, nonoverlapping input/output objects for the synchronous call.
 * NULL is rejected even for empty spans. Outputs remain unchanged on failure.
 * Parse requires exactly one whole canonical transaction; zero vectors, null
 * outpoints, duplicate inputs, invalid expiry and excessive output sums refuse.
 * A nonempty opaque script does not establish that an input is signed.
 */
zcl_status zcl_transaction_parse(const uint8_t *wire, size_t length,
                                 zcl_transparent_tx *transaction);
zcl_status zcl_transaction_serialize(const zcl_transparent_tx *transaction,
                                     uint8_t *wire, size_t capacity, size_t *length);
/* SHA256d of this exact canonical serialization, in displayed big-endian order.
 * Changing a script (including signing) changes the ID. This is NOT a sighash.
 */
zcl_status zcl_transaction_id(const zcl_transparent_tx *transaction,
                              uint8_t *txid, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
