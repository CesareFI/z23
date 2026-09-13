/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_ELECTRUM_H
#define ZCL_ELECTRUM_H
#include "zcl_wallet.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_ELECTRUM_FRAME_MAX ((size_t)16384)
#define ZCL_ELECTRUM_REQUEST_MAX ((size_t)256)
#define ZCL_ELECTRUM_HISTORY_MAX ((size_t)16)
typedef enum {
    ZCL_ELECTRUM_VERSION = 0, ZCL_ELECTRUM_FEATURES, ZCL_ELECTRUM_GENESIS,
    ZCL_ELECTRUM_TIP, ZCL_ELECTRUM_BALANCE, ZCL_ELECTRUM_HISTORY
} zcl_electrum_method;

/* All functions below are synchronous, allocation-free, read-only protocol
 * operations. No keys, I/O, sockets, implicit text lengths or saved pointers.
 * Caller spans are stable and non-overlapping for the call. Public outputs
 * remain unchanged on failure. A valid response is NOT a consensus proof.
 * Protocol baseline: Electrum 1.2, as pinned in docs/READ_ONLY_SYNC.md. */
zcl_status zcl_electrum_script_hash(const uint8_t *address, size_t address_len,
                                     zcl_network network, uint8_t *text, size_t capacity);
/* address may be NULL/0 except for BALANCE/HISTORY; IDs must be nonzero.
 * Output is an exact JSON request including one trailing LF, no trailing NUL. */
zcl_status zcl_electrum_request(zcl_electrum_method method, uint32_t id,
                                const uint8_t *address, size_t address_len, zcl_network network,
                                uint8_t *text, size_t capacity, size_t *length);
zcl_status zcl_electrum_version_reply(const uint8_t *frame, size_t length, uint32_t id);
zcl_status zcl_electrum_features_reply(const uint8_t *frame, size_t length, uint32_t id,
                                       zcl_network network);
/* Checks an independently pinned genesis hash, not subsequent chain truth. */
zcl_status zcl_electrum_genesis_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_network network);

typedef struct {
    uint64_t confirmed;
    int64_t pending_delta;
    uint64_t total;
} zcl_reported_balance;
zcl_status zcl_electrum_balance_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_reported_balance *balance);

typedef struct {
    uint8_t txid[32]; /* displayed big-endian hash; no transaction is verified */
    int32_t reported_height; /* -1: unconfirmed parent, 0: mempool, >0: claimed block */
} zcl_reported_history_entry;
typedef struct {
    size_t count;
    zcl_reported_history_entry entries[ZCL_ELECTRUM_HISTORY_MAX];
} zcl_reported_history;
/* Parses a complete bounded response, preserving server order. More than 16
 * entries refuses instead of truncating. Duplicate binary txids are invalid.
 * Heights are -1..INT32_MAX, matching the supported tip range; -2 is local-only
 * in the pinned client and is not accepted from a server. Unknown bounded
 * fields (including fee) are syntax-checked but not returned. This supplies no
 * history completeness, inclusion, confirmation, value or spending authority.
 * Empty success means only that the server returned an empty array. */
zcl_status zcl_electrum_history_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_reported_history *history);

typedef struct {
    uint32_t height;
    uint8_t hash[32]; /* displayed big-endian hash of the returned raw header */
} zcl_reported_tip;
zcl_status zcl_electrum_tip_reply(const uint8_t *frame, size_t length, uint32_t id,
                                  zcl_network network, zcl_reported_tip *tip);

/* Initialize with reset before first use; no concurrent access. One caller-
 * owned framing buffer; reset clears its bytes. feed consumes at
 * most one line and reports consumed input bytes on success. The caller keeps
 * any unconsumed suffix. ready makes the buffer immutable until reset. A frame
 * overflow makes the state sticky-failed until reset; argument errors leave
 * the state unchanged. Do not allocate this struct on an
 * Android thread stack; its owner supplies bounded heap or enclosing storage. */
typedef struct {
    uint8_t bytes[ZCL_ELECTRUM_FRAME_MAX];
    size_t used;
    bool ready, failed;
} zcl_electrum_line;
void zcl_electrum_line_reset(zcl_electrum_line *line);
zcl_status zcl_electrum_line_feed(zcl_electrum_line *line, const uint8_t *input,
                                  size_t length, size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif
