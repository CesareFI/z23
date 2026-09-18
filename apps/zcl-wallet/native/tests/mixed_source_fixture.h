/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MIXED_SOURCE_FIXTURE_H
#define ZCL_MIXED_SOURCE_FIXTURE_H
#include "source_assessment_fixture.h"
/* Public synthetic profiles:0=v1,1/2=v2 without/with PHGR,3/4=v3 without/with
 * PHGR,5..12=v4 section masks0..7. Opaque proofs/signatures are invalid. */
bool mixed_source_init(source_assessment_fixture *fixture, unsigned first, unsigned second);
bool mixed_source_extend(source_assessment_fixture *fixture, size_t index, unsigned profile);
#endif
