/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Sign one proof ticket for each executed plain compile and publish
 *          it through the staged pending row, CAS, and the guarded head. */

#include "build_fabric_proof_context_internal.h"

#include "base/hex.h"
#include "base/log_macros.h"
#include "models/build_fabric.h"
#include "platform/time_compat.h"
#include "services/build_fabric_attach.h"
#include "services/build_fabric_proof_recovery.h"
#include "services/build_fabric_worker.h"
#include "vcs/build_action.h"
#include "vcs/build_execution_observation.h"
#include "vcs/proof_reuse.h"
#include "vcs/vcs_object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include "build_fabric_attach_identity_internal.h"
#endif

#define BFPI_LOG "build_fabric"

#ifdef ZCL_TESTING
static void (*g_bfpi_hook)(enum build_fabric_proof_issue_point, void *);
static void *g_bfpi_hook_context;

void build_fabric_proof_test_issue_hook(
    void (*hook)(enum build_fabric_proof_issue_point point, void *context),
    void *context)
{
    g_bfpi_hook = hook;
    g_bfpi_hook_context = context;
}

#define BFPI_POINT(point) \
    do { if (g_bfpi_hook) g_bfpi_hook((point), g_bfpi_hook_context); } while (0)
#else
#define BFPI_POINT(point) do { } while (0)
#endif

struct zcl_result build_fabric_proof_compile_key(
    const char *workspace, const struct db_build_action *action,
    const uint8_t input_bytes_root[32],
    struct vcs_component_proof_key_v1 *out)
{
    if (!workspace || !action || !input_bytes_root || !out)
        return ZCL_ERR(-1, "proof compile key requires action and input");
#if defined(_WIN32)
    return ZCL_ERR(-1, "proof-key-unavailable: no qualified Windows executor");
#else
    /* The cached capsule and descriptor, as attach composes its key: no
     * toolchain re-capture launches, only the runtime closure probes. */
    struct vcs_toolchain_capsule_v1 capsule;
    struct platform_toolchain_descriptor descriptor;
    uint8_t driver[32], backend[32], assembler[32];
    uint8_t runtime[32], verifier[32], policy[32] = {0};
    if (!vcs_toolchain_capsule_v1_cached(&capsule, &descriptor))
        return ZCL_ERR(-1, "proof-key-toolchain-capsule-unavailable");
    ZCL_CHECK(bfat_cached_tool_hashes(&descriptor, driver, backend,
                                      assembler));
    ZCL_CHECK(bfat_runtime_roots(workspace, &descriptor, runtime, verifier));
    if (action->proof_policy_root_sha3[0] &&
        !zcl_hex_decode_lower(action->proof_policy_root_sha3, policy, 32))
        return ZCL_ERR(-1, "proof-key-policy-root-malformed");
    struct vcs_fixed_compile_proof_inputs inputs = {
        .capsule = &capsule,
        .driver_bytes_sha3 = driver,
        .backend_bytes_sha3 = backend,
        .assembler_bytes_sha3 = assembler,
        .runtime_bytes_sha3 = runtime,
        .verifier_bytes_sha3 = verifier,
        .input_bytes_sha3 = input_bytes_root,
        .proof_policy_root = policy,
        .target = action->target,
        .resource_policy = action->resource_policy,
    };
    if (!vcs_build_action_v1_compile_proof_key(&inputs, out))
        return ZCL_ERR(-1, "proof-key-incomplete");
    return ZCL_OK;
#endif
}

/* What one executed compile proves, each fact re-derived from CAS. */
struct bfpi_facts {
    struct vcs_component_proof_key_v1 key;
    uint8_t input_key[32];
    uint8_t preimage_root[32];
    uint8_t evidence_root[32];
    uint8_t artifact_root[32];
};

static const char *bfpi_observation(
    const char *workspace, const struct db_build_action *action,
    const struct db_build_receipt *receipt,
    struct vcs_build_execution_observation_v1 *obs, uint8_t evidence[32])
{
    uint8_t *wire = NULL, checked[32], action_root[32];
    size_t len = 0;
    if (!zcl_hex_decode_lower(receipt->observation_sha3, evidence, 32) ||
        vcs_object_load_raw(workspace, evidence, &wire, &len) != 0)
        return "observation_absent";
    bool ok = vcs_build_execution_observation_v1_parse(wire, len, obs) &&
              vcs_build_execution_observation_v1_root(obs, checked) &&
              memcmp(checked, evidence, 32) == 0;
    free(wire);
    if (!ok) return "observation_malformed";
    if (!zcl_hex_decode_lower(action->action_id, action_root, 32) ||
        memcmp(obs->action_root, action_root, 32) != 0)
        return "observation_action_mismatch";
    return NULL;
}

/* The key must be the one the worker's checked publish just placed in the
 * workspace: its preimage present at its own root proves both derivations
 * saw the same tool, runtime, and input bytes. */
static const char *bfpi_facts_load(
    struct node_db *ndb, const char *workspace,
    const struct db_build_action *action,
    const struct db_build_receipt *receipt, struct bfpi_facts *facts)
{
    struct db_build_action admitted;
    if (strcmp(action->kind, VCS_BUILD_ACTION_KIND_V1) != 0 ||
        action->task_root_sha3[0] || receipt->exit_status != 0)
        return "not_executed_plain_compile";
    if (strcmp(receipt->action_id, action->action_id) != 0 ||
        !db_build_action_find(ndb, action->action_id, &admitted) ||
        strcmp(admitted.state, "ACCEPTED") != 0)
        return "receipt_not_admitted";
    struct vcs_build_execution_observation_v1 obs;
    const char *refused = bfpi_observation(workspace, action, receipt, &obs,
                                           facts->evidence_root);
    if (refused) return refused;
    if (!build_fabric_proof_compile_key(workspace, action,
                                        obs.observed_input_bytes_root,
                                        &facts->key).ok)
        return "proof_key_unavailable";
    if (!vcs_component_proof_key_derive(&facts->key, facts->input_key) ||
        !vcs_component_proof_key_preimage_root(&facts->key,
                                               facts->preimage_root))
        return "proof_key_incomplete";
    if (!vcs_object_has(workspace, facts->preimage_root))
        return "proof_key_not_published";
    memcpy(facts->artifact_root, obs.output_bytes_root, 32);
    return NULL;
}

static void bfpi_ticket(const struct bfpi_facts *facts,
                        const struct build_fabric_host_accounting *acct,
                        uint64_t now, struct vcs_proof_ticket_v1 *t)
{
    memset(t, 0, sizeof(*t));
    t->verdict = VCS_PROOF_VERDICT_PASS;
    t->basis = VCS_PROOF_BASIS_EXECUTED;
    t->action_class = VCS_PROOF_ACTION_BUILD;
    memcpy(t->input_key, facts->input_key, 32);
    memcpy(t->key_preimage_root, facts->preimage_root, 32);
    memcpy(t->source_root, facts->key.roots[VCS_CPK_SOURCE_CLOSURE], 32);
    memcpy(t->artifact_root, facts->artifact_root, 32);
    memcpy(t->evidence_root, facts->evidence_root, 32);
    t->checks_run = 1;
    t->checks_passed = 1;
    if (acct && acct->measured) {
        int64_t cpu = acct->host_cpu_user_us + acct->host_cpu_system_us;
        t->cpu_us = cpu > 0 ? (uint64_t)cpu : 0;
        t->wall_us = acct->host_wall_us > 0 ? (uint64_t)acct->host_wall_us : 0;
    }
    t->created_unix = now;
}

struct bfpi_wires {
    uint8_t ticket[VCS_PROOF_TICKET_WIRE_BYTES];
    uint8_t checkpoint[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
};

/* Sign in memory, then make the exact wires durable in the order recovery
 * can complete from any prefix: staged row, ticket, checkpoint, head. */
static const char *bfpi_stage(struct build_fabric_proof_context *ctx,
                              struct node_db *ndb,
                              struct vcs_proof_ticket_v1 *ticket,
                              struct bfpi_wires *w)
{
    struct db_build_worker row;
    if (db_build_worker_find_checked(ndb, ctx->worker_id, &row) != 1 ||
        strcmp(row.signer_pubkey, ctx->signer_hex) != 0)
        return "worker_row_unreadable";
    if (!vcs_proof_issuer_log_append(ctx->issuer, ticket, w->ticket) ||
        !vcs_proof_issuer_log_checkpoint(ctx->issuer, ticket->created_unix,
                                         w->checkpoint))
        return "issuer_append_refused";
    struct db_build_worker_proof_pending pending;
    memset(&pending, 0, sizeof(pending));
    memcpy(pending.worker_id, ctx->worker_id, sizeof(pending.worker_id));
    memcpy(pending.signer_pubkey, ctx->signer_hex,
           sizeof(pending.signer_pubkey));
    memcpy(pending.expected_head, row.proof_checkpoint_head_sha3,
           sizeof(pending.expected_head));
    memcpy(pending.ticket_wire, w->ticket, sizeof(w->ticket));
    memcpy(pending.checkpoint_wire, w->checkpoint, sizeof(w->checkpoint));
    if (!db_build_worker_proof_pending_stage(ndb, &pending))
        return "pending_stage_refused";
    BFPI_POINT(BUILD_FABRIC_PROOF_ISSUE_AFTER_STAGE);
    return NULL;
}

static const char *bfpi_publish(struct build_fabric_proof_context *ctx,
                                struct node_db *ndb,
                                const struct bfpi_wires *w)
{
    uint8_t root[32];
    if (!vcs_proof_ticket_store_put_no_evict(ctx->store, w->ticket,
                                             sizeof(w->ticket), root))
        return "ticket_put_refused";
    BFPI_POINT(BUILD_FABRIC_PROOF_ISSUE_AFTER_TICKET_PUT);
    if (!vcs_proof_ticket_store_put_no_evict(ctx->store, w->checkpoint,
                                             sizeof(w->checkpoint), root))
        return "checkpoint_put_refused";
    BFPI_POINT(BUILD_FABRIC_PROOF_ISSUE_AFTER_CHECKPOINT_PUT);
    if (!build_fabric_proof_pending_recover(ndb, ctx->store, ctx->worker_id,
                                            ctx->seed).ok)
        return "head_commit_refused";
    return NULL;
}

/* The receiver learns the new ticket through the same verified sync a peer
 * would use; a refusal leaves it unchanged and names why. */
static void bfpi_receiver_sync(struct build_fabric_proof_context *ctx,
                               const struct bfpi_wires *w)
{
    if (!ctx->receiver) return;
    const uint8_t *delta[1] = { w->ticket };
    size_t lens[1] = { sizeof(w->ticket) };
    struct vcs_proof_sync_report report;
    bool synced = vcs_proof_receiver_sync(ctx->receiver, w->checkpoint,
                                          sizeof(w->checkpoint), delta, lens,
                                          1, &report) &&
                  (report.outcome == VCS_PROOF_SYNC_ADVANCED ||
                   report.outcome == VCS_PROOF_SYNC_CURRENT);
    if (!synced) {
        atomic_store(&ctx->live.receiver_state,
                     BUILD_FABRIC_PROOF_STATE_SYNC_REFUSED);
        LOG_ERROR(BFPI_LOG, "proof receiver refused own ticket: %s",
                  report.reason ? report.reason : "unknown");
    }
    atomic_store(&ctx->live.receiver_tickets,
                 vcs_proof_receiver_ticket_count(ctx->receiver));
}

/* A staged row left by an earlier refused publication is completed first,
 * so the in-memory issuer always extends the durable head. */
static const char *bfpi_prepare(struct build_fabric_proof_context *ctx,
                                struct node_db *ndb)
{
    struct db_build_worker_proof_pending pending;
    int found = db_build_worker_proof_pending_find_checked(
        ndb, ctx->worker_id, &pending);
    if (found < 0) return "pending_unreadable";
    if (found > 0) {
        if (!build_fabric_proof_pending_recover(ndb, ctx->store,
                                                ctx->worker_id,
                                                ctx->seed).ok)
            return "pending_recovery_refused";
        bfpc_issuer_resync(ctx, ndb);
    }
    return ctx->issuer ? NULL : "issuer_unavailable";
}

static const char *bfpi_issue(struct build_fabric_proof_context *ctx,
                              struct node_db *ndb, const char *workspace,
                              const struct db_build_action *action,
                              const struct db_build_receipt *receipt,
                              const struct build_fabric_host_accounting *acct)
{
    if (!ctx->store) return "store_unavailable";
    struct bfpi_facts facts;
    const char *refused = bfpi_facts_load(ndb, workspace, action, receipt,
                                       &facts);
    if (refused) return refused;
    refused = bfpi_prepare(ctx, ndb);
    if (refused) return refused;
    uint8_t preimage[VCS_CPK_WIRE_BYTES], root[32];
    if (!vcs_component_proof_key_encode(&facts.key, preimage) ||
        !vcs_proof_ticket_store_put_no_evict(ctx->store, preimage,
                                             sizeof(preimage), root) ||
        memcmp(root, facts.preimage_root, 32) != 0)
        return "key_preimage_put_refused";
    struct vcs_proof_ticket_v1 ticket;
    bfpi_ticket(&facts, acct, (uint64_t)platform_time_wall_unix(), &ticket);
    struct bfpi_wires wires;
    refused = bfpi_stage(ctx, ndb, &ticket, &wires);
    if (!refused) refused = bfpi_publish(ctx, ndb, &wires);
    if (!refused) bfpi_receiver_sync(ctx, &wires);
    return refused;
}

struct zcl_result build_fabric_proof_issue_executed(
    struct build_fabric_proof_context *ctx, struct node_db *ndb,
    const char *workspace, const struct db_build_action *action,
    const struct db_build_receipt *receipt,
    const struct build_fabric_host_accounting *accounting)
{
    if (!ctx || !ndb || !workspace || !action || !receipt)
        return ZCL_ERR(-1, "proof issue requires context, action, receipt");
    const char *refused = bfpi_issue(ctx, ndb, workspace, action, receipt,
                                     accounting);
    if (refused && strcmp(refused, "not_executed_plain_compile") == 0)
        return ZCL_OK;
    if (refused) {
        atomic_fetch_add(&ctx->live.issue_refused, 1);
        atomic_store(&ctx->live.last_issue_refusal, refused);
        bfpc_issuer_resync(ctx, ndb);
        LOG_WARN(BFPI_LOG,
                 "schema=zcl.proof_issue.v1 action=%s issued=false "
                 "refusal=%s", action->action_id, refused);
        return ZCL_ERR(-1, "proof ticket not issued: %s", refused);
    }
    atomic_fetch_add(&ctx->live.issued, 1);
    atomic_store(&ctx->live.issuer_leaves,
                 vcs_proof_issuer_log_count(ctx->issuer));
    LOG_INFO(BFPI_LOG,
             "schema=zcl.proof_issue.v1 action=%s issued=true leaves=%llu",
             action->action_id,
             (unsigned long long)vcs_proof_issuer_log_count(ctx->issuer));
    return ZCL_OK;
}
