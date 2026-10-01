/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Derive, sign, and verify the per-pair mandatory coverage
 *          manifest that binds executed test groups to eligible signed
 *          observations.
 *
 * The canonical lifecycle (docs/work/CANONICAL_LIFECYCLE.md, PROOF_SET)
 * separates reusable input keys from immutable observation roots: coverage
 * binds the mandatory input keys derived for a candidate and the receiver's
 * eligibility basis. The proof worker already emits one signed verdict-leaf
 * observation per executed group into a durable per-pair CAS
 * (<state>/observations.<key>, see dev_proof_observation.h). What was missing
 * is the object that proves WHICH groups a pair's receipt actually executed
 * and that every one of them is backed by eligible PASS evidence with no
 * preserved contradiction. That is this manifest.
 *
 * The worker derives and signs it after the receipt stores; the pre-push
 * hook re-derives and re-verifies it on every push. Both sides run the same
 * bounded verification over the same private CAS, so a producer cannot omit
 * a known eligible contradiction and a corrupted or replayed manifest is
 * named, never silently re-derived.
 *
 * Wire shapes are versioned apart from layout, exactly like the pair
 * receipt: the manifest is admitted only under the policy version stamped
 * into it, and an older or newer producer is named instead of misread. */

#ifndef ZCL_DEV_PROOF_COVERAGE_H
#define ZCL_DEV_PROOF_COVERAGE_H

#include "dev_proof_receipt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Manifest file on disk: the fixed envelope wire followed by blob_len
 * canonical blob bytes. One file per pair, beside the receipt sidecars. */
#define ZCL_DEV_COVERAGE_MAGIC "Z23COV1"
#define ZCL_DEV_COVERAGE_MAGIC_BYTES 8u
#define ZCL_DEV_COVERAGE_VERSION 1u
#define ZCL_DEV_COVERAGE_UNSIGNED_WIRE_BYTES 232u
#define ZCL_DEV_COVERAGE_WIRE_BYTES \
    (ZCL_DEV_COVERAGE_UNSIGNED_WIRE_BYTES + ZCL_DEV_PROOF_SIGNER_TRAILER_BYTES)
/* Canonical blob ceiling: bounded so a hook push check stays milliseconds. */
#define ZCL_DEV_COVERAGE_BLOB_MAX_BYTES (1u << 20)
/* Hard ceilings, aligned with the observation store and lookup caps. */
#define ZCL_DEV_COVERAGE_MAX_ROWS 8192u
#define ZCL_DEV_COVERAGE_MAX_ROW_ROOTS 256u

/* Refusal tokens. These are the exact strings written into a `why` buffer
 * and printed by the pre-push hook, so a reader can act on the line without
 * reading this file. */
#define ZCL_DEV_COVERAGE_WHY_ARGUMENTS "coverage_arguments_invalid"
#define ZCL_DEV_COVERAGE_WHY_LOG_UNAVAILABLE "coverage_log_unavailable"
#define ZCL_DEV_COVERAGE_WHY_LOG_INVALID "coverage_log_invalid"
#define ZCL_DEV_COVERAGE_WHY_LOG_DUPLICATE "coverage_log_duplicate"
#define ZCL_DEV_COVERAGE_WHY_OBSERVATION_REFUSED "coverage_observation_refused"
#define ZCL_DEV_COVERAGE_WHY_OBSERVATION_FAIL "coverage_observation_fail"
#define ZCL_DEV_COVERAGE_WHY_COUNT_MISMATCH "coverage_count_mismatch"
#define ZCL_DEV_COVERAGE_WHY_STORE_INCOMPLETE "coverage_store_incomplete"
#define ZCL_DEV_COVERAGE_WHY_STORE_TOO_MANY "coverage_store_too_many_roots"
#define ZCL_DEV_COVERAGE_WHY_EMITTED_MISSING "coverage_emitted_root_missing"
#define ZCL_DEV_COVERAGE_WHY_MISSING "coverage_missing"
#define ZCL_DEV_COVERAGE_WHY_MANIFEST_INVALID "coverage_manifest_invalid"
#define ZCL_DEV_COVERAGE_WHY_MANIFEST_UNSIGNED "coverage_manifest_unsigned"
#define ZCL_DEV_COVERAGE_WHY_MANIFEST_TOO_LARGE "coverage_manifest_too_large"
#define ZCL_DEV_COVERAGE_WHY_BLOB_INVALID "coverage_blob_invalid"
#define ZCL_DEV_COVERAGE_WHY_BINDING "coverage_binding_mismatch"
#define ZCL_DEV_COVERAGE_WHY_CONFLICT "proof_observation_conflict"

/* The receipt identity a manifest is bound to. A manifest is valid for
 * exactly one pair receipt: same commits, same child-set root, same impact
 * policy, same admission policy version. */
struct zcl_dev_coverage_binding {
    uint8_t local_commit[ZCL_DEV_PROOF_OID_MAX];
    uint8_t local_commit_len;
    uint8_t remote_base[ZCL_DEV_PROOF_OID_MAX];
    uint8_t remote_base_len;
    uint8_t child_set_root[ZCL_DEV_PROOF_ROOT_BYTES];
    uint8_t impact_policy_root[ZCL_DEV_PROOF_ROOT_BYTES];
    uint32_t policy_version;
};

/* Parse the test-dimension child log for canonical observation lines.
 * Every executed group emits exactly one
 *   OBSERVATION group=<name> verdict=PASS key=<hex> root=<hex> source=independent_execution
 * line; an executed group whose observation could not be recorded emits an
 * OBSERVATION REFUSE line and refuses derivation. `expected` is the
 * receipt's test-dimension ran count; when it is zero, `log_path` is not
 * opened and zero rows parse. Rows are returned in first-seen log order;
 * the canonical blob sorts them. */
struct zcl_dev_coverage_log_row {
    char group[ZCL_DEV_VERDICT_LEAF_GROUP_BYTES];
    uint8_t group_len;
    uint8_t key[ZCL_DEV_VERDICT_LEAF_KEY_BYTES];
    uint8_t root[ZCL_DEV_PROOF_ROOT_BYTES];
};

bool zcl_dev_coverage_log_rows(const char *log_path, uint32_t expected,
    struct zcl_dev_coverage_log_row *rows, uint32_t rows_cap,
    uint32_t *out_count, char *why, size_t why_len);

/* Producer side: derive the mandatory set from the executed-group log
 * rows, classify it against the observation CAS, and sign the manifest.
 * Fills `envelope` (ZCL_DEV_COVERAGE_WIRE_BYTES) and allocates the
 * canonical blob (`*blob_out`, caller frees with free()). The store must
 * contain an eligible PASS observation for every mandatory (key, group)
 * including each row's emitted root, and no eligible contradiction; every
 * refusal is named. */
bool zcl_dev_coverage_manifest_derive(const char *store_root,
    const char *test_log_path,
    const struct zcl_dev_coverage_binding *binding, uint32_t expected_rows,
    uint8_t envelope[ZCL_DEV_COVERAGE_WIRE_BYTES],
    uint8_t **blob_out, size_t *blob_len_out,
    char *why, size_t why_len);

/* Read only the blob length from a stored envelope wire. The pre-push
 * hook uses this to size its bounded second read of the manifest file;
 * full parse and verification happen in _manifest_verify. */
bool zcl_dev_coverage_envelope_blob_len(const uint8_t *envelope_wire,
                                        size_t envelope_len,
                                        uint32_t *blob_len_out);

/* Receiver side: verify a stored manifest (the fixed envelope wire and its
 * canonical blob) against the binding and a complete local enumeration of
 * the same observation CAS. Refuses by name; never aborts. */
bool zcl_dev_coverage_manifest_verify(const char *store_root,
    const uint8_t *envelope_wire, size_t envelope_len,
    const uint8_t *blob, size_t blob_len,
    const struct zcl_dev_coverage_binding *binding,
    char *why, size_t why_len);

#endif
