/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_ASSESSMENT_FIXTURE_H
#define ZCL_ASSESSMENT_FIXTURE_H
#include "zcl_transaction_assess.h"
/* Public synthetic funding, never a wallet or spendable outpoint. Store this
 * large test-only fixture outside production/thread stack frames. */
typedef struct {
    zcl_transparent_tx previous[2];
    zcl_transparent_tx spending;
    uint8_t wire[2][ZCL_TX_WIRE_MAX];
    zcl_previous_transaction sources[ZCL_TX_INPUT_MAX];
} assessment_fixture;
bool assessment_fixture_init(assessment_fixture *fixture);
bool assessment_fixture_rebind(assessment_fixture *fixture, size_t index);
#endif
