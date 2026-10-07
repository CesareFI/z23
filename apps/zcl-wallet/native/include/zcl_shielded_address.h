/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SHIELDED_ADDRESS_H
#define ZCL_SHIELDED_ADDRESS_H

#include "zcl_wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ZCL_SPROUT_ADDRESS = 1, ZCL_SAPLING_ADDRESS = 2 } zcl_shielded_address_kind;
typedef struct {
    zcl_network network;
    zcl_shielded_address_kind kind;
    /* Sprout: a_pk || pk_enc (64 bytes). Sapling: d || pk_d (43 bytes),
     * remaining bytes zero. These serialized bytes contain no private keys. */
    uint8_t bytes[64];
} zcl_shielded_address;

/* Public envelope decoding ONLY: checksum, encoding, length and main/test
 * prefix. No point/diversifier validation, ownership, spendability, chain
 * identity or payment authorization is established. Prefixes are shared with
 * other chains. Does not enable shielded sending/receiving or alter the
 * transparent payment parser. Regtest Sapling addresses are not testnet.
 *
 * Stable caller-owned, nonoverlapping input/output; explicit text length,
 * no terminator needed. No allocation, pointer retention, input mutation,
 * secrets or logging. Every failure leaves the entire output unchanged.
 * Accepts uniform lower/upper Sapling case, rejects mixed case and whitespace.
 */
zcl_status zcl_shielded_address_parse(const uint8_t *text, size_t text_len,
                                    zcl_network network, zcl_shielded_address *address);

#ifdef __cplusplus
}
#endif
#endif
