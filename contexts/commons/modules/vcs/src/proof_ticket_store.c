/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: CAS placement of proof tickets, checkpoints and key preimages,
 *          and the receiver rebuilt as a projection over those blobs. */

#include "proof_reuse_priv.h"

#include "vcs/blob_store.h"
#include "vcs/package_store.h"

#include "base/log_macros.h"
#include "base/safe_alloc.h"

#include <stdlib.h>
#include <string.h>

#define PTS_LOG "vcs.proof_ticket_store"
#define PTS_BLOB_MAX VCS_CPK_WIRE_BYTES

bool vcs_proof_ticket_store_put(struct vcs_package_store *store,
                                const uint8_t *wire, size_t len,
                                uint8_t blob_root[VCS_PROOF_ROOT_BYTES])
{
    if (!store || !wire || !blob_root)
        LOG_RETURN(false, PTS_LOG, "store put: null argument");
    enum vcs_blob_result r = vcs_blob_put_to(store, wire, len, blob_root);
    if (r != VCS_BLOB_OK)
        LOG_RETURN(false, PTS_LOG, "store put (%zu bytes): %s", len,
                   vcs_blob_result_string(r));
    return true;
}

bool vcs_component_proof_key_load(struct vcs_package_store *store,
                                  const uint8_t preimage_root[32],
                                  struct vcs_component_proof_key_v1 *out)
{
    if (!store || !preimage_root || !out)
        LOG_RETURN(false, PTS_LOG, "preimage load: null argument");
    uint8_t wire[VCS_CPK_WIRE_BYTES + 1u];
    size_t len = 0;
    enum vcs_blob_result r =
        vcs_blob_get_from(store, preimage_root, wire, sizeof(wire), &len);
    if (r != VCS_BLOB_OK)
        LOG_RETURN(false, PTS_LOG, "preimage load: %s",
                   vcs_blob_result_string(r));
    if (!vcs_component_proof_key_decode(wire, len, out))
        LOG_RETURN(false, PTS_LOG, "preimage load: blob is not a key preimage");
    uint8_t again[VCS_PROOF_ROOT_BYTES];
    if (!vcs_component_proof_key_preimage_root(out, again) ||
        memcmp(again, preimage_root, VCS_PROOF_ROOT_BYTES) != 0)
        LOG_RETURN(false, PTS_LOG, "preimage load: root does not re-derive");
    return true;
}

/* ── Rebuild ────────────────────────────────────────────────────────── */

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
    size_t checkpoints;
    size_t skipped;
};

struct pts_roots {
    uint8_t (*items)[32];
    size_t count;
    size_t cap;
};

static bool pts_root_add(struct pts_roots *roots, const uint8_t root[32])
{
    if (roots->count == roots->cap) {
        size_t cap = roots->cap ? roots->cap * 2u : 32u;
        if (cap < roots->cap || cap > SIZE_MAX / sizeof(*roots->items))
            LOG_RETURN(false, PTS_LOG, "rebuild: proof root capacity overflow");
        uint8_t (*grown)[32] = zcl_realloc(
            roots->items, cap * sizeof(*roots->items), "proof_roots");
        if (!grown)
            LOG_RETURN(false, PTS_LOG, "rebuild: proof root allocation failed");
        roots->items = grown;
        roots->cap = cap;
    }
    memcpy(roots->items[roots->count++], root, 32);
    return true;
}

static bool pts_take_cp(struct pts_cps *cps, const uint8_t *blob, size_t len,
                        struct pts_counts *n, bool *retained)
{
    struct vcs_proof_checkpoint_v1 decoded;
    if (!vcs_proof_checkpoint_decode(blob, len, &decoded) ||
        !vcs_proof_checkpoint_signature_valid(&decoded)) {
        n->skipped++;
        return true;
    }
    if (cps->count == cps->cap) {
        size_t cap = cps->cap ? cps->cap * 2u : 32u;
        if (cap < cps->cap || cap > SIZE_MAX / sizeof(*cps->items))
            LOG_RETURN(false, PTS_LOG, "rebuild: checkpoint capacity overflow");
        struct pts_cp *grown = zcl_realloc(
            cps->items, cap * sizeof(*grown), "proof_cps");
        if (!grown)
            LOG_RETURN(false, PTS_LOG, "rebuild: checkpoint allocation failed");
        cps->items = grown;
        cps->cap = cap;
    }
    struct pts_cp *item = &cps->items[cps->count++];
    item->decoded = decoded;
    memcpy(item->wire, blob, len);
    if (!vcs_proof_checkpoint_root(blob, len, item->root))
        LOG_RETURN(false, PTS_LOG, "rebuild: checkpoint root failed");
    *retained = true;
    return true;
}

static bool pts_take(struct vcs_proof_receiver *r, struct pts_cps *cps,
                     const uint8_t *blob, size_t len, struct pts_counts *n,
                     bool *retained)
{
    *retained = false;
    bool added = false;
    if (len == VCS_PROOF_TICKET_WIRE_BYTES &&
        memcmp(blob, "Z23PTK1\0", 8) == 0 &&
        vcs_proof_ticket_decode(blob, len, &(struct vcs_proof_ticket_v1){0})) {
        if (!vcs_proof_receiver_add_ticket(r, blob, len, &added))
            return false;
        *retained = true;
        if (added) n->tickets++;
        return true;
    }
    if (len == VCS_PROOF_CHECKPOINT_WIRE_BYTES &&
        memcmp(blob, "Z23PCK1\0", 8) == 0)
        return pts_take_cp(cps, blob, len, n, retained);
    n->skipped++;
    return true;
}

static bool pts_valid_ticket(const struct pr_entry *e)
{
    struct vcs_proof_ticket_v1 t;
    return vcs_proof_ticket_decode(e->wire, sizeof(e->wire), &t) &&
           vcs_proof_ticket_signature_valid(&t);
}

/* Select the same signature-valid ticket at each sequence regardless of CAS
 * arrival order. Ticket ingest already records signed sequence forks. */
static bool pts_delta(const struct vcs_proof_receiver *r,
                      const uint8_t issuer[32], uint64_t from, uint64_t to,
                      const uint8_t **wires, size_t *lens)
{
    for (uint64_t seq = from; seq < to; seq++) {
        const struct pr_entry *chosen = NULL;
        for (size_t one = pr_entry_seq_first(r, issuer, seq); one;
             one = r->entries[one - 1u].next_seq) {
            const struct pr_entry *e = &r->entries[one - 1u];
            if (!pts_valid_ticket(e)) continue;
            if (!chosen || memcmp(e->observation_root,
                                  chosen->observation_root, 32) < 0)
                chosen = e;
        }
        if (!chosen) return false;
        wires[seq - from] = chosen->wire;
        lens[seq - from] = VCS_PROOF_TICKET_WIRE_BYTES;
    }
    return true;
}

static bool pts_replay_delta(struct vcs_proof_receiver *r,
                             const struct pts_cp *cp, uint64_t from,
                             size_t count, const uint8_t **wires,
                             size_t *lens)
{
    const struct vcs_proof_checkpoint_v1 *c = &cp->decoded;
    if (!pts_delta(r, c->issuer_pubkey, from, from + count, wires, lens))
        return false;
    struct vcs_proof_sync_report rep;
    if (!vcs_proof_receiver_sync(r, cp->wire,
                                 VCS_PROOF_CHECKPOINT_WIRE_BYTES,
                                 (const uint8_t *const *)wires, lens,
                                 count, &rep))
        return false;
    bool accepted = rep.outcome == VCS_PROOF_SYNC_ADVANCED ||
                    rep.outcome == VCS_PROOF_SYNC_CURRENT ||
                    (rep.outcome == VCS_PROOF_SYNC_EQUIVOCATION &&
                     rep.reason && strcmp(rep.reason,
                                          VCS_PROOF_SYNC_WHY_EQUIVOCATION) == 0);
    if (!accepted) return false;
    const struct pr_issuer *issuer = pr_issuer_find(r, c->issuer_pubkey);
    if (!issuer) return false;
    for (size_t i = 0; i < issuer->cp_count; i++)
        if (memcmp(issuer->cps[i].root, cp->root, 32) == 0)
            return true;
    return false;
}

static bool pts_replay_one(struct vcs_proof_receiver *r,
                           const struct pts_cps *cps, size_t k,
                           struct pts_counts *n)
{
    const struct vcs_proof_checkpoint_v1 *c = &cps->items[k].decoded;
    if (!vcs_proof_checkpoint_signature_valid(c)) {
        n->skipped++;
        return true;
    }
    uint64_t from = vcs_proof_receiver_issuer_leaves(r, c->issuer_pubkey);
    if (c->leaf_count > from && c->leaf_count - from > SIZE_MAX)
        LOG_RETURN(false, PTS_LOG, "rebuild: checkpoint delta exceeds size_t");
    size_t count = c->leaf_count > from ? (size_t)(c->leaf_count - from) : 0;
    const uint8_t **wires = count ? zcl_calloc(count, sizeof(*wires),
                                               "proof_rebuild_delta") : NULL;
    size_t *lens = count ? zcl_calloc(count, sizeof(*lens),
                                      "proof_rebuild_lens") : NULL;
    bool ok = (count == 0 || (wires && lens)) &&
              pts_replay_delta(r, &cps->items[k], from, count, wires, lens);
    free(wires);
    free(lens);
    if (!ok)
        LOG_RETURN(false, PTS_LOG,
                   "rebuild: signed checkpoint cannot be reproduced at %llu leaves",
                   (unsigned long long)c->leaf_count);
    n->checkpoints++;
    return true;
}

static int pts_root_compare(const void *a, const void *b)
{
    const struct pts_cp *x = a, *y = b;
    return memcmp(x->root, y->root, VCS_PROOF_ROOT_BYTES);
}

static size_t pts_parent(const struct pts_cps *cps,
                         const uint8_t root[VCS_PROOF_ROOT_BYTES])
{
    size_t lo = 0, hi = cps->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        int cmp = memcmp(cps->items[mid].root, root, VCS_PROOF_ROOT_BYTES);
        if (cmp < 0) lo = mid + 1u;
        else hi = mid;
    }
    if (lo == cps->count ||
        memcmp(cps->items[lo].root, root, VCS_PROOF_ROOT_BYTES) != 0)
        return SIZE_MAX;
    return lo;
}

static bool pts_ancestry_depth(struct pts_cps *cps)
{
    if (!cps->count) return true;
    uint8_t *state = zcl_calloc(cps->count, 1u, "proof_cp_state");
    size_t *stack = zcl_calloc(cps->count, sizeof(*stack), "proof_cp_stack");
    if (!state || !stack) {
        free(state);
        free(stack);
        LOG_RETURN(false, PTS_LOG, "rebuild: ancestry allocation failed");
    }
    bool ok = true;
    for (size_t k = 0; k < cps->count && ok; k++) {
        size_t cur = k, used = 0;
        while (cur != SIZE_MAX && state[cur] == 0) {
            state[cur] = 1;
            stack[used++] = cur;
            cur = cps->items[cur].parent;
        }
        if (cur != SIZE_MAX && state[cur] == 1) {
            LOG_ERROR(PTS_LOG, "rebuild: checkpoint ancestry cycle");
            ok = false;
            break;
        }
        size_t depth = cur == SIZE_MAX ? 0u : cps->items[cur].depth + 1u;
        while (used) {
            size_t one = stack[--used];
            cps->items[one].depth = depth++;
            state[one] = 2;
        }
    }
    free(stack);
    free(state);
    return ok;
}

static bool pts_index_ancestry(struct pts_cps *cps)
{
    if (cps->count > 1u)
        qsort(cps->items, cps->count, sizeof(*cps->items), pts_root_compare);
    for (size_t k = 0; k < cps->count; k++) {
        struct pts_cp *cp = &cps->items[k];
        const uint8_t *prev = cp->decoded.prev_checkpoint_root;
        if (memcmp(prev, (const uint8_t[32]){0}, 32) == 0) {
            cp->parent = SIZE_MAX;
            continue;
        }
        cp->parent = pts_parent(cps, prev);
        if (cp->parent == SIZE_MAX)
            LOG_RETURN(false, PTS_LOG, "rebuild: missing checkpoint ancestor");
        const struct pts_cp *parent = &cps->items[cp->parent];
        if (memcmp(parent->decoded.issuer_pubkey, cp->decoded.issuer_pubkey, 32) != 0 ||
            parent->decoded.leaf_count > cp->decoded.leaf_count)
            LOG_RETURN(false, PTS_LOG, "rebuild: invalid checkpoint ancestor");
    }
    return pts_ancestry_depth(cps);
}

static int pts_cp_compare(const void *a, const void *b)
{
    const struct pts_cp *x = a, *y = b;
    if (x->decoded.leaf_count < y->decoded.leaf_count) return -1;
    if (x->decoded.leaf_count > y->decoded.leaf_count) return 1;
    if (x->depth < y->depth) return -1;
    if (x->depth > y->depth) return 1;
    return memcmp(x->wire, y->wire, sizeof(x->wire));
}

/* Leaf count and signed ancestry together determine replay order. */
static bool pts_replay(struct vcs_proof_receiver *r, struct pts_cps *cps,
                       struct pts_counts *n)
{
    if (!pts_index_ancestry(cps)) return false;
    if (cps->count > 1u)
        qsort(cps->items, cps->count, sizeof(*cps->items), pts_cp_compare);
    bool ok = true;
    for (size_t k = 0; k < cps->count && ok; k++)
        ok = pts_replay_one(r, cps, k, n);
    return ok;
}

static bool pts_scan_one(struct vcs_proof_receiver *r,
                         struct vcs_package_store *store,
                         struct pts_cps *cps, struct pts_counts *n,
                         struct pts_roots *roots, const uint8_t root[32])
{
    uint8_t blob[PTS_BLOB_MAX + 1u];
    size_t len = 0;
    enum vcs_blob_result got = vcs_blob_get_from(
        store, root, blob, sizeof(blob), &len);
    if (got == VCS_BLOB_OK) {
        bool retained = false;
        if (!pts_take(r, cps, blob, len, n, &retained)) return false;
        return !retained || pts_root_add(roots, root);
    }
    if (got == VCS_BLOB_ERR_SHAPE || got == VCS_BLOB_ERR_CAPACITY) {
        n->skipped++;
        return true;
    }
    LOG_RETURN(false, PTS_LOG, "rebuild: listed CAS blob cannot be read: %s",
               vcs_blob_result_string(got));
}

static bool pts_scan(struct vcs_proof_receiver *r,
                     struct vcs_package_store *store, struct pts_cps *cps,
                     struct pts_counts *n, struct pts_roots *roots,
                     uint64_t *generation)
{
    struct vcs_package_store_summary *rows =
        zcl_calloc(VCS_PACKAGE_STORE_PAGE_MAX, sizeof(*rows),
                   "proof_rebuild_rows");
    if (!rows) LOG_RETURN(false, PTS_LOG, "rebuild: out of memory");
    uint8_t cursor[32];
    bool resume = false;
    bool done = false;
    bool ok = true;
    while (ok && !done) {
        struct vcs_package_store_page page;
        enum vcs_package_store_page_result result =
            vcs_package_store_page_summaries(store, resume ? cursor : NULL,
                                             VCS_PACKAGE_STORE_PAGE_MAX,
                                             resume ? *generation : 0,
                                             rows, &page);
        if (result != VCS_PACKAGE_STORE_PAGE_OK) {
            LOG_ERROR(PTS_LOG, "rebuild: package catalog page refused (%d)",
                      (int)result);
            ok = false;
            break;
        }
        *generation = page.generation;
        for (size_t i = 0; ok && i < page.count; i++)
            ok = pts_scan_one(r, store, cps, n, roots, rows[i].root);
        if (page.has_more && page.count == 0) ok = false;
        memcpy(cursor, page.next_root, sizeof(cursor));
        resume = true;
        done = !page.has_more;
    }
    free(rows);
    return ok;
}

static bool pts_recheck(struct vcs_package_store *store,
                        const struct pts_roots *roots)
{
    for (size_t i = 0; i < roots->count; i++) {
        uint8_t wire[PTS_BLOB_MAX + 1u];
        size_t len = 0;
        enum vcs_blob_result got = vcs_blob_get_from(
            store, roots->items[i], wire, sizeof(wire), &len);
        if (got != VCS_BLOB_OK)
            LOG_RETURN(false, PTS_LOG,
                       "rebuild: retained proof blob changed before publish: %s",
                       vcs_blob_result_string(got));
    }
    return true;
}

#ifdef ZCL_TESTING
static void (*pts_before_recheck_hook)(void *);
static void *pts_before_recheck_context;
void vcs_proof_receiver_test_before_recheck(void (*hook)(void *), void *context)
{
    pts_before_recheck_hook = hook;
    pts_before_recheck_context = context;
}
#endif

struct pts_publish_context {
    struct vcs_proof_receiver *live;
    struct vcs_proof_receiver *staged;
};

static void pts_publish(void *context)
{
    struct pts_publish_context *p = context;
    struct vcs_proof_receiver old = *p->live;
    *p->live = *p->staged;
    *p->staged = old;
}

static bool pts_publish_rechecked(struct vcs_proof_receiver *live,
                                  struct vcs_proof_receiver *staging,
                                  struct vcs_package_store *store,
                                  const struct pts_roots *roots,
                                  uint64_t generation)
{
#ifdef ZCL_TESTING
    if (pts_before_recheck_hook)
        pts_before_recheck_hook(pts_before_recheck_context);
#endif
    if (!pts_recheck(store, roots)) return false;
    struct pts_publish_context p = {live, staging};
    return vcs_package_store_publish_if_generation(
        store, generation, pts_publish, &p) == VCS_PACKAGE_STORE_PAGE_OK;
}

static bool pts_empty_issuer(const struct pr_issuer *was)
{
    return was->cp_count == 0 && was->verified_count == 0 &&
           was->mmr.num_leaves == 0 && !was->equivocating;
}

static bool pts_preserves_issuer(const struct pr_issuer *was,
                                 const struct vcs_proof_receiver *next)
{
    const struct pr_issuer *now = pr_issuer_find(next, was->pubkey);
    if (!now && pts_empty_issuer(was))
        return true; /* a refused sync created no prior evidence */
    if (!now || now->mmr.num_leaves < was->mmr.num_leaves)
        LOG_RETURN(false, PTS_LOG, "rebuild: issuer high-water regressed");
    if (was->equivocating && !now->equivocating)
        LOG_RETURN(false, PTS_LOG, "rebuild: issuer fork disappeared");
    for (size_t j = 0; j < was->cp_count; j++) {
        bool found = false;
        bool verified = false;
        for (size_t k = 0; k < now->cp_count; k++)
            if (memcmp(was->cps[j].root, now->cps[k].root, 32) == 0) {
                found = true;
                verified = now->cps[k].verified;
                break;
            }
        if (!found)
            LOG_RETURN(false, PTS_LOG, "rebuild: prior checkpoint disappeared");
        if (was->cps[j].verified && !verified)
            LOG_RETURN(false, PTS_LOG,
                       "rebuild: prior checkpoint lost verification");
    }
    return true;
}

/* A live receiver is also an anchor: a rebuilt view may extend it but may
 * not silently forget a ticket, checkpoint, or signed fork it already saw. */
static bool pts_preserves_prior(const struct vcs_proof_receiver *old,
                                const struct vcs_proof_receiver *next)
{
    for (size_t i = 0; i < old->count; i++) {
        const struct pr_entry *entry =
            pr_entry_find(next, old->entries[i].observation_root);
        if (!entry)
            LOG_RETURN(false, PTS_LOG, "rebuild: prior ticket disappeared");
        if (old->entries[i].covered && !entry->covered)
            LOG_RETURN(false, PTS_LOG, "rebuild: prior ticket lost coverage");
    }
    for (size_t i = 0; i < old->issuer_count; i++)
        if (!pts_preserves_issuer(&old->issuers[i], next)) return false;
    return true;
}

/* A live receiver may have verified a checkpoint before it learned that the
 * same issuer signed a second ticket at one sequence. Replaying a complete
 * CAS from an empty receiver sees the fork first and conservatively marks
 * that earlier checkpoint unverified. Carry the old verification forward
 * only when the replay retained its exact signed checkpoint and every
 * previously covered ticket. The issuer stays equivocating, so this cannot
 * turn conflicting evidence into a reuse hit. */
static struct pr_issuer *pts_find_issuer(struct vcs_proof_receiver *receiver,
                                         const uint8_t pubkey[32])
{
    for (size_t i = 0; i < receiver->issuer_count; i++)
        if (memcmp(receiver->issuers[i].pubkey, pubkey, 32) == 0)
            return &receiver->issuers[i];
    return NULL;
}

static bool pts_anchor_checkpoints(const struct pr_issuer *was,
                                    struct pr_issuer *now, bool *lost)
{
    *lost = false;
    for (size_t j = 0; j < was->cp_count; j++) {
        const struct pr_checkpoint *prior = &was->cps[j];
        if (!prior->verified) continue;
        struct pr_checkpoint *found = NULL;
        for (size_t k = 0; k < now->cp_count; k++)
            if (memcmp(now->cps[k].root, prior->root, 32) == 0) {
                found = &now->cps[k];
                break;
            }
        if (!found || memcmp(found->wire, prior->wire,
                             sizeof(prior->wire)) != 0)
            LOG_RETURN(false, PTS_LOG,
                       "rebuild: anchored checkpoint absent from CAS");
        if (!found->verified) *lost = true;
    }
    return true;
}

static bool pts_anchor_tickets(const struct vcs_proof_receiver *old,
                                const struct pr_issuer *was,
                                struct vcs_proof_receiver *next)
{
    for (size_t j = 0; j < old->count; j++) {
        const struct pr_entry *prior = &old->entries[j];
        if (!prior->covered ||
            memcmp(prior->producer, was->pubkey, 32) != 0)
            continue;
        struct pr_entry *found =
            pr_entry_find(next, prior->observation_root);
        if (!found || memcmp(found->wire, prior->wire,
                             sizeof(prior->wire)) != 0)
            LOG_RETURN(false, PTS_LOG,
                       "rebuild: anchored ticket absent from CAS");
        found->covered = true;
    }
    return true;
}

static void pts_anchor_verification(const struct pr_issuer *was,
                                     struct pr_issuer *now)
{
    for (size_t j = 0; j < was->cp_count; j++) {
        const struct pr_checkpoint *prior = &was->cps[j];
        if (!prior->verified) continue;
        for (size_t k = 0; k < now->cp_count; k++)
            if (memcmp(now->cps[k].root, prior->root, 32) == 0) {
                now->cps[k].verified = true;
                break;
            }
    }
    now->mmr = was->mmr;
    memcpy(now->last_root, was->last_root, sizeof(now->last_root));
    now->verified_count = was->verified_count;
}

static bool pts_restore_anchored_fork(const struct vcs_proof_receiver *old,
                                      struct vcs_proof_receiver *next)
{
    for (size_t i = 0; i < old->issuer_count; i++) {
        const struct pr_issuer *was = &old->issuers[i];
        if (!was->verified_count) continue;
        struct pr_issuer *now = pts_find_issuer(next, was->pubkey);
        /* Only a newly observed ticket fork may use this anchor. A known or
         * newly listed checkpoint fork keeps the existing explicit refusal:
         * choosing a branch after replay would change verified ancestry. */
        if (!now || !now->equivocating ||
            was->cp_count != was->verified_count ||
            now->cp_count != was->cp_count)
            continue;
        bool lost = false;
        if (!pts_anchor_checkpoints(was, now, &lost)) return false;
        if (!lost) continue;
        if (!pts_anchor_tickets(old, was, next)) return false;
        pts_anchor_verification(was, now);
    }
    return true;
}

bool vcs_proof_receiver_rebuild(struct vcs_proof_receiver *r,
                                struct vcs_package_store *store,
                                size_t *tickets, size_t *checkpoints,
                                size_t *skipped)
{
    if (tickets) *tickets = 0;
    if (checkpoints) *checkpoints = 0;
    if (skipped) *skipped = 0;
    if (!r || !store)
        LOG_RETURN(false, PTS_LOG, "rebuild: null argument");
    struct vcs_proof_receiver *staging = vcs_proof_receiver_new();
    if (!staging)
        LOG_RETURN(false, PTS_LOG, "rebuild: cannot allocate staging receiver");
    struct pts_cps cps = {0};
    struct pts_counts n = {0};
    struct pts_roots roots = {0};
    uint64_t generation = 0;
    bool ok = pts_scan(staging, store, &cps, &n, &roots, &generation) &&
              pts_replay(staging, &cps, &n) &&
              pts_restore_anchored_fork(r, staging) &&
              pts_preserves_prior(r, staging);
    free(cps.items);
    if (ok) ok = pts_publish_rechecked(r, staging, store, &roots, generation);
    if (!ok)
        n = (struct pts_counts){0};
    vcs_proof_receiver_free(staging);
    free(roots.items);
    if (tickets) *tickets = n.tickets;
    if (checkpoints) *checkpoints = n.checkpoints;
    if (skipped) *skipped = n.skipped;
    return ok;
}
