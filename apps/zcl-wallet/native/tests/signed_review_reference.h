/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SIGNED_REVIEW_REFERENCE_H
#define ZCL_SIGNED_REVIEW_REFERENCE_H
#include "signed_review_fixture.h"
/* Host-only public fixture oracle. Independent epoch traversal, reader/libsodium,
 * OpenSSL and bounded byte splicing; no wallet parser/serializer/signer calls.
 * baseline is an immutable fixture with empty scripts, lock0 and final sequences.
 * Output spans are fixed1925 bytes and stable/nonoverlapping. False may leave
 * partial reference bytes; only true supplies an expected complete candidate. */
bool signed_review_reference(const signed_review_fixture *baseline, const zcl_review_block *block,
    const zcl_signature *signatures, size_t count, uint8_t *wire, size_t *length);
#endif
