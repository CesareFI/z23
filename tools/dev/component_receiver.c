/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * Verify fetched component bindings before confined disposable execution. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "component_receiver.h"
#include "base/log_macros.h"
#include "base/safe_alloc.h"
#include "vcs/build_action.h"
#include "vcs/build_artifact_manifest.h"
#include "vcs/package_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool cr_refuse(struct zcl_component_receiver_outcome *out,
                       const char *reason)
{
    if (out) (void)snprintf(out->reason, sizeof(out->reason), "%s", reason);
    LOG_WARN("devloop.component", "receiver refused: %s", reason);
    return false;
}

#if defined(__linux__) && defined(__x86_64__) && (defined(ZCL_DEV_BUILD) || defined(ZCL_TESTING))
#include "devloop_reflex_runner_wire.h"
#include "hotswap/hotswap_elf_probe.h"
#include "hotswap/hotswap_sealed_image.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static bool cr_root_set(const uint8_t root[32])
{
    uint8_t bits = 0;
    for (size_t i = 0; i < 32; ++i) bits |= root[i];
    return bits != 0;
}

static bool cr_expectations(const struct zcl_component_receiver_request *r)
{
    return r && cr_root_set(r->package_root) && cr_root_set(r->action_root) &&
        cr_root_set(r->artifact_root) && cr_root_set(r->toolchain_root) &&
        cr_root_set(r->sysroot_root) && cr_root_set(r->harness_root) &&
        cr_root_set(r->fixtures_root) && cr_root_set(r->policy_root) &&
        r->execution.mode == ZCL_REFLEX_MODE_HOT_FORK &&
        r->execution.artifact_sha256;
}

static bool cr_shape(const struct vcs_package_manifest *m)
{
    static const char *const paths[] = { ZCL_COMPONENT_ACTION_PATH,
        ZCL_COMPONENT_MANIFEST_PATH, ZCL_COMPONENT_IMAGE_PATH };
    const uint64_t caps[] = { VCS_ACTION_PREIMAGE_V2_MAX_BYTES,
        VCS_BUILD_ARTIFACT_WIRE_MAX, ZCL_HOTSWAP_SEALED_IMAGE_MAX_BYTES };
    if (m->count != 3) return false;
    for (size_t i = 0; i < 3; ++i)
        if (strcmp(m->files[i].path, paths[i]) != 0 ||
            m->files[i].mode != VCS_PACKAGE_MODE_FILE ||
            m->files[i].size == 0 || m->files[i].size > caps[i])
            return false;
    return true;
}

static bool cr_manifest(struct vcs_package_store *store,
                         const struct zcl_component_receiver_request *r,
                         struct vcs_package_manifest *m,
                         struct zcl_component_receiver_outcome *out)
{
    uint8_t *wire = NULL, root[32];
    size_t len = 0;
    bool ok = vcs_package_store_get_manifest_wire(store, r->package_root,
        &wire, &len) == VCS_PACKAGE_STORE_OK;
    if (ok) out->manifest_bytes_read += len;
    ok = ok && vcs_package_manifest_parse(wire, len, m) &&
        vcs_package_manifest_root(m, root) &&
        memcmp(root, r->package_root, 32) == 0 && cr_shape(m);
    free(wire);
    return ok || cr_refuse(out, "package root or exact carrier shape mismatch");
}

static bool cr_file(struct vcs_package_store *store,
                     const struct zcl_component_receiver_request *r,
                     const struct vcs_package_file *file, uint32_t index,
                     uint8_t **bytes, size_t *length,
                     struct zcl_component_receiver_outcome *out)
{
    *bytes = zcl_malloc((size_t)file->size, "component receiver metadata");
    *length = 0;
    if (!*bytes) return cr_refuse(out, "metadata allocation failed");
    for (uint32_t i = 0; i < file->chunk_count; ++i) {
        uint8_t *chunk = NULL;
        size_t size = 0;
        bool ok = vcs_package_store_get_chunk_at(store, r->package_root,
            index, i, &chunk, &size) == VCS_PACKAGE_STORE_OK;
        if (ok) out->cas_chunk_bytes_read += size;
        ok = ok && *length <= file->size && size <= file->size - *length &&
            vcs_package_verify_chunk(file, i, chunk, size);
        if (ok) { memcpy(*bytes + *length, chunk, size); *length += size; }
        free(chunk);
        if (!ok) return cr_refuse(out, "metadata chunk absent or corrupt");
    }
    return *length == file->size || cr_refuse(out, "metadata size mismatch");
}

static bool cr_target(const struct vcs_action_preimage_v2 *a)
{
    size_t selectors = 0;
    for (size_t i = 0; i < a->argc; ++i) {
        if (strncmp(a->argv[i], "--target=", 9) == 0) {
            if (++selectors != 1 ||
                strcmp(a->argv[i], "--target=" ZCL_COMPONENT_TARGET) != 0)
                return false;
        } else if (strcmp(a->argv[i], "-target") == 0) {
            if (++selectors != 1 || i + 1 >= a->argc ||
                strcmp(a->argv[++i], ZCL_COMPONENT_TARGET) != 0)
                return false;
        }
    }
    return selectors == 1;
}

static bool cr_action_profile(const struct vcs_action_preimage_v2 *a,
    const struct zcl_component_receiver_request *r)
{
    return strcmp(a->stage_kind, ZCL_COMPONENT_STAGE) == 0 &&
        a->stage_version == 1 && a->linker.links && cr_target(a) &&
        a->abi_generation == 1 && a->abi_count == 1 &&
        strcmp(a->abi[0].name, "zcl_hotfork_capsule") == 0 &&
        a->abi[0].version == ZCL_HOTFORK_CAPSULE_ABI_V1 &&
        memcmp(a->toolchain_root, r->toolchain_root, 32) == 0 &&
        memcmp(a->sysroot.objects_sha3, r->sysroot_root, 32) == 0;
}

static bool cr_action_refs(const struct vcs_action_preimage_v2 *a,
    const struct zcl_component_receiver_request *r)
{
    return a->harness.present && a->fixtures.present && a->policy.present &&
        memcmp(a->harness.root, r->harness_root, 32) == 0 &&
        memcmp(a->fixtures.root, r->fixtures_root, 32) == 0 &&
        memcmp(a->policy.root, r->policy_root, 32) == 0;
}

static bool cr_action(const uint8_t *bytes, size_t len,
                       const struct zcl_component_receiver_request *r,
                       struct zcl_component_receiver_outcome *out)
{
    struct vcs_action_preimage_v2_decoded d = {0};
    uint8_t root[32];
    char why[192] = {0};
    bool ok = vcs_action_root_v2_from_bytes(bytes, len, root, why, sizeof(why)) &&
        memcmp(root, r->action_root, 32) == 0 &&
        vcs_action_preimage_v2_decode(bytes, len, &d, why, sizeof(why)) &&
        cr_action_profile(&d.view, r) && cr_action_refs(&d.view, r);
    vcs_action_preimage_v2_decoded_free(&d);
    return ok || cr_refuse(out, "action root, target, stage, ABI or local policy mismatch");
}

static bool cr_artifact(const uint8_t *bytes, size_t len,
                         const struct zcl_component_receiver_request *r,
                         const struct vcs_package_file *file,
                         struct vcs_build_artifact_manifest_v1 *artifact,
                         struct zcl_component_receiver_outcome *out)
{
    uint8_t root[32];
    bool ok = vcs_build_artifact_manifest_v1_parse(bytes, len, artifact) &&
        vcs_build_artifact_manifest_v1_root(artifact, root) &&
        memcmp(root, r->artifact_root, 32) == 0 &&
        memcmp(artifact->action_sha3, r->action_root, 32) == 0 &&
        artifact->total_bytes == file->size &&
        artifact->chunk_bytes == VCS_PACKAGE_CHUNK_BYTES &&
        artifact->chunk_count == file->chunk_count;
    if (ok) ok = memcmp(artifact->chunk_sha3, file->chunk_hashes,
        (size_t)file->chunk_count * 32) == 0;
    return ok || cr_refuse(out, "artifact manifest/action/chunk binding mismatch");
}

static bool cr_write(int fd, const uint8_t *bytes, size_t len,
                      struct zcl_component_receiver_outcome *out)
{
    size_t offset = 0;
    while (offset < len) {
        ssize_t wrote = write(fd, bytes + offset, len - offset);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) return cr_refuse(out, "component assembly write failed");
        offset += (size_t)wrote;
        out->assembly_bytes_written += (size_t)wrote;
    }
    return true;
}

static int cr_image(struct vcs_package_store *store,
                     const struct zcl_component_receiver_request *r,
                     const struct vcs_build_artifact_manifest_v1 *a,
                     struct zcl_component_receiver_outcome *out)
{
    int assembled = memfd_create("z23-component-assembly", MFD_CLOEXEC);
    if (assembled < 0) return cr_refuse(out, "component memfd unavailable"), -1;
    bool ok = true;
    for (uint32_t i = 0; ok && i < a->chunk_count; ++i) {
        uint8_t *chunk = NULL;
        size_t len = 0;
        ok = vcs_package_store_get_chunk_at(store, r->package_root, 2, i,
            &chunk, &len) == VCS_PACKAGE_STORE_OK;
        if (ok) out->cas_chunk_bytes_read += len;
        ok = ok && vcs_build_artifact_manifest_v1_verify_chunk(a, i, chunk, len);
        if (ok) out->artifact_chunk_bytes_verified += len;
        ok = ok && cr_write(assembled, chunk, len, out);
        free(chunk);
    }
    char why[192] = {0};
    int sealed = ok ? hotswap_sealed_image_from_fd(assembled, why, sizeof(why)) : -1;
    (void)close(assembled);
    if (sealed < 0) return cr_refuse(out, "component assembly or sealing failed"), -1;
    out->sealed_copy_bytes = a->total_bytes;
    out->component_bytes = a->total_bytes;
    return sealed;
}

static bool cr_metadata(struct vcs_package_store *store,
    const struct zcl_component_receiver_request *r,
    const struct vcs_package_manifest *m,
    struct vcs_build_artifact_manifest_v1 *artifact,
    struct zcl_component_receiver_outcome *out)
{
    uint8_t *action = NULL, *wire = NULL;
    size_t action_len = 0, wire_len = 0;
    bool ok = cr_file(store, r, &m->files[0], 0, &action, &action_len, out) &&
        cr_action(action, action_len, r, out) &&
        cr_file(store, r, &m->files[1], 1, &wire, &wire_len, out) &&
        cr_artifact(wire, wire_len, r, &m->files[2], artifact, out);
    free(action);
    free(wire);
    return ok;
}

static bool cr_execute(int fd, const struct zcl_component_receiver_request *r,
                        struct zcl_component_receiver_outcome *out)
{
    struct hotswap_elf_hotfork_pure_facts facts;
    char why[192] = {0};
    char digest[65];
    if (!zcl_reflex_sha256_fd(fd, digest) ||
        strcmp(digest, r->execution.artifact_sha256) != 0)
        return cr_refuse(out, "component SHA-256 does not match local expectation");
    if (!hotswap_elf_hotfork_pure_fd(fd, &facts, why, sizeof(why)))
        return cr_refuse(out, why);
    out->admitted = true;
    if (!zcl_reflex_runner_run_fd(fd, &r->execution, &out->runner))
        return cr_refuse(out, out->runner.reason);
    if (!out->runner.green) return cr_refuse(out, "component story did not pass");
    return true;
}
#endif

bool zcl_component_receiver_run(struct vcs_package_store *store,
    const struct zcl_component_receiver_request *r,
    struct zcl_component_receiver_outcome *out)
{
    if (!out) return cr_refuse(NULL, "receiver outcome is required");
    memset(out, 0, sizeof(*out));
#if defined(__linux__) && defined(__x86_64__) && (defined(ZCL_DEV_BUILD) || defined(ZCL_TESTING))
    if (!store || !cr_expectations(r))
        return cr_refuse(out, "complete nonzero local expectations are required");
    struct vcs_package_manifest manifest;
    vcs_package_manifest_init(&manifest);
    struct vcs_build_artifact_manifest_v1 artifact;
    bool ok = cr_manifest(store, r, &manifest, out) &&
        cr_metadata(store, r, &manifest, &artifact, out);
    int fd = ok ? cr_image(store, r, &artifact, out) : -1;
    ok = fd >= 0 && cr_execute(fd, r, out);
    if (fd >= 0) (void)close(fd);
    vcs_package_manifest_free(&manifest);
    return ok;
#else
    (void)store;
    (void)r;
    return cr_refuse(out, "component receiver requires Linux x86-64 development execution");
#endif
}
