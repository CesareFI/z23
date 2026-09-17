/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_SOURCE_INTERNAL_H
#define ZCL_TRANSACTION_SOURCE_INTERNAL_H
#include "zcl_transaction_assess.h"

/* Pinned original Zclassic v4 serialized transaction limit, NOT a Zcash SDK
 * limit. This reader inspects public bytes only and never authorizes funding.
 * The existing spend codec, prevout admission and JNI limits are unchanged. */
#define ZCL_V4_SOURCE_MAX ((size_t)102000)
#define ZCL_LEGACY_SOURCE_MAX ((size_t)100000)
typedef struct {
    uint8_t transaction_id[32]; /* Full raw wire SHA256d, displayed order. */
    zcl_tx_output output;       /* Selected transparent row, owned script. */
    size_t input_count, output_count, spend_count, shielded_count, joinsplit_count;
    uint32_t lock_time, expiry_height;
    int64_t value_balance;     /* Raw signed field, not verified accounting. */
} zcl_source_view;
typedef zcl_source_view zcl_v4_source; /* Preserve the existing explicit v4 profile. */

/* Parse the complete canonical v4 layout, including opaque Sapling descriptions,
 * Groth JoinSplits and their conditional signatures. Transparent script lengths
 * and vector counts are bounded by remaining bytes, not the small spend profile.
 * Only the selected output script must fit zcl_tx_output (<=25 bytes). All
 * transparent output values and their sum must be within MAX_MONEY.
 * Full byte/hash/index consistency only: NO proof, signature, script, duplicate
 * outpoint/nullifier, maturity, finality, inclusion or unspentness validation.
 * Legacy/v3/NU5/Orchard refuse. No global state, heap, retained pointer or I/O.
 * Caller owns stable nonoverlapping spans. Entire output unchanged on failure.
 * This is not a replacement for zcl_transaction_prevout admission. */
zcl_status zcl_v4_source_inspect(const uint8_t *wire, size_t length,
    uint32_t output_index, zcl_v4_source *output);

/* Explicit historical profile: v1, v2 and Overwinter v3 only. Reuses owned
 * source fields, with absent expiry/Sapling fields zero. Canonical PHGR point
 * prefixes are serialization checks, not curve/proof verification. Bound100000
 * follows the pinned original pre-Sapling source limit. No v4 admission here;
 * no existing prevout, draft, review, commitment or signing profile is widened.
 * Same structural-only, stable-span and failure-atomic contract as above. */
zcl_status zcl_legacy_source_inspect(const uint8_t *wire, size_t length,
    uint32_t output_index, zcl_source_view *output);

/* Match complete source identity/index before publishing its owned output.
 * Same structural-only contract; the expected outpoint is caller-supplied and
 * is not authenticated by this operation. Existing prevout admission is intact. */
zcl_status zcl_v4_source_prevout(const zcl_tx_input *input,
    const uint8_t *wire, size_t length, zcl_tx_output *output);

/* Explicit offline DATA assessment with full-v4 sources. The current spending
 * transaction still satisfies every existing bounded transparent predicate;
 * only source inspection uses the wider, structural-only contract above.
 * Matches every source hash/index, exact P2PKH/P2SH templates, checked totals
 * and the explicit fee ceiling. No review owner, signing or chain authority.
 * At most8 borrowed sources, each <=102000; no allocation/retained pointers.
 * Caller owns stable nonoverlapping spans. Whole report unchanged on failure. */
zcl_status zcl_v4_source_assess(const zcl_transparent_tx *transaction,
    zcl_network network, const zcl_previous_transaction *previous,
    size_t previous_count, uint64_t maximum_fee, zcl_transaction_assessment *assessment);
#endif
