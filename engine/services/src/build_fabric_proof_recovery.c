/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Replay exact staged proof wires after an interrupted CAS transfer. */

#include "services/build_fabric_proof_recovery.h"

#include "base/hex.h"
#include "models/build_fabric.h"
#include "vcs/blob_store.h"
#include "vcs/proof_reuse.h"

#include <string.h>

static bool bfpr_signed_pair(
    const struct db_build_worker_proof_pending *pending,
    const uint8_t pubkey[32], struct vcs_proof_ticket_v1 *ticket,
    struct vcs_proof_checkpoint_v1 *checkpoint)
{
    if (!vcs_proof_ticket_decode(pending->ticket_wire,
                                 sizeof(pending->ticket_wire), ticket) ||
        !vcs_proof_ticket_signature_valid(ticket) ||
        !vcs_proof_checkpoint_decode(pending->checkpoint_wire,
                                     sizeof(pending->checkpoint_wire),
                                     checkpoint) ||
        !vcs_proof_checkpoint_signature_valid(checkpoint) ||
        memcmp(ticket->producer_pubkey, pubkey, 32) != 0 ||
        memcmp(checkpoint->issuer_pubkey, pubkey, 32) != 0 ||
        ticket->issuer_seq == UINT64_MAX ||
        checkpoint->leaf_count != ticket->issuer_seq + 1u)
        return false;
    return true;
}

static bool bfpr_parent_matches(struct vcs_package_store *store,
                                const char *expected_head,
                                const uint8_t pubkey[32],
                                const struct vcs_proof_ticket_v1 *ticket,
                                const struct vcs_proof_checkpoint_v1 *cp)
{
    uint8_t parent[32] = {0};
    if (expected_head[0]) {
        uint8_t base_blob[32], base_wire[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
        struct vcs_proof_checkpoint_v1 base;
        if (!zcl_hex_decode_lower(expected_head, base_blob, 32) ||
            !vcs_proof_checkpoint_store_load(store, base_blob, pubkey,
                                              base_wire, parent) ||
            !vcs_proof_checkpoint_decode(base_wire, sizeof(base_wire),
                                          &base) ||
            ticket->issuer_seq != base.leaf_count)
            return false;
    } else if (ticket->issuer_seq != 0) {
        return false;
    }
    return memcmp(cp->prev_checkpoint_root, parent, sizeof(parent)) == 0;
}

enum bfpr_issuer_state {
    BFPR_ISSUER_UNAVAILABLE,
    BFPR_ISSUER_CHANGED,
    BFPR_ISSUER_MATCH,
};

static enum bfpr_issuer_state bfpr_issuer_matches(
    const uint8_t signer_seed[32],
    const struct db_build_worker_proof_pending *pending,
    uint8_t pubkey[32])
{
    struct vcs_proof_issuer_log *identity =
        vcs_proof_issuer_log_new(signer_seed);
    if (!identity) return BFPR_ISSUER_UNAVAILABLE;
    vcs_proof_issuer_log_pubkey(identity, pubkey);
    vcs_proof_issuer_log_free(identity);
    char signer_hex[65];
    zcl_hex_encode(pubkey, 32, signer_hex);
    return strcmp(signer_hex, pending->signer_pubkey) == 0
               ? BFPR_ISSUER_MATCH : BFPR_ISSUER_CHANGED;
}

enum bfpr_transfer_state {
    BFPR_TRANSFER_ROOT_INVALID,
    BFPR_TRANSFER_INCOMPLETE,
    BFPR_TRANSFER_COMPLETE,
};

static enum bfpr_transfer_state bfpr_transfer_complete(
    struct vcs_package_store *store,
    const struct db_build_worker_proof_pending *pending,
    uint8_t expected_head[32])
{
    uint8_t expected_ticket[32];
    if (!vcs_blob_root(pending->ticket_wire, sizeof(pending->ticket_wire),
                       expected_ticket) ||
        !vcs_blob_root(pending->checkpoint_wire,
                       sizeof(pending->checkpoint_wire), expected_head))
        return BFPR_TRANSFER_ROOT_INVALID;
    uint8_t present[VCS_PROOF_TICKET_WIRE_BYTES];
    size_t present_len = 0;
    if (vcs_blob_get_from(store, expected_ticket, present,
                          sizeof(present), &present_len) != VCS_BLOB_OK ||
        present_len != sizeof(pending->ticket_wire) ||
        memcmp(present, pending->ticket_wire, present_len) != 0 ||
        vcs_blob_get_from(store, expected_head, present,
                          sizeof(present), &present_len) != VCS_BLOB_OK ||
        present_len != sizeof(pending->checkpoint_wire) ||
        memcmp(present, pending->checkpoint_wire, present_len) != 0)
        return BFPR_TRANSFER_INCOMPLETE;
    return BFPR_TRANSFER_COMPLETE;
}

/* The pending row carries signed, exact wires. Resume only their immutable
 * CAS transfer; quota refusal leaves the published DB head unchanged. */
static bool bfpr_resume_transfer(
    struct vcs_package_store *store,
    const struct db_build_worker_proof_pending *pending,
    const uint8_t expected_head[32])
{
    uint8_t expected_ticket[32], stored_ticket[32], stored_head[32];
    if (!vcs_blob_root(pending->ticket_wire, sizeof(pending->ticket_wire),
                       expected_ticket) ||
        !vcs_proof_ticket_store_put_no_evict(
            store, pending->ticket_wire, sizeof(pending->ticket_wire),
            stored_ticket) ||
        memcmp(stored_ticket, expected_ticket, 32) != 0 ||
        !vcs_proof_ticket_store_put_no_evict(
            store, pending->checkpoint_wire, sizeof(pending->checkpoint_wire),
            stored_head) ||
        memcmp(stored_head, expected_head, 32) != 0)
        return false;
    return true;
}

static enum bfpr_transfer_state bfpr_transfer_ready(
    struct vcs_package_store *store,
    const struct db_build_worker_proof_pending *pending,
    uint8_t expected_head[32])
{
    enum bfpr_transfer_state state =
        bfpr_transfer_complete(store, pending, expected_head);
    if (state != BFPR_TRANSFER_INCOMPLETE) return state;
    if (!bfpr_resume_transfer(store, pending, expected_head))
        return BFPR_TRANSFER_INCOMPLETE;
    return bfpr_transfer_complete(store, pending, expected_head);
}

static bool bfpr_replay_selects_staged(
    const struct vcs_proof_issuer_log *restored,
    const struct db_build_worker_proof_pending *pending,
    const struct vcs_proof_ticket_v1 *ticket,
    const struct vcs_proof_checkpoint_v1 *checkpoint)
{
    const uint8_t *selected = vcs_proof_issuer_log_ticket(
        restored, ticket->issuer_seq);
    return vcs_proof_issuer_log_count(restored) == checkpoint->leaf_count &&
           selected && memcmp(selected, pending->ticket_wire,
                              sizeof(pending->ticket_wire)) == 0;
}

static void bfpr_clear_outputs(bool *had_pending, char next_head_hex[65])
{
    if (had_pending) *had_pending = false;
    if (next_head_hex) next_head_hex[0] = '\0';
}

static bool bfpr_inputs_valid(struct node_db *ndb, const char *worker_id,
                              const uint8_t signer_seed[32],
                              bool *had_pending, char next_head_hex[65])
{
    return ndb && worker_id && signer_seed && had_pending && next_head_hex;
}

struct zcl_result build_fabric_proof_pending_replay(
    struct node_db *ndb, struct vcs_package_store *store,
    const char *worker_id, const uint8_t signer_seed[32],
    size_t max_catalog_rows, size_t max_tickets,
    bool *had_pending, char next_head_hex[65])
{
    bfpr_clear_outputs(had_pending, next_head_hex);
    if (!bfpr_inputs_valid(ndb, worker_id, signer_seed,
                           had_pending, next_head_hex))
        return ZCL_ERR(-1, "proof-pending-replay-invalid-input");
    struct db_build_worker_proof_pending pending;
    int found = db_build_worker_proof_pending_find_checked(
        ndb, worker_id, &pending);
    if (found < 0)
        return ZCL_ERR(-1, "proof-pending-replay-missing-or-corrupt-row");
    if (found == 0) return ZCL_OK;
    if (!store || !max_catalog_rows || !max_tickets)
        return ZCL_ERR(-1, "proof-pending-replay-store-or-budget-missing");

    uint8_t pubkey[32];
    enum bfpr_issuer_state issuer =
        bfpr_issuer_matches(signer_seed, &pending, pubkey);
    if (issuer == BFPR_ISSUER_UNAVAILABLE)
        return ZCL_ERR(-1, "proof-pending-replay-issuer-key-unavailable");
    if (issuer != BFPR_ISSUER_MATCH)
        return ZCL_ERR(-1, "proof-pending-replay-signer-changed");

    struct vcs_proof_ticket_v1 ticket;
    struct vcs_proof_checkpoint_v1 cp;
    if (!bfpr_signed_pair(&pending, pubkey, &ticket, &cp) ||
        !bfpr_parent_matches(store, pending.expected_head, pubkey,
                             &ticket, &cp))
        return ZCL_ERR(-1, "proof-pending-replay-signed-chain-invalid");

    uint8_t expected_head[32];
    enum bfpr_transfer_state transfer =
        bfpr_transfer_ready(store, &pending, expected_head);
    if (transfer == BFPR_TRANSFER_ROOT_INVALID)
        return ZCL_ERR(-1, "proof-pending-replay-root-invalid");
    if (transfer != BFPR_TRANSFER_COMPLETE)
        return ZCL_ERR(-1, "proof-pending-replay-transfer-incomplete");

    struct vcs_proof_issuer_log *restored =
        vcs_proof_issuer_log_restore_from_store(
            signer_seed, store, expected_head,
            max_catalog_rows, max_tickets);
    if (!restored)
        return ZCL_ERR(-1, "proof-pending-replay-incomplete-history");
    bool exact = bfpr_replay_selects_staged(restored, &pending, &ticket, &cp);
    vcs_proof_issuer_log_free(restored);
    if (!exact)
        return ZCL_ERR(-1, "proof-pending-replay-ticket-not-selected");
    zcl_hex_encode(expected_head, sizeof(expected_head), next_head_hex);
    *had_pending = true;
    return ZCL_OK;
}
