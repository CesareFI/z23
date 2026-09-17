/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SOURCE_ASSESSMENT_FIXTURE_H
#define ZCL_SOURCE_ASSESSMENT_FIXTURE_H
#include "assessment_fixture.h"
/* Synthetic opaque proofs/signatures are deliberately invalid. Public byte
 * and fee fixtures only; never chain inclusion or spendability evidence. */
typedef struct {
    assessment_fixture base;
    uint8_t wire[2][4096];
} source_assessment_fixture;
bool source_assessment_init(source_assessment_fixture *fixture, unsigned tail);
bool source_assessment_extend(source_assessment_fixture *fixture, size_t index, unsigned tail);
bool source_assessment_hash(const uint8_t *wire, size_t length, uint8_t id[32]);
#endif
