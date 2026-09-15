/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BIP32_INTERNAL_H
#define ZCL_BIP32_INTERNAL_H
#include "ec_context.h"
zcl_status zcl_bip32_step(const zcl_extended_private *parent, uint32_t index,
                          const secp256k1_context *context, zcl_extended_private *output);
/* Invocation-private seed reuse. Caller owns/clears seed and owns the blinded
 * context until each address call returns. Neither helper retains a span or
 * context, changes an index, or authenticates wallet ownership. */
zcl_status zcl_entropy_seed(const uint8_t *entropy, size_t entropy_len,
    uint8_t *seed, size_t capacity);
zcl_status zcl_seed_address(const uint8_t *seed, size_t seed_len, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context,
    uint8_t *address, size_t capacity, size_t *length);
#endif
