/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_REVIEW_H
#define ZCL_TRANSACTION_REVIEW_H
#include "zcl_transaction_assess.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_REVIEW_ID_MAX UINT64_C(9223372036854775807)
#define ZCL_REVIEW_LIFETIME_MS UINT64_C(90000)

typedef struct {
    uint8_t previous_txid[32]; /* Displayed big-endian order. */
    uint32_t previous_index;
    uint32_t sequence;
} zcl_review_input;

/* Exact raw transaction fields, not interpreted finality/expiry/chain evidence.
 * Valid input rows are bounded by the accompanying assessment.input_count;
 * unused rows are zero. Destinations/values share that same input order. */
typedef struct {
    uint32_t lock_time;
    uint32_t expiry_height;
    zcl_review_input inputs[ZCL_TX_INPUT_MAX];
} zcl_review_context;

/* Private representation: only the functions below may modify these fields. */
typedef struct {
    uint64_t id;
    uint64_t last_ms;
    uint64_t deadline_ms;
    size_t wire_length;
    uint8_t wire[ZCL_TX_WIRE_MAX];
    zcl_transaction_assessment assessment;
    zcl_review_context context;
} zcl_review_data;

/* One active unsigned draft. Initialize {0} once per enclosing adapter lifetime;
 * never reset/copy while callbacks may retain an ID. Serialize ALL access under
 * the same adapter lock. This owns only public bytes and no pointer or heap.
 * IDs are local lifetime identities, not authentication or signing authority.
 */
typedef struct {
    uint64_t issued;
    zcl_review_data data;
} zcl_review_owner;

typedef struct {
    zcl_transaction_assessment assessment;
    zcl_review_context context;
    uint64_t remaining_ms;
} zcl_review_snapshot;

/* Prepare before publishing. Active state returns BUSY until cancelled or an
 * expiry read clears it. Failure leaves owner/id unchanged. Caller owns stable
 * inputs; output/owner must not overlap inputs or each other. Previous input
 * spans may share bytes. Reject input scripts: this lifetime is unsigned only.
 * The fixed deadline never extends. now_ms is trusted elapsed monotonic time,
 * not wall/server time; reject an opening whose deadline would overflow.
 */
zcl_status zcl_review_open(zcl_review_owner *owner, const uint8_t *wire, size_t length,
                           zcl_network network, const zcl_previous_transaction *previous,
                           size_t previous_count, uint64_t maximum_fee, uint64_t now_ms,
                           uint64_t *id);
/* Copy owned data only. Late IDs cannot read or expire a replacement. Valid
 * IDs expire at the deadline (TIMED_OUT), or cancel on clock rollback. Those
 * transitions clear retained draft data; failure leaves caller outputs intact.
 * The relative delay is a UI hint: every delivery must recheck current state.
 * No read grants consent, chain validity, ownership or authority to sign/send.
 */
zcl_status zcl_review_snapshot_get(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
                                   zcl_review_snapshot *snapshot);
zcl_status zcl_review_copy_wire(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
                                uint8_t *wire, size_t capacity, size_t *length);
zcl_status zcl_review_cancel(zcl_review_owner *owner, uint64_t id);
/* Background/lock/process teardown invalidates the draft without resetting IDs.
 * Never restore review state/IDs from Bundle, disk or an intent. */
void zcl_review_clear(zcl_review_owner *owner);

#ifdef __cplusplus
}
#endif
#endif
