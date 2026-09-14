/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_SIGHASH_ORACLE_H
#define ZCL_TEST_SIGHASH_ORACLE_H
#include <stddef.h>
#include <stdint.h>

/* Host fixture only. Independently parse a known bounded valid v4 wire fixture
 * and compute raw SIGHASH_ALL with libsodium. Invalid fixture data aborts the
 * isolated test process. No wallet parser/provider or key/chain authority.
 * Stable nonoverlapping caller spans: wire<=11000, script<=128, output>=32;
 * input index must exist and amount must be <=2100000000000000. NULL refuses.
 */
void zcl_test_sighash_all(const uint8_t *wire, size_t wire_length, size_t input_index,
    const uint8_t *script, size_t script_length, uint64_t amount, uint32_t branch,
    uint8_t *digest, size_t capacity);
#endif
