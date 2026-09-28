/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_PAYMENT_HOST_ASSEMBLE_H
#define ZCL_BLUE_PAYMENT_HOST_ASSEMBLE_H

#include "blue_payment_host_verify.h"

/* Replaces every empty transparent input script in a reviewed Sapling-v4
 * all-transparent wire with its verified P2PKH signature and public key.
 * Output must not overlap the unsigned wire, signatures, or expected digests.
 * Failure leaves output_length zero; callers must never publish the output
 * buffer after a failure. */
bool blue_payment_host_assemble(const uint8_t *unsigned_wire,
    size_t unsigned_length,
    const blue_payment_verified_signature *signatures,
    const uint8_t (*expected_digests)[32], size_t count,
    uint8_t *output, size_t capacity, size_t *output_length);

#endif
