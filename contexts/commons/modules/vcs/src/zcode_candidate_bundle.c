/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Bounded candidate authority transfer over existing content.v2. */

#include "vcs/zcode_candidate_bundle.h"

#include "vcs_priv.h"

#include "util/safe_alloc.h"
#include "vcs/package_store.h"
#include "vcs/vcs.h"
#include "vcs/vcs_object.h"
#include "vcs/zcode_patch.h"
#include "vcs/zcode_write_scope.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t bundle_magic[8] = {
    'Z', 'C', 'B', 'N', 'D', 'L', '\r', '\n'
};

struct bundle_blob {
    uint8_t hash[32];
    uint8_t *bytes;
    size_t len;
};

struct bundle_parts {
    const uint8_t *scope;
    size_t scope_len;
    const uint8_t *patch;
    size_t patch_len;
    const uint8_t *base;
    size_t base_len;
    const uint8_t *candidate;
    size_t candidate_len;
    const uint8_t *blob_records;
    uint32_t blob_count;
};

struct bundle_export_parts {
    uint8_t *scope;
    size_t scope_len;
    uint8_t *patch;
    size_t patch_len;
    uint8_t *base;
    size_t base_len;
    uint8_t *candidate;
    size_t candidate_len;
};

const char *vcs_zcode_candidate_bundle_result_string(
    enum vcs_zcode_candidate_bundle_result result)
{
    switch (result) {
    case VCS_ZCODE_CANDIDATE_BUNDLE_OK: return "ok";
    case VCS_ZCODE_CANDIDATE_BUNDLE_NULL: return "null-argument";
    case VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE: return "noncanonical-bundle";
    case VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT: return "bundle-limit";
    case VCS_ZCODE_CANDIDATE_BUNDLE_CAS: return "cas-miss-or-corrupt";
    case VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY:
        return "candidate-authority-mismatch";
    case VCS_ZCODE_CANDIDATE_BUNDLE_ALLOC: return "allocation-failed";
    }
    return "unknown";
}

static void bundle_blobs_free(struct bundle_blob *blobs, size_t count)
{
    if (!blobs) return;
    for (size_t i = 0; i < count; i++) free(blobs[i].bytes);
    free(blobs);
}

static int bundle_blob_compare(const void *a, const void *b)
{
    const struct bundle_blob *left = a, *right = b;
    return memcmp(left->hash, right->hash, 32);
}

static void bundle_export_parts_free(struct bundle_export_parts *parts)
{
    free(parts->candidate); free(parts->base);
    free(parts->patch); free(parts->scope);
    memset(parts, 0, sizeof(*parts));
}

static enum vcs_zcode_candidate_bundle_result bundle_load_part(
    const char *repo_root, const uint8_t root[32], size_t maximum,
    size_t *remaining, uint8_t **wire, size_t *wire_len)
{
    if (maximum > *remaining) maximum = *remaining;
    int status = vcs_object_load_raw_bounded(
        repo_root, root, maximum, wire, wire_len);
    if (status != 0)
        return status == -2 ? VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT
                            : VCS_ZCODE_CANDIDATE_BUNDLE_CAS;
    *remaining -= *wire_len;
    return VCS_ZCODE_CANDIDATE_BUNDLE_OK;
}

static enum vcs_zcode_candidate_bundle_result bundle_load_export_parts(
    const char *repo_root, const struct vcs_zcode_task_v1 *task,
    const struct vcs_zcode_candidate_v1 *candidate,
    struct bundle_export_parts *parts, size_t *remaining)
{
    memset(parts, 0, sizeof(*parts));
    size_t limit = (size_t)task->max_context_bytes;
    if (limit > (size_t)VCS_PACKAGE_STORE_MAX_PACKAGE_BYTES)
        limit = (size_t)VCS_PACKAGE_STORE_MAX_PACKAGE_BYTES;
    if (limit < VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES)
        return VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT;
    *remaining = limit - VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES;
    enum vcs_zcode_candidate_bundle_result result = bundle_load_part(
        repo_root, task->write_scope_root, VCS_ZCODE_WRITE_SCOPE_WIRE_MAX,
        remaining, &parts->scope, &parts->scope_len);
    if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK)
        result = bundle_load_part(
            repo_root, candidate->patch_root, VCS_ZCODE_PATCH_WIRE_MAX,
            remaining, &parts->patch, &parts->patch_len);
    if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK)
        result = bundle_load_part(
            repo_root, task->source_root, *remaining, remaining,
            &parts->base, &parts->base_len);
    if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK)
        result = bundle_load_part(
            repo_root, candidate->candidate_source_root, *remaining,
            remaining, &parts->candidate, &parts->candidate_len);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK)
        bundle_export_parts_free(parts);
    return result;
}

static enum vcs_zcode_candidate_bundle_result bundle_collect_blobs(
    const char *repo_root, const struct vcs_zcode_patch_v1 *patch,
    struct bundle_blob **out, size_t *out_count, uint64_t max_bytes)
{
    *out = NULL; *out_count = 0;
    struct bundle_blob *blobs = zcl_calloc(
        patch->count, sizeof(*blobs), "zcode.candidate_bundle.blobs");
    if (!blobs) return VCS_ZCODE_CANDIDATE_BUNDLE_ALLOC;
    uint64_t total = 0; size_t count = 0;
    for (size_t i = 0; i < patch->count; i++) {
        const struct vcs_zcode_patch_change_v1 *change = &patch->changes[i];
        if (change->kind == VCS_DIFF_REMOVED) continue;
        bool duplicate = false;
        for (size_t j = 0; j < count; j++)
            if (memcmp(blobs[j].hash, change->new_blob, 32) == 0) {
                duplicate = true; break;
            }
        if (duplicate) continue;
        if (change->new_size > SIZE_MAX ||
            UINT64_MAX - VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES <
                change->new_size) {
            bundle_blobs_free(blobs, count);
            return VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT;
        }
        uint64_t record_size =
            VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES + change->new_size;
        if (UINT64_MAX - total < record_size ||
            total + record_size > max_bytes) {
            bundle_blobs_free(blobs, count);
            return VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT;
        }
        uint8_t *bytes = NULL; size_t len = 0;
        if (vcs_object_get_bounded(
                repo_root, change->new_blob, VCS_TAG_BLOB,
                (size_t)change->new_size, &bytes, &len) != 0 ||
            len != change->new_size) {
            free(bytes); bundle_blobs_free(blobs, count);
            return VCS_ZCODE_CANDIDATE_BUNDLE_CAS;
        }
        memcpy(blobs[count].hash, change->new_blob, 32);
        blobs[count].bytes = bytes; blobs[count].len = len;
        total += record_size; count++;
    }
    qsort(blobs, count, sizeof(*blobs), bundle_blob_compare);
    *out = blobs; *out_count = count;
    return VCS_ZCODE_CANDIDATE_BUNDLE_OK;
}

static bool bundle_add_size(size_t *total, size_t add)
{
    if (SIZE_MAX - *total < add) return false;
    *total += add;
    return true;
}

static void bundle_clear_outputs(uint8_t **wire_out, size_t *wire_len)
{
    if (wire_out) *wire_out = NULL;
    if (wire_len) *wire_len = 0;
}

static enum vcs_zcode_candidate_bundle_result bundle_task_binding(
    const char *repo_root, const struct vcs_zcode_task_v1 *task,
    const struct vcs_zcode_candidate_v1 *candidate)
{
    if (!repo_root || !task || !candidate)
        return VCS_ZCODE_CANDIDATE_BUNDLE_NULL;
    /* Transfer is inert: check the immutable binding at creation, while
     * current execution admission remains the receiving worker's decision. */
    return vcs_zcode_candidate_validate_for_task(
        task, candidate, candidate->created_unix) == VCS_ZCODE_DEV_OK
        ? VCS_ZCODE_CANDIDATE_BUNDLE_OK
        : VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
}

enum vcs_zcode_candidate_bundle_result vcs_zcode_candidate_bundle_export(
    const char *repo_root, const struct vcs_zcode_task_v1 *task,
    const struct vcs_zcode_candidate_v1 *candidate,
    uint8_t **wire_out, size_t *wire_len)
{
    bundle_clear_outputs(wire_out, wire_len);
    if (!wire_out || !wire_len)
        return VCS_ZCODE_CANDIDATE_BUNDLE_NULL;
    enum vcs_zcode_candidate_bundle_result result =
        bundle_task_binding(repo_root, task, candidate);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    if (vcs_zcode_patch_verify_cas(repo_root, task, candidate) !=
        VCS_ZCODE_PATCH_OK)
        return VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
    struct bundle_export_parts parts;
    size_t remaining = 0;
    result = bundle_load_export_parts(
        repo_root, task, candidate, &parts, &remaining);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    struct vcs_zcode_patch_v1 parsed_patch;
    if (vcs_zcode_patch_parse(parts.patch, parts.patch_len, &parsed_patch) !=
        VCS_ZCODE_PATCH_OK) {
        bundle_export_parts_free(&parts);
        return VCS_ZCODE_CANDIDATE_BUNDLE_CAS;
    }
    struct bundle_blob *blobs = NULL; size_t blob_count = 0;
    result = bundle_collect_blobs(
        repo_root, &parsed_patch, &blobs, &blob_count,
        remaining);
    vcs_zcode_patch_free(&parsed_patch);
    size_t total = VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES;
    bool sized = result == VCS_ZCODE_CANDIDATE_BUNDLE_OK &&
        bundle_add_size(&total, parts.scope_len) &&
        bundle_add_size(&total, parts.patch_len) &&
        bundle_add_size(&total, parts.base_len) &&
        bundle_add_size(&total, parts.candidate_len);
    for (size_t i = 0; sized && i < blob_count; i++)
        sized = bundle_add_size(
            &total, VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES) &&
            bundle_add_size(&total, blobs[i].len);
    if (!sized || total > task->max_context_bytes ||
        total > VCS_PACKAGE_STORE_MAX_PACKAGE_BYTES || blob_count > UINT32_MAX) {
        if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK)
            result = VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT;
        bundle_blobs_free(blobs, blob_count);
        bundle_export_parts_free(&parts);
        return result;
    }
    uint8_t *wire = zcl_malloc(total, "zcode.candidate_bundle.wire");
    if (!wire) result = VCS_ZCODE_CANDIDATE_BUNDLE_ALLOC;
    if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK) {
        memcpy(wire, bundle_magic, 8);
        vcs_wr_u16le(wire + 8, VCS_ZCODE_CANDIDATE_BUNDLE_VERSION);
        vcs_wr_u16le(wire + 10, 0);
        vcs_wr_u32le(wire + 12, (uint32_t)blob_count);
        vcs_wr_u64le(wire + 16, parts.scope_len);
        vcs_wr_u64le(wire + 24, parts.patch_len);
        vcs_wr_u64le(wire + 32, parts.base_len);
        vcs_wr_u64le(wire + 40, parts.candidate_len);
        size_t off = VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES;
        memcpy(wire + off, parts.scope, parts.scope_len);
        off += parts.scope_len;
        memcpy(wire + off, parts.patch, parts.patch_len);
        off += parts.patch_len;
        memcpy(wire + off, parts.base, parts.base_len);
        off += parts.base_len;
        memcpy(wire + off, parts.candidate, parts.candidate_len);
        off += parts.candidate_len;
        for (size_t i = 0; i < blob_count; i++) {
            memcpy(wire + off, blobs[i].hash, 32); off += 32;
            vcs_wr_u64le(wire + off, blobs[i].len); off += 8;
            memcpy(wire + off, blobs[i].bytes, blobs[i].len);
            off += blobs[i].len;
        }
        if (off != total) result = VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
    }
    bundle_blobs_free(blobs, blob_count);
    bundle_export_parts_free(&parts);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) {
        free(wire); return result;
    }
    *wire_out = wire; *wire_len = total;
    return VCS_ZCODE_CANDIDATE_BUNDLE_OK;
}

static bool bundle_take(const uint8_t *wire, size_t wire_len, size_t *off,
                        uint64_t wanted, const uint8_t **part,
                        size_t *part_len)
{
    if (wanted > SIZE_MAX || (size_t)wanted > wire_len - *off) return false;
    *part = wire + *off; *part_len = (size_t)wanted; *off += (size_t)wanted;
    return true;
}

static enum vcs_zcode_candidate_bundle_result bundle_parse_parts(
    const uint8_t *wire, size_t wire_len,
    const struct vcs_zcode_task_v1 *task, struct bundle_parts *parts)
{
    memset(parts, 0, sizeof(*parts));
    if (wire_len < VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES ||
        wire_len > task->max_context_bytes ||
        wire_len > VCS_PACKAGE_STORE_MAX_PACKAGE_BYTES ||
        memcmp(wire, bundle_magic, 8) != 0 ||
        vcs_rd_u16le(wire + 8) != VCS_ZCODE_CANDIDATE_BUNDLE_VERSION ||
        vcs_rd_u16le(wire + 10) != 0)
        return VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
    parts->blob_count = vcs_rd_u32le(wire + 12);
    if (parts->blob_count > 4096u) return VCS_ZCODE_CANDIDATE_BUNDLE_LIMIT;
    size_t off = VCS_ZCODE_CANDIDATE_BUNDLE_HEADER_BYTES;
    if (!bundle_take(wire, wire_len, &off, vcs_rd_u64le(wire + 16),
                     &parts->scope, &parts->scope_len) ||
        !bundle_take(wire, wire_len, &off, vcs_rd_u64le(wire + 24),
                     &parts->patch, &parts->patch_len) ||
        !bundle_take(wire, wire_len, &off, vcs_rd_u64le(wire + 32),
                     &parts->base, &parts->base_len) ||
        !bundle_take(wire, wire_len, &off, vcs_rd_u64le(wire + 40),
                     &parts->candidate, &parts->candidate_len))
        return VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
    parts->blob_records = wire + off;
    for (uint32_t i = 0; i < parts->blob_count; i++) {
        if (wire_len - off < VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES)
            return VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
        uint64_t len = vcs_rd_u64le(wire + off + 32);
        off += VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES;
        if (len > SIZE_MAX || (size_t)len > wire_len - off)
            return VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
        off += (size_t)len;
    }
    return off == wire_len ? VCS_ZCODE_CANDIDATE_BUNDLE_OK
                           : VCS_ZCODE_CANDIDATE_BUNDLE_SHAPE;
}

static enum vcs_zcode_candidate_bundle_result bundle_validate_authority(
    const struct bundle_parts *parts,
    const struct vcs_zcode_task_v1 *task,
    const struct vcs_zcode_candidate_v1 *candidate,
    struct vcs_zcode_patch_v1 *patch_out)
{
    vcs_zcode_patch_init(patch_out);
    struct vcs_zcode_write_scope_v1 scope;
    struct vcs_manifest base, candidate_manifest;
    uint8_t checked[32];
    if (vcs_zcode_write_scope_parse(parts->scope, parts->scope_len, &scope) !=
            VCS_ZCODE_WRITE_SCOPE_OK ||
        vcs_zcode_write_scope_root(&scope, checked) !=
            VCS_ZCODE_WRITE_SCOPE_OK ||
        memcmp(checked, task->write_scope_root, 32) != 0 ||
        !vcs_manifest_parse(parts->base, parts->base_len, &base))
        return VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
    bool valid = vcs_manifest_tree_hash(&base, checked) &&
        memcmp(checked, task->source_root, 32) == 0 &&
        vcs_manifest_parse(parts->candidate, parts->candidate_len,
                           &candidate_manifest);
    if (!valid) {
        vcs_manifest_free(&base);
        return VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
    }
    valid = vcs_manifest_tree_hash(&candidate_manifest, checked) &&
        memcmp(checked, candidate->candidate_source_root, 32) == 0 &&
        vcs_zcode_patch_parse(parts->patch, parts->patch_len, patch_out) ==
            VCS_ZCODE_PATCH_OK &&
        vcs_zcode_patch_root(patch_out, checked) == VCS_ZCODE_PATCH_OK &&
        memcmp(checked, candidate->patch_root, 32) == 0;
    if (valid) {
        struct vcs_zcode_patch_v1 derived;
        bool derived_ready = vcs_zcode_patch_derive(
            &base, task->source_root, &candidate_manifest,
            candidate->candidate_source_root, &scope,
            task->max_changed_files, task->max_patch_bytes, &derived) ==
                VCS_ZCODE_PATCH_OK;
        valid = derived_ready &&
            vcs_zcode_patch_root(&derived, checked) == VCS_ZCODE_PATCH_OK &&
            memcmp(checked, candidate->patch_root, 32) == 0;
        if (derived_ready) vcs_zcode_patch_free(&derived);
    }
    vcs_manifest_free(&candidate_manifest); vcs_manifest_free(&base);
    if (!valid) vcs_zcode_patch_free(patch_out);
    return valid ? VCS_ZCODE_CANDIDATE_BUNDLE_OK
                 : VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
}

static bool bundle_blob_expected(const struct vcs_zcode_patch_v1 *patch,
                                 const uint8_t hash[32], uint64_t len)
{
    for (size_t i = 0; i < patch->count; i++)
        if (patch->changes[i].kind != VCS_DIFF_REMOVED &&
            memcmp(patch->changes[i].new_blob, hash, 32) == 0)
            return patch->changes[i].new_size == len;
    return false;
}

static enum vcs_zcode_candidate_bundle_result bundle_validate_blobs(
    const struct bundle_parts *parts,
    const struct vcs_zcode_patch_v1 *patch)
{
    const uint8_t *at = parts->blob_records;
    uint64_t total = 0; uint8_t prior[32] = {0};
    for (uint32_t i = 0; i < parts->blob_count; i++) {
        const uint8_t *hash = at; uint64_t len = vcs_rd_u64le(at + 32);
        at += VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES;
        if ((i > 0 && memcmp(prior, hash, 32) >= 0) ||
            !bundle_blob_expected(patch, hash, len) ||
            UINT64_MAX - total < len)
            return VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
        uint8_t got[32];
        vcs_sha3_tag(VCS_TAG_BLOB, at, (size_t)len, got);
        if (memcmp(got, hash, 32) != 0)
            return VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
        memcpy(prior, hash, 32); total += len; at += (size_t)len;
    }
    size_t unique_expected = 0;
    for (size_t i = 0; i < patch->count; i++) {
        if (patch->changes[i].kind == VCS_DIFF_REMOVED) continue;
        bool seen = false;
        for (size_t j = 0; j < i; j++)
            if (patch->changes[j].kind != VCS_DIFF_REMOVED &&
                memcmp(patch->changes[j].new_blob,
                       patch->changes[i].new_blob, 32) == 0) {
                seen = true; break;
            }
        if (!seen) unique_expected++;
    }
    return unique_expected == parts->blob_count &&
           total <= patch->content_bytes
        ? VCS_ZCODE_CANDIDATE_BUNDLE_OK
        : VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
}

static enum vcs_zcode_candidate_bundle_result bundle_store_blobs(
    const char *repo_root, const struct bundle_parts *parts)
{
    const uint8_t *at = parts->blob_records;
    for (uint32_t i = 0; i < parts->blob_count; i++) {
        const uint8_t *hash = at; uint64_t len = vcs_rd_u64le(at + 32);
        at += VCS_ZCODE_CANDIDATE_BUNDLE_BLOB_HEADER_BYTES;
        uint8_t got[32];
        if (!vcs_object_put_repair(repo_root, at, (size_t)len,
                                   VCS_TAG_BLOB, got, NULL) ||
            memcmp(got, hash, 32) != 0)
            return VCS_ZCODE_CANDIDATE_BUNDLE_CAS;
        at += (size_t)len;
    }
    return VCS_ZCODE_CANDIDATE_BUNDLE_OK;
}

enum vcs_zcode_candidate_bundle_result vcs_zcode_candidate_bundle_import(
    const char *repo_root, const struct vcs_zcode_task_v1 *task,
    const struct vcs_zcode_candidate_v1 *candidate,
    const uint8_t *wire, size_t wire_len)
{
    if (!wire)
        return VCS_ZCODE_CANDIDATE_BUNDLE_NULL;
    enum vcs_zcode_candidate_bundle_result result =
        bundle_task_binding(repo_root, task, candidate);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    struct bundle_parts parts;
    result = bundle_parse_parts(
        wire, wire_len, task, &parts);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    struct vcs_zcode_patch_v1 patch;
    result = bundle_validate_authority(&parts, task, candidate, &patch);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    result = bundle_validate_blobs(&parts, &patch);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) {
        vcs_zcode_patch_free(&patch);
        return result;
    }
    if (!vcs_object_store_init(repo_root) ||
        !vcs_object_put_addressed_repair(repo_root, task->write_scope_root,
                                         parts.scope, parts.scope_len, NULL) ||
        !vcs_object_put_addressed_repair(repo_root, candidate->patch_root,
                                         parts.patch, parts.patch_len, NULL) ||
        !vcs_object_put_addressed_repair(repo_root, task->source_root,
                                         parts.base, parts.base_len, NULL) ||
        !vcs_object_put_addressed_repair(
            repo_root, candidate->candidate_source_root,
            parts.candidate, parts.candidate_len, NULL))
        result = VCS_ZCODE_CANDIDATE_BUNDLE_CAS;
    if (result == VCS_ZCODE_CANDIDATE_BUNDLE_OK)
        result = bundle_store_blobs(repo_root, &parts);
    vcs_zcode_patch_free(&patch);
    if (result != VCS_ZCODE_CANDIDATE_BUNDLE_OK) return result;
    return vcs_zcode_patch_verify_cas(repo_root, task, candidate) ==
            VCS_ZCODE_PATCH_OK
        ? VCS_ZCODE_CANDIDATE_BUNDLE_OK
        : VCS_ZCODE_CANDIDATE_BUNDLE_AUTHORITY;
}
