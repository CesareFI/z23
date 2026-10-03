/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SYNC_WATCH_H
#define ZCL_SYNC_WATCH_H
#include "zcl_sync.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_SYNC_TIMEOUT_MAX_MS UINT64_C(30000)
#define ZCL_SYNC_FRESH_MS UINT64_C(60000)
typedef enum {
    ZCL_BALANCE_UNAVAILABLE = 0, ZCL_BALANCE_UNVERIFIED, ZCL_BALANCE_STALE
} zcl_balance_freshness;

typedef struct {
    zcl_balance_freshness freshness;
    bool refreshing;
    zcl_status last_fault;
    uint64_t age_ms; /* Zero with UNAVAILABLE; report is then entirely zero. */
    /* Relative delay from this snapshot's clock sample until the next timed
     * freshness/deadline change. Zero means none pending. At most FRESH_MS.
     * A timer is only a wakeup hint: read a new snapshot when it actually runs. */
    uint64_t next_change_ms;
    uint8_t source_id[32]; /* Opaque caller-selected configuration identity. */
    zcl_sync_report report;
} zcl_sync_snapshot;

/* One worker exclusively owns one foreground lifetime, address and selected
 * source. No allocation, retained pointers, timers, sockets, persistence or
 * automatic retries. All fields are private to these functions.
 * Source ID is caller metadata, never server authentication or a chain proof.
 * Every asynchronous operation must retain its owner lifetime AND token. Close
 * that owner before destruction/source change; never route an old callback to
 * a newly initialized watch even if its numeric token matches. Restart creates
 * an empty owner. No snapshot may be restored as fresh state from disk. */
typedef struct {
    zcl_sync attempt;
    zcl_sync_report last;
    uint8_t address[35], source_id[32];
    zcl_network network;
    uint64_t sequence, deadline_ms, clock_ms, observed_ms;
    zcl_status last_fault;
    bool initialized, in_flight, has_report, include_history;
} zcl_sync_watch;

/* Initialize once per owner lifetime. Nonoverlapping, caller-owned spans must
 * be stable during each synchronous call. Failure clears a non-NULL watch. */
zcl_status zcl_sync_watch_init(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length);
/* Opt-in history is fixed for the entire owner lifetime. It shares all source,
 * token, deadline, freshness and empty-restart rules with the balance report.
 * A bounded-history refusal fails the whole attempt; no partial report appears. */
zcl_status zcl_sync_watch_init_with_history(zcl_sync_watch *watch, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source_id, size_t source_length);
void zcl_sync_watch_close(zcl_sync_watch *watch);
/* now_ms comes from one monotonic clock, including suspended time, throughout
 * this owner lifetime. Begin reserves six IDs (seven with history) on a NEW connection; the adapter
 * must close every old connection and require authenticated TLS independently.
 * Deadline includes connection setup and all responses. No wrapping arithmetic.
 * Token output stays unchanged on failure. Only one attempt can be active. */
zcl_status zcl_sync_watch_begin(zcl_sync_watch *watch, uint64_t now_ms, uint64_t timeout_ms,
    uint32_t first_id, uint64_t *token);
/* Read-only active-token check for adapters before allocating/copying input.
 * Same owner lifetime/exclusive access as every other operation. OK does not
 * check time or admit a reply; request/reply still enforce their full contract.
 * No watch state, clock or output changes, including on refusal. */
zcl_status zcl_sync_watch_check_attempt(const zcl_sync_watch *watch, uint64_t token);
/* Old/completed/cancelled tokens return CANCELLED without mutating current
 * state or outputs, including without advancing its clock. A current attempt
 * fails at now_ms >= deadline; a backward clock discards cached reports too. */
zcl_status zcl_sync_watch_request(zcl_sync_watch *watch, uint64_t token, uint64_t now_ms,
    uint8_t *output, size_t capacity, size_t *length);
zcl_status zcl_sync_watch_reply(zcl_sync_watch *watch, uint64_t token, uint64_t now_ms,
    const uint8_t *frame, size_t length);
/* A current token accepts only a declared non-OK zcl_status reason. Invalid
 * reasons leave the active attempt unchanged. */
zcl_status zcl_sync_watch_fail(zcl_sync_watch *watch, uint64_t token, zcl_status reason);
/* Polling also expires the deadline. Retained reports are STALE during refresh,
 * after failure/offline/cancellation, or at age >= 60s. Even fresh reports are
 * UNVERIFIED and carry no spending authority. Closed/invalid output is unchanged. */
zcl_status zcl_sync_watch_snapshot(zcl_sync_watch *watch, uint64_t now_ms,
    zcl_sync_snapshot *snapshot);
#ifdef __cplusplus
}
#endif
#endif
