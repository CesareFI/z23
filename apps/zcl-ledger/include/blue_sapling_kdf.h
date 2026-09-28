/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef BLUE_SAPLING_KDF_H
#define BLUE_SAPLING_KDF_H

#include "zcl_zip243.h"

/* Derive a Sapling note cipher key from a supplied DH result and epk.
 * The output must not overlap an input. Failure clears the output. The caller
 * verifies the output description and clears the result and hasher context
 * after use. */
bool blue_sapling_kdf(uint8_t key[32], const uint8_t dh[32],
    const uint8_t epk[32], const zcl_zip243_hasher *hasher);

#endif
