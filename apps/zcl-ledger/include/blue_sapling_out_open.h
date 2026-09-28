/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_SAPLING_OUT_OPEN_H
#define ZCL_BLUE_SAPLING_OUT_OPEN_H

#include <stdbool.h>
#include <stdint.h>

/* Authenticate and decrypt a Sapling 80-byte outgoing ciphertext under a
 * previously derived OCK. The 64-byte result is pk_d || esk. Failure clears
 * the result. Inputs and output must be disjoint; the caller must erase the
 * OCK and result after checking epk and the note commitment. */
bool blue_sapling_out_open(uint8_t plaintext[64], const uint8_t key[32],
    const uint8_t ciphertext[80]);

#endif
