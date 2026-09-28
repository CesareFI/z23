/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Validate and replay an interrupted local proof-head publication. */

#ifndef ZCL_SERVICES_BUILD_FABRIC_PROOF_RECOVERY_H
#define ZCL_SERVICES_BUILD_FABRIC_PROOF_RECOVERY_H

#include "base/result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct node_db;
struct vcs_package_store;

/* Re-read the exact staged ticket/checkpoint wires, verify the local signer,
 * sequence and checkpoint parent against the still-current worker head, then
 * require both exact wires already present in CAS and reconstruct the complete
 * issuer log at the proposed next head. An incomplete transfer stays pending
 * until the existing transfer path resumes it. On refusal,
 * outputs clear and the worker's durable head/pending bytes are untouched.
 * This does NOT finalize the database head, authorize reuse, or pin objects.
 * The caller must fence the local issuer writer and store mutation across
 * this call and any later conditional head publication. */
struct zcl_result build_fabric_proof_pending_replay(
    struct node_db *ndb, struct vcs_package_store *store,
    const char *worker_id, const uint8_t signer_seed[32],
    size_t max_catalog_rows, size_t max_tickets,
    bool *had_pending, char next_head_hex[65]);

#endif /* ZCL_SERVICES_BUILD_FABRIC_PROOF_RECOVERY_H */
