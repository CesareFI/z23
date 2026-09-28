/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_SAPLING_OCK_H
#define ZCL_BLUE_SAPLING_OCK_H

#include "zcl_zip243.h"

/* Derive a Sapling outgoing cipher key. The output must not overlap an input.
 * Failure clears the output. The caller clears the result and hasher context
 * after use because both can contain key material. */
bool blue_sapling_ock(uint8_t key[32], const uint8_t ovk[32],
    const uint8_t cv[32], const uint8_t cm[32], const uint8_t epk[32],
    const zcl_zip243_hasher *hasher);

#endif
