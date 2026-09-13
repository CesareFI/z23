/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_DRAFT_INTERNAL_H
#define ZCL_TRANSACTION_DRAFT_INTERNAL_H
#include "zcl_transaction_draft.h"

/* Separate bounded frames for canonical previous parsing and final assessment.
 * Helpers write only the private candidate; no caller output is published here.
 */
zcl_status zcl_draft_bind_funding(const zcl_draft_funding *funding, zcl_tx_input *input);
zcl_status zcl_draft_assess(const zcl_draft_request *request, const zcl_transparent_tx *transaction);
#endif
