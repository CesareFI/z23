/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SYNC_H
#define ZCL_SYNC_H
#include "zcl_electrum.h"
#ifdef __cplusplus
extern "C" {
#endif

/* One transparent address only. This is a server report, never an account
 * balance, UTXO set, chain proof, confirmation proof or spending authority. */
typedef struct {
    uint8_t address[35];
    zcl_network network;
    zcl_reported_balance balance;
    zcl_reported_tip tip;
    bool has_history; /* false means no history query, not an empty history */
    zcl_reported_history history;
} zcl_sync_report;

typedef enum {
    ZCL_SYNC_IDLE = 0, ZCL_SYNC_VERSION, ZCL_SYNC_FEATURES, ZCL_SYNC_GENESIS,
    ZCL_SYNC_TIP_BEFORE, ZCL_SYNC_BALANCE, ZCL_SYNC_TIP_AFTER, ZCL_SYNC_DONE,
    ZCL_SYNC_FAILED, ZCL_SYNC_HISTORY /* appended to preserve existing phase values */
} zcl_sync_phase;

/* Caller-owned, single-worker state with no retained pointers, allocation,
 * sockets, timestamps, secrets or authentication authority. Fields are private
 * to these functions; initialize with start before any other operation.
 * One instance belongs to one connection and request-ID range. Never feed a
 * reply from a different connection or retry using a previous instance. */
typedef struct {
    zcl_sync_report candidate;
    uint32_t request_id;
    zcl_sync_phase phase;
    zcl_status fault;
    bool waiting, include_history;
} zcl_sync;

/* Copies a validated address/network. Always initializes a non-NULL session,
 * including failures, which leave it in FAILED state. Reinitialization drops
 * all prior progress. Input/session spans must not overlap.
 * Reserve six nonzero request IDs: first_id must be <= UINT32_MAX - 5. */
zcl_status zcl_sync_start(zcl_sync *session, const uint8_t *address, size_t address_length,
                           zcl_network network, uint32_t first_id);
/* Opt-in seven-response profile: balance, then bounded history, then final tip.
 * Reserve seven IDs, first_id <= UINT32_MAX-6. The default start stays balance-
 * only; unavailable/oversized history cannot block that separate profile. */
zcl_status zcl_sync_start_with_history(zcl_sync *session, const uint8_t *address,
    size_t address_length, zcl_network network, uint32_t first_id);
/* Only one outstanding request. A failed output-buffer check is retryable and
 * leaves both session and output unchanged. Address queries become possible
 * only after protocol, network/genesis and initial-tip replies are accepted.
 * The adapter must separately require authenticated TLS before transmission. */
zcl_status zcl_sync_request(zcl_sync *session, uint8_t *output, size_t capacity, size_t *length);
/* Strict expected-ID response handling. Unexpected, malformed or failed
 * responses poison the attempt. Header notifications also invalidate this
 * one-shot attempt; a future adapter may classify bounded notifications before
 * this boundary. It must never route one as a reply. A changed final tip fails
 * with IO_UNCERTAIN, publishing no balance. No automatic retries occur here. */
zcl_status zcl_sync_reply(zcl_sync *session, const uint8_t *frame, size_t length);
/* Cancellation/disconnect discards candidate amounts/history, even after DONE. */
zcl_status zcl_sync_abort(zcl_sync *session, zcl_status reason);
/* Publishes only after all selected replies and equal before/after tip height/hash.
 * Optional history's positive claimed heights must not exceed the returned tip.
 * This consistency check cannot establish the truth of server claims. */
zcl_status zcl_sync_get_report(const zcl_sync *session, zcl_sync_report *report);
#ifdef __cplusplus
}
#endif
#endif
