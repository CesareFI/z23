/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_RECOVERY_H
#define ZCL_RECOVERY_H
#include "zcl_keys.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_RECOVERY_BATCH_MAX ((size_t)16)
#define ZCL_RECOVERY_ADDRESS_BYTES ((size_t)35)

/* Derive 1..16 consecutive public account-0 addresses for offline recovery:
 * m/44'/147'/0'/chain/index (coin1 on testnet), chain0 receive / chain1 change.
 * Every requested index must be <2^31. Never skip an invalid child.
 * Output is count adjacent 35-byte addresses, without terminators; unused
 * capacity and all output on failure remain unchanged. Borrowed spans must
 * be stable and nonoverlapping. No pointer or secret is retained after return.
 * Supply 32 independent OS-random blinding bytes per batch operation. One
 * bounded invocation-local EC context and seed are reused, then cleared.
 * This derives addresses only: no balance discovery, ownership admission,
 * journal mutation, index reservation, custody or spending authority. */
zcl_status zcl_recovery_address_batch(const uint8_t *entropy, size_t entropy_len,
    zcl_network network, uint32_t chain, uint32_t first, size_t count,
    const uint8_t *blinding, size_t blinding_len, uint8_t *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
