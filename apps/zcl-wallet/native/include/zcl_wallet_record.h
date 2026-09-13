/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_WALLET_RECORD_H
#define ZCL_WALLET_RECORD_H
#include "zcl_keys.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_WALLET_HEADER_BYTES ((size_t)80)
#define ZCL_WALLET_IV_BYTES ((size_t)12)
#define ZCL_WALLET_TAG_BYTES ((size_t)16)
#define ZCL_WALLET_RECORD_MAX ((size_t)140)

typedef struct {
    zcl_network network;
    size_t entropy_len;
    uint8_t address[35];
} zcl_wallet_info;

typedef struct {
    uint8_t header[80];
    uint8_t iv[12];
    uint8_t ciphertext[48];
    size_t ciphertext_len;
    zcl_wallet_info info;
} zcl_wallet_record;

/* Public metadata/ciphertext only. Parsing does not establish authenticity.
 * All buffers/structures are caller-owned; no pointer escapes. Output is
 * unchanged on failure. Input/output must not overlap. See WALLET_RECORD.md.
 */
zcl_status zcl_wallet_header_parse(const uint8_t *header, size_t header_len, zcl_wallet_info *info);
zcl_status zcl_wallet_header_create(const uint8_t *entropy, size_t entropy_len,
                                    zcl_network network, const uint8_t *blinding, size_t blinding_len,
                                    uint8_t *header, size_t header_capacity);
zcl_status zcl_wallet_record_pack(const uint8_t *header, size_t header_len,
                                  const uint8_t *iv, size_t iv_len,
                                  const uint8_t *ciphertext, size_t ciphertext_len,
                                  uint8_t *record, size_t capacity, size_t *record_len);
zcl_status zcl_wallet_record_parse(const uint8_t *record, size_t record_len, zcl_wallet_record *output);

/* Platform MUST authenticate GCM before this call. It verifies entropy/profile
 * agreement and returns the re-derived public address, avoiding a later read
 * of mutable stored metadata. It does not replace the platform's GCM check. */
zcl_status zcl_wallet_recovered_address(const uint8_t *header, size_t header_len,
                                       const uint8_t *entropy, size_t entropy_len,
                                       const uint8_t *blinding, size_t blinding_len,
                                       uint8_t *address, size_t capacity);

#define ZCL_WALLET_CHANGE_BLINDING_BYTES ((size_t)64)
/* Platform MUST first authenticate this exact header/entropy with GCM.
 * Verify the v1 recovery profile and external0 wallet address, then derive
 * internal chain1/index on that same network/account. Index must be <2^31.
 * blinding contains two independent32-byte OS-random values, one for each
 * derivation; fixed values are test-only. The caller owns/clears secret spans.
 * Writes exactly35 public ASCII bytes, no terminator, only on success.
 * Stable input/output spans must not overlap; no pointer/secret is retained.
 * This does not authenticate GCM itself, reserve an index, classify an output
 * as change, prove chain funding, or authorize any transaction/signature. */
zcl_status zcl_wallet_recovered_change(const uint8_t *header, size_t header_len,
                                      const uint8_t *entropy, size_t entropy_len,
                                      uint32_t index, const uint8_t *blinding, size_t blinding_len,
                                      uint8_t *address, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
