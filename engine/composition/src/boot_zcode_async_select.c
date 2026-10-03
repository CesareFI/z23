/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Independent-worker selection for async proof dispatch. */

#include "config/boot_zcode_async_select.h"

#include "base/hex.h"
#include "models/build_fabric.h"
#include "models/build_proof_event.h"
#include "util/log_macros.h"
#include "vcs/zcode_work_node.h"

#include <string.h>

static bool async_capability_allows(
    const struct vcs_zcode_work_capability_v1 *capability,
    const struct db_build_job *job, uint8_t work_kind)
{
    uint8_t toolchain[32];
    return capability &&
        zcl_hex_decode_lower(job->toolchain_sha3, toolchain, 32) &&
        capability->queue_headroom > 0 &&
        capability->max_lease_seconds > 0 &&
        capability->target == VCS_ZCODE_WORK_TARGET_LINUX_X86_64_V3 &&
        (capability->work_kinds & (UINT32_C(1) << work_kind)) != 0 &&
        (capability->confinement & VCS_ZCODE_WORK_CONFINEMENT_V1_MASK) ==
            VCS_ZCODE_WORK_CONFINEMENT_V1_MASK &&
        memcmp(capability->toolchain_capsule_root, toolchain, 32) == 0;
}

bool boot_zcode_async_session_lost(
    struct vcs_zcode_work_node *work,
    const struct db_build_proof_event *event)
{
    return work && event && event->peer_id != 0 &&
        !vcs_zcode_work_node_peer_present(work, event->peer_id);
}

/* A request whose session dropped is retried like an expired one: its worker
 * may have finished and queued the RESULT to the dead session. */
bool boot_zcode_async_needs_retry(
    struct vcs_zcode_work_node *work,
    const struct db_build_proof_event *event, int64_t now)
{
    return (event->deadline_at > 0 && now >= event->deadline_at) ||
        boot_zcode_async_session_lost(work, event);
}

static bool async_expired_worker_signer(
    struct vcs_zcode_work_node *work,
    const struct db_build_proof_event *event, int64_t now,
    uint8_t signer_out[32])
{
    uint8_t action_root[32];
    return event->peer_id != 0 && event->deadline_at > 0 &&
        now >= event->deadline_at &&
        zcl_hex_decode_lower(event->action_id, action_root, 32) &&
        vcs_zcode_work_node_outbound_signer(
            work, event->peer_id, event->request_id, action_root,
            signer_out);
}

static bool async_retry_candidate_allows(
    const struct db_build_proof_event *event,
    const struct db_build_job *job, uint8_t work_kind,
    uint64_t peer, const struct vcs_zcode_work_capability_v1 *capability,
    bool retry, bool known_signer, const uint8_t expired_signer[32],
    size_t pass)
{
    if (retry && peer == event->peer_id)
        return false;
    bool same_worker = known_signer &&
        memcmp(capability->signer_pubkey, expired_signer, 32) == 0;
    if (known_signer && ((pass < 2) == same_worker))
        return false;
    struct vcs_zcode_work_capability_v1 probe = *capability;
    if (retry && (pass & 1u) != 0 && probe.queue_headroom == 0)
        probe.queue_headroom = 1;
    return async_capability_allows(&probe, job, work_kind);
}

static bool async_select_from_pool(
    struct vcs_zcode_work_node *work,
    const struct db_build_proof_event *event,
    const struct db_build_job *job, uint8_t work_kind, int64_t now,
    bool retry, uint64_t *peer_out,
    struct vcs_zcode_work_capability_v1 *capability_out)
{
    uint64_t peers[VCS_ZCODE_WORK_NODE_MAX_PEERS];
    struct vcs_zcode_work_capability_v1 capabilities[
        VCS_ZCODE_WORK_NODE_MAX_PEERS];
    size_t count = vcs_zcode_work_node_capable_peers(
        work, now, peers, capabilities, VCS_ZCODE_WORK_NODE_MAX_PEERS);
    uint8_t expired_signer[32] = {0};
    bool known_signer = async_expired_worker_signer(
        work, event, now, expired_signer);
    size_t passes = retry ? (known_signer ? 4 : 2) : 1;
    for (size_t pass = 0; pass < passes; pass++) {
        for (size_t i = 0; i < count; i++) {
            /* Expired leases prefer another signer across all sessions.
             * Same-worker reconnect remains the fallback. Signed zero
             * headroom is probed only on the second pass of each tier;
             * the receiving worker still controls exact admission. */
            if (!async_retry_candidate_allows(
                    event, job, work_kind, peers[i], &capabilities[i],
                    retry, known_signer, expired_signer, pass))
                continue;
            *peer_out = peers[i];
            *capability_out = capabilities[i];
            return true;
        }
    }
    return false;
}

bool boot_zcode_async_select_peer(
    struct vcs_zcode_work_node *work,
    const struct db_build_proof_event *event,
    const struct db_build_job *job, uint8_t work_kind, int64_t now,
    uint64_t *peer_out, struct vcs_zcode_work_capability_v1 *capability_out)
{
    if (!work || !event || !job || !peer_out || !capability_out)
        return false;
    bool retry = boot_zcode_async_needs_retry(work, event, now);
    if (event->peer_id && !retry &&
        vcs_zcode_work_node_peer_capability(
            work, event->peer_id, now, capability_out)) {
        /* This request already consumed its peer slot. Zero advertised
         * headroom prevents a new lease; it must not evict the live one. */
        uint16_t headroom = capability_out->queue_headroom;
        if (headroom == 0) capability_out->queue_headroom = 1;
        bool still_eligible = async_capability_allows(
            capability_out, job, work_kind);
        capability_out->queue_headroom = headroom;
        if (still_eligible) {
            *peer_out = event->peer_id;
            return true;
        }
    }
    return async_select_from_pool(
        work, event, job, work_kind, now, retry, peer_out,
        capability_out);
}

const char *boot_zcode_async_log_no_peer(
    struct vcs_zcode_work_node *work, const struct db_build_job *job,
    const char *action_id, int64_t now)
{
    uint64_t peers[VCS_ZCODE_WORK_NODE_MAX_PEERS];
    struct vcs_zcode_work_capability_v1 caps[VCS_ZCODE_WORK_NODE_MAX_PEERS];
    size_t capable = vcs_zcode_work_node_capable_peers(
        work, now, peers, caps, VCS_ZCODE_WORK_NODE_MAX_PEERS);
    char peer_toolchain[65];
    peer_toolchain[0] = '\0';
    bool toolchain_match = false;
    const char *job_toolchain =
        job && job->toolchain_sha3[0] ? job->toolchain_sha3 : "none";
    if (capable > 0)
        zcl_hex_encode(caps[0].toolchain_capsule_root, 32, peer_toolchain);
    for (size_t i = 0; i < capable; i++) {
        char hex[65];
        zcl_hex_encode(caps[i].toolchain_capsule_root, 32, hex);
        if (strcmp(hex, job_toolchain) == 0) {
            toolchain_match = true;
            break;
        }
    }
    const char *reason = capable == 0 ? "no-capable-worker" :
        toolchain_match ? "worker-busy-or-target-mismatch" :
        "toolchain-capsule-mismatch";
    const char *next = capable == 0
        ? "z23 join"
        : "run zcode work toolchain here and on the proving node";
    LOG_WARN("net.zcode_async",
             "dispatch refused action=%s stage=peer_selection "
             "reason=%s capable=%zu job_toolchain=%s "
             "peer0_toolchain=%s next_action=%s",
             action_id, reason, capable, job_toolchain,
             peer_toolchain[0] ? peer_toolchain : "none", next);
    return next;
}
