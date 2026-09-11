/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BIP32_INTERNAL_H
#define ZCL_BIP32_INTERNAL_H
#include "ec_context.h"
zcl_status zcl_bip32_step(const zcl_extended_private *parent, uint32_t index,
                          const secp256k1_context *context, zcl_extended_private *output);
#endif
