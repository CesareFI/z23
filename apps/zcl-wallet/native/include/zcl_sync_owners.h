/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SYNC_OWNERS_H
#define ZCL_SYNC_OWNERS_H
#include "zcl_sync_watch.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_SYNC_OWNER_CAPACITY ((size_t)4)
/* Positive IDs fit Java long without a signed conversion or pointer cast. */
#define ZCL_SYNC_OWNER_ID_MAX UINT64_C(9223372036854775807)

typedef struct {
    uint64_t id; /* Zero denotes a free slot. */
    zcl_sync_watch watch;
} zcl_sync_owner_slot;

/* Caller-owned, bounded public sync state. Initialize to {0} exactly once per
 * enclosing adapter lifetime. No allocation, global state, secrets or sockets.
 * All fields are private to these functions. Serialize ALL access, including
 * use of a borrowed watch, on one worker or under the same adapter lock.
 * Never copy/reset this pool while any callback may retain an ID. */
typedef struct {
    uint64_t issued;
    zcl_sync_owner_slot slots[ZCL_SYNC_OWNER_CAPACITY];
} zcl_sync_owners;

/* Reserve one slot and a never-reused positive owner ID. Failure leaves the
 * pool and output unchanged. Input/output spans must not overlap the pool or
 * each other. Releasing a slot does not reset its ID history.
 * Owner IDs identify this pool's lifetimes, not authentication or chain truth. */
zcl_status zcl_sync_owners_open(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id);
/* Same pool and nonreused IDs, with history fixed on for this owner lifetime. */
zcl_status zcl_sync_owners_open_with_history(zcl_sync_owners *owners, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source, size_t source_length,
    uint64_t *id);
/* A borrowed watch must stay within the caller's serialized synchronous call.
 * No borrowed pointer may cross JNI or escape into an asynchronous callback.
 * Every callback looks up its owner ID again before using its attempt token.
 * Invalid/closed IDs return CANCELLED and leave the output unchanged. */
zcl_status zcl_sync_owners_get(zcl_sync_owners *owners, uint64_t id, zcl_sync_watch **watch);
zcl_status zcl_sync_owners_close(zcl_sync_owners *owners, uint64_t id);
/* Clear public reports and attempts, retaining the monotonically issued ID.
 * This invalidates every outstanding callback without reusing any old ID. */
void zcl_sync_owners_close_all(zcl_sync_owners *owners);

#ifdef __cplusplus
}
#endif
#endif
