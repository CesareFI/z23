/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_KEYS_H
#define ZCL_KEYS_H

#include "zcl_wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCL_ENTROPY_MAX ((size_t)32)
#define ZCL_MNEMONIC_MAX ((size_t)215)
#define ZCL_SEED_BYTES ((size_t)64)
#define ZCL_PASSPHRASE_MAX ((size_t)128)

/* Secret spans remain caller-owned. No pointers are retained. Mnemonic APIs
 * allocate no heap storage; EC operations own one bounded transient context
 * allocation which is destroyed, cleared and freed before returning.
 * Caller must clear secret inputs/outputs after use.
 * Output remains unchanged on error. Input/output spans must not overlap.
 * Text lengths exclude a terminator; no API appends a NUL byte.
 */
zcl_status zcl_mnemonic_encode(const uint8_t *entropy, size_t entropy_len,
                              uint8_t *text, size_t text_capacity, size_t *text_len);
zcl_status zcl_mnemonic_decode(const uint8_t *text, size_t text_len,
                              uint8_t *entropy, size_t entropy_capacity, size_t *entropy_len);
/* Check backup confirmation without returning decoded entropy. Comparison
 * examines every entropy byte; no mismatch position or secret is exposed. */
zcl_status zcl_mnemonic_confirm(const uint8_t *entropy, size_t entropy_len,
                               const uint8_t *text, size_t text_len);

/* English, canonical single-space mnemonic and checksum required. Passphrase
 * accepts printable ASCII only (already NFKD); unsupported Unicode fails.
 * An empty passphrase still requires a valid pointer with length zero.
 * Exactly the BIP39 2048 iterations and 64-byte seed; no custom KDF profile.
 */
zcl_status zcl_mnemonic_seed(const uint8_t *text, size_t text_len,
                            const uint8_t *passphrase, size_t passphrase_len,
                            uint8_t *seed, size_t seed_capacity);

/* Clear an owned writable span using the provider's optimization-resistant
 * primitive. NULL is a no-op, allowing unconditional cleanup of optional
 * owned spans. A non-NULL pointer must designate buffer_length writable bytes.
 */
void zcl_secure_zero(void *buffer, size_t buffer_length);

/* OS CSPRNG only, 1..64 bytes per call. No fallback entropy. Failure leaves
 * caller output unchanged. These random bytes are secret and caller-owned. */
zcl_status zcl_random_bytes(uint8_t *output, size_t output_length);

typedef struct {
    uint8_t secret[32];
    uint8_t chain_code[32];
} zcl_extended_private;

/* BIP32 nodes are secret, caller-owned values; clear with zcl_secure_zero.
 * Master seed is 16..64 bytes. Child derivation returns ZCL_INVALID_CHILD for
 * the rare invalid result; it never silently changes the requested index.
 * A caller may explicitly try the next index in the same hardened/normal range.
 */
zcl_status zcl_bip32_master(const uint8_t *seed, size_t seed_len, zcl_extended_private *output);
zcl_status zcl_bip32_child(const zcl_extended_private *parent, uint32_t index,
                          const uint8_t *blinding, size_t blinding_len,
                          zcl_extended_private *output);
zcl_status zcl_public_key(const uint8_t *secret, size_t secret_len,
                         const uint8_t *blinding, size_t blinding_len,
                         uint8_t *public_key, size_t public_key_capacity);

/* Blinding must be 32 independent OS-random bytes per operation, even though
 * it does not affect the derived public result. Fixed blinding is test-only.
 * This API has no signing operation and never publishes a private key. */
zcl_status zcl_receive_from_entropy(const uint8_t *entropy, size_t entropy_len,
                                    zcl_network network, uint32_t index,
                                    const uint8_t *blinding, size_t blinding_len,
                                    uint8_t *address, size_t address_capacity, size_t *address_len);

#ifdef __cplusplus
}
#endif
#endif
