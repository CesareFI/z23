/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Box-level receiver index of signed observation leaves.
 *
 * Canonical lifecycle item 3 (docs/work/FORWARD_PLAN.md): reuse of
 * unchanged unit evidence across candidates is permitted "only when its
 * exact input closure and receiver policy permit it". The receiver's half
 * of that contract is a durable, verified, receiver-local basis of
 * observations seen on this box — the index the reuse admission and the
 * coverage queries classify against.
 *
 * After a proof publishes, the worker merges the pair's durable observation
 * CAS into this index. Every row is a self-verifying (root, wire) pair:
 * the root re-derives from the wire, so a tampered row is named on read
 * and never silently re-derived. Contradictory and ineligible leaves are
 * preserved, not collapsed: classification time refuses unresolved
 * eligible conflicts, exactly like the per-pair projection.
 *
 * The index is a rebuildable projection, not authority: merge failures are
 * named and reported but never block an already-proven publication. */

#ifndef ZCL_DEV_PROOF_OBSERVATION_INDEX_H
#define ZCL_DEV_PROOF_OBSERVATION_INDEX_H

#include "dev_proof_observation_walk.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Index wire: magic[8] "Z23OIDX\0" || version u32 = 1 || row_count u32 ||
 * reserved[16] = zero || rows sorted by root, each root[32] || leaf wire
 * [ZCL_DEV_VERDICT_LEAF_WIRE_BYTES]. */
#define ZCL_DEV_OBSERVATION_INDEX_WIRE_BYTES 32u
#define ZCL_DEV_OBSERVATION_INDEX_ROW_BYTES \
    (ZCL_DEV_PROOF_ROOT_BYTES + ZCL_DEV_VERDICT_LEAF_WIRE_BYTES)
#define ZCL_DEV_OBSERVATION_INDEX_MAX_ROWS 8192u

#define ZCL_DEV_OBSERVATION_INDEX_WHY_ARGUMENTS "observation_index_arguments"
#define ZCL_DEV_OBSERVATION_INDEX_WHY_INVALID "observation_index_invalid"
#define ZCL_DEV_OBSERVATION_INDEX_WHY_TOO_MANY "observation_index_too_many"

/* Merge every leaf of one per-pair observation CAS into the box index.
 * Read-modify-write under an exclusive lock; existing rows are verified
 * before the union and the result is written atomically. An absent store
 * merges nothing; an absent index is created. Refuses by name on any
 * unverifiable byte; the caller decides whether the refusal is fatal (the
 * proof worker treats it as advisory). */
bool zcl_dev_observation_index_merge(const char *index_path,
    const char *store_root, char *why, size_t why_len);

/* Load and verify the index into observation leaves (root re-derived,
 * signature eligibility decided per leaf). An absent index is not an
 * error: *present_out is false and the projection is empty. */
bool zcl_dev_observation_index_load(const char *index_path,
    struct zcl_dev_observation_leaf **leaves_out, size_t *count_out,
    bool *present_out, char *why, size_t why_len);

#endif
