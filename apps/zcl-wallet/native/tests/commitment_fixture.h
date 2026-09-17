/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_COMMITMENT_FIXTURE_H
#define ZCL_COMMITMENT_FIXTURE_H
#include "source_commitment_internal.h"
#include "source_assessment_fixture.h"
typedef struct {
    source_assessment_fixture funding;
    uint8_t header[543];
    zcl_merkle_branch branch;
    zcl_source_commitment_request request;
} commitment_fixture;
/* Public synthetic wire/opaque subtrees, NOT valid block/proof fixtures. */
bool commitment_fixture_init(commitment_fixture *fixture, unsigned tail, uint32_t width, uint32_t index);
bool commitment_fixture_bind(commitment_fixture *fixture);
#endif
