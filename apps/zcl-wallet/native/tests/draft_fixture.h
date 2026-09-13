/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_DRAFT_FIXTURE_H
#define ZCL_DRAFT_FIXTURE_H
#include "assessment_fixture.h"
#include "zcl_transaction_draft.h"

/* Caller keeps the large public backing fixture alive and outside production
 * stack frames. Request borrows its wire spans; neither contains wallet keys. */
bool draft_fixture_init(zcl_draft_request *request, assessment_fixture *fixture, zcl_network network);
#endif
