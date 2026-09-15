/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_BIP32_ORACLE_H
#define ZCL_TEST_BIP32_ORACLE_H
#include "zcl_keys.h"

/* Host-only OpenSSL oracle, shared by fixed address vectors and HD fuzzing.
 * Public synthetic inputs only. No wallet hash, HD or curve implementation is
 * called. The shared types describe bytes/statuses, not algorithmic authority. */
int zcl_test_bip32_public(const uint8_t *secret, size_t length, uint8_t *out, size_t capacity);
zcl_status zcl_test_bip32_master(const uint8_t *seed, size_t length, zcl_extended_private *out);
zcl_status zcl_test_bip32_child(const zcl_extended_private *parent, uint32_t index,
    zcl_extended_private *out);
#endif
