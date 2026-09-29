/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Rebuild internals shared by the CAS scan, the per-issuer
 *          checkpoint replay and issuer isolation units. Not an API. */

#ifndef ZCL_VCS_PROOF_REPLAY_PRIV_H
#define ZCL_VCS_PROOF_REPLAY_PRIV_H

#include "proof_reuse_priv.h"

/* One signed checkpoint listed in the CAS. `parent` and `depth` index the
 * issuer's own checkpoint range while its ancestry is replayed. */
struct pts_cp {
    uint8_t wire[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
    uint8_t root[VCS_PROOF_ROOT_BYTES];
    struct vcs_proof_checkpoint_v1 decoded;
    size_t parent;
    size_t depth;
};

struct pts_cps {
    struct pts_cp *items;
    size_t count;
    size_t cap;
};

struct pts_counts {
    size_t tickets;
    size_t checkpoints;        /* replayed for issuers that were kept */
    size_t skipped;
    size_t isolated;           /* issuers whose history could not replay */
    size_t carried;            /* of those, kept as the live view had them */
};

/* What a rebuild checks beyond the catalog, and whether it may write. With
 * a trust policy it pins the signed fork evidence of that policy's
 * unrevoked verifiers; without one, and always for the issuer-log restore
 * (which publishes against the generation it scanned), it never writes. */
struct pts_scope {
    const struct vcs_proof_receiver_anchor *anchors;
    size_t anchor_count;
    const struct vcs_proof_reuse_policy *evidence_trust;
};

/* How one issuer's stored history replayed. A data failure isolates only
 * that issuer; running out of memory fails the whole rebuild. */
enum pts_verdict {
    PTS_KEEP = 0,
    PTS_ISOLATE,
    PTS_FATAL,
};

/* The retained ticket decodes and carries its issuer's valid signature. */
bool pts_entry_signed(const struct pr_entry *e);

/* Replay every issuer's listed checkpoints into `r` (already holding every
 * listed ticket), one issuer at a time. An issuer whose history is missing,
 * orphaned or unverifiable is isolated; `prior` is the live receiver the
 * view will replace. False only when the rebuild itself must refuse. */
bool pts_replay(const struct vcs_proof_receiver *prior,
                struct vcs_proof_receiver *r, struct pts_cps *cps,
                struct pts_counts *n, const struct pts_scope *scope);

/* Isolate the issuer of `items` (its whole listed checkpoint range) in `r`:
 * nothing it signed stays covered and it is marked history_incomplete.
 * When `prior` already holds that issuer and `may_carry`, its prior state
 * is kept instead (*carried). Either way, two of its listed checkpoints
 * that contradict each other or a checkpoint it kept mark it equivocating
 * and are retained as evidence. False only on allocation failure. */
bool pii_isolate(const struct vcs_proof_receiver *prior,
                 struct vcs_proof_receiver *r, struct pts_cp *items,
                 size_t count, bool may_carry, bool *carried);

/* Re-add every ticket `prior` held for the carried issuers `keys` (sorted),
 * with its coverage. False only on allocation failure. */
bool pii_carry_tickets(const struct vcs_proof_receiver *prior,
                       struct vcs_proof_receiver *r,
                       const uint8_t (*keys)[VCS_PROOF_PUBKEY_BYTES],
                       size_t count);

#endif
