/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_DRAFT_INTERNAL_H
#define ZCL_TRANSACTION_DRAFT_INTERNAL_H
#include "zcl_transaction_draft.h"

/* Separate bounded frames for canonical previous parsing and final assessment.
 * Helpers write only the private candidate; no caller output is published here.
 */
zcl_status zcl_draft_bind_funding(const zcl_draft_funding *funding, zcl_tx_input *input);
zcl_status zcl_draft_assess(const zcl_draft_request *request, const zcl_transparent_tx *transaction);

/* Explicit full-v4 source profile. The request/current transaction limits,
 * exact row order, destination/network checks and absolute fee policy remain.
 * Each source is structural public data <=102000 bytes; proofs/signatures are
 * opaque and do not establish consensus, inclusion, maturity or unspentness.
 * Derive exact full-wire outpoint IDs, then reassess every source before whole
 * output publication. Stable nonoverlapping inputs; no heap, retained source,
 * implicit change, custody, consent, signing or broadcast authority. The legacy
 * builder keeps its original funding profile. Helpers write private scratch. */
zcl_status zcl_transaction_draft_full_sources(const zcl_draft_request *request,
    zcl_transparent_tx *transaction);
zcl_status zcl_draft_bind_full_funding(const zcl_draft_funding *funding, zcl_tx_input *input);
zcl_status zcl_draft_assess_full_sources(const zcl_draft_request *request, const zcl_transparent_tx *transaction);
#endif
