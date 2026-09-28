/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Package-store catalog validation, bounded paging, and policy mutation. */
#include "package_store_priv.h"
#include "base/log_macros.h"
#include "base/safe_alloc.h"
#include "platform/positioned_file.h"
#include "platform/directory_compat.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif

#define STORE_LOG "vcs.store"

bool store_chunk_inputs_valid(struct vcs_package_store *store,
                              const uint8_t package_root[32],
                              const char *path, const uint8_t *chunk)
{
    return store && package_root && path && chunk;
}

bool store_manifest_identity(const uint8_t *wire, size_t wire_len,
                             uint8_t root[32], uint64_t *total_bytes)
{
    struct vcs_package_manifest manifest;
    if (!vcs_package_manifest_parse(wire, wire_len, &manifest))
        return false;
    if (!vcs_package_manifest_root(&manifest, root)) {
        vcs_package_manifest_free(&manifest);
        return false;
    }
    *total_bytes = 0;
    for (size_t i = 0; i < manifest.count; i++)
        *total_bytes += manifest.files[i].size;
    vcs_package_manifest_free(&manifest);
    return true;
}

const struct vcs_package_file *store_resolve_file(
    const struct store_package *pkg, const char *path)
{
    for (size_t i = 0; i < pkg->manifest.count; i++)
        if (strcmp(pkg->manifest.files[i].path, path) == 0)
            return &pkg->manifest.files[i];
    return NULL;
}

enum vcs_package_store_result store_chunk_hash_checked(
    const struct store_package *pkg, const char *path, uint32_t chunk_index,
    const uint8_t *chunk, size_t chunk_len, uint8_t hash[32])
{
    const struct vcs_package_file *file = store_resolve_file(pkg, path);
    if (!file || chunk_index >= file->chunk_count)
        return VCS_PACKAGE_STORE_ERR_CHUNK_COORD;
    if (!vcs_package_verify_chunk(file, chunk_index, chunk, chunk_len))
        return VCS_PACKAGE_STORE_ERR_CHUNK_HASH;
    memcpy(hash, file->chunk_hashes + (size_t)chunk_index * 32u, 32);
    return VCS_PACKAGE_STORE_OK;
}

uint8_t *store_read_file(const char *path, size_t *out_len)
{
    *out_len = 0;
    struct platform_positioned_file file;
    platform_positioned_file_init(&file);
    if (!platform_positioned_file_open(&file, path))
        LOG_NULL(STORE_LOG, "open regular file %s", path);
    uint64_t size = 0;
    if (!platform_positioned_file_size(&file, &size) || size == 0 ||
        size > VCS_PACKAGE_MANIFEST_MAX_WIRE_BYTES) {
        platform_positioned_file_close(&file);
        LOG_NULL(STORE_LOG, "%s: invalid file or size", path);
    }
    size_t len = (size_t)size;
    uint8_t *buf = zcl_malloc(len, "store_read_file");
    if (!buf) {
        platform_positioned_file_close(&file);
        LOG_NULL(STORE_LOG, "alloc %zu for %s", len, path);
    }
    int64_t got = platform_positioned_file_read(&file, buf, len, 0);
    platform_positioned_file_close(&file);
    if (got != (int64_t)len) {
        free(buf);
        LOG_NULL(STORE_LOG, "read exact file %s", path);
    }
    *out_len = len;
    return buf;
}

static enum vcs_package_store_page_result store_catalog_count_names(
    const char *dir, bool committed, size_t *seen)
{
#if defined(_WIN32)
    struct platform_directory_list files = {0};
    bool listed = committed
        ? platform_directory_list_regular_sorted(dir, &files)
        : platform_directory_list_real_sorted(dir, &files);
    if (!listed)
        return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    for (size_t j = 0; j < files.count; j++) {
        const char *name = files.entries[j].name;
        if (!store_name_is_hex64(name)) continue;
        (*seen)++;
    }
    platform_directory_list_free(&files);
#else
    (void)committed;
    DIR *d = opendir(dir);
    if (!d) return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    enum vcs_package_store_page_result result = VCS_PACKAGE_STORE_PAGE_OK;
    while (true) {
        errno = 0;
        struct dirent *ent = readdir(d);
        if (!ent) {
            if (errno != 0) result = VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
            break;
        }
        if (!store_name_is_hex64(ent->d_name)) continue;
        (*seen)++;
    }
    closedir(d);
    if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
#endif
    return VCS_PACKAGE_STORE_PAGE_OK;
}

static enum vcs_package_store_page_result store_catalog_names_match(
    const struct vcs_package_store *store, bool committed)
{
    char dir[STORE_PATH_MAX];
    int dn = snprintf(dir, sizeof(dir), "%s/%s", store->root,
                      committed ? "manifests" : "staging");
    if (dn <= 0 || (size_t)dn >= sizeof(dir))
        return VCS_PACKAGE_STORE_PAGE_IO;
    size_t seen = 0, expected = 0;
    for (size_t i = 0; i < store->pkg_count; i++)
        if (store->pkgs[i].committed == committed)
            expected++;
    enum vcs_package_store_page_result result =
        store_catalog_count_names(dir, committed, &seen);
    if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
    return seen == expected ? VCS_PACKAGE_STORE_PAGE_OK
                            : VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
}

static enum vcs_package_store_page_result store_catalog_validate_package(
    const struct vcs_package_store *store, const struct store_package *pkg,
    bool check_pin)
{
    char path[STORE_PATH_MAX];
    int n = snprintf(path, sizeof(path), pkg->committed
                         ? "%s/manifests/%s" : "%s/staging/%s/manifest",
                         store->root, pkg->root_hex);
    if (n <= 0 || (size_t)n >= sizeof(path))
        return VCS_PACKAGE_STORE_PAGE_IO;
    size_t len = 0;
    uint8_t *wire = store_read_file(path, &len);
    if (!wire) return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    bool same = len == pkg->manifest_wire_len;
    if (same && pkg->manifest_loaded)
        same = memcmp(wire, pkg->manifest_wire, len) == 0;
    else if (same) {
        struct vcs_package_manifest parsed;
        same = vcs_package_manifest_parse(wire, len, &parsed);
        if (same) {
            uint8_t root[32];
            same = vcs_package_manifest_root(&parsed, root) &&
                   memcmp(root, pkg->root, sizeof(root)) == 0;
            vcs_package_manifest_free(&parsed);
        }
    }
    free(wire);
    if (!same) return VCS_PACKAGE_STORE_PAGE_STALE;
    if (check_pin) {
        n = snprintf(path, sizeof(path), "%s/pins/%s", store->root,
                     pkg->root_hex);
        if (n <= 0 || (size_t)n >= sizeof(path))
            return VCS_PACKAGE_STORE_PAGE_IO;
        if (store_path_exists(path) != pkg->pinned)
            return VCS_PACKAGE_STORE_PAGE_STALE;
    }
    return VCS_PACKAGE_STORE_PAGE_OK;
}

enum vcs_package_store_page_result store_catalog_validate_record_disk(
    const struct vcs_package_store *store, const struct store_package *pkg)
{
    enum vcs_package_store_page_result result =
        store_catalog_validate_package(store, pkg, false);
    if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
    for (size_t i = 0; i < pkg->chunk_count; i++) {
        const uint8_t *hash = pkg->chunks[i].hash;
        if (!store_cas_contains(store, hash)) continue;
        char path[STORE_PATH_MAX];
        store_cas_path(store, hash, path, sizeof(path));
        if (!store_path_exists(path))
            return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    }
    return VCS_PACKAGE_STORE_PAGE_OK;
}

/* A read can finish against an old inode while another handle replaces its
 * path. Recheck the current leaf under the process lock before quarantine. */
static int store_cas_current_state(const char *path,
                                   const struct vcs_package_file *file,
                                   uint32_t chunk_index)
{
    struct platform_positioned_file current;
    platform_positioned_file_init(&current);
    if (!platform_positioned_file_open(&current, path))
        return errno == ENOENT ? 0 : -1;
    uint64_t size = 0;
    if (!platform_positioned_file_size(&current, &size)) {
        platform_positioned_file_close(&current);
        return -1;
    }
    if (size == 0 || size > VCS_PACKAGE_CHUNK_BYTES) {
        platform_positioned_file_close(&current);
        return 0;
    }
    uint8_t *bytes = zcl_malloc((size_t)size, "store_cas_recheck");
    if (!bytes) {
        platform_positioned_file_close(&current);
        return -1;
    }
    int64_t got = platform_positioned_file_read(
        &current, bytes, (size_t)size, 0);
    platform_positioned_file_close(&current);
    int state = got == (int64_t)size
        ? (vcs_package_verify_chunk(file, chunk_index, bytes,
                                    (size_t)size) ? 1 : 0)
        : -1;
    free(bytes);
    return state;
}

enum vcs_package_store_result store_cas_quarantine_if_bad(
    struct vcs_package_store *store, const uint8_t hash[32],
    const struct vcs_package_file *file, uint32_t chunk_index,
    enum vcs_package_store_result observed)
{
    char path[STORE_PATH_MAX];
    store_cas_path(store, hash, path, sizeof(path));
    if (!store_process_lock(store))
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "lock store to quarantine CAS %s", path);
    if (!store_generation_check(store)) {
        store_process_unlock(store);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "stale handle during CAS quarantine %s", path);
    }
    int state = store_cas_current_state(path, file, chunk_index);
    if (state < 0) {
        store_process_unlock(store);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "recheck CAS before quarantine %s", path);
    }
    if (state > 0) {
        store_process_unlock(store);
        LOG_RETURN(observed, STORE_LOG,
                   "CAS changed to valid bytes during read %s", path);
    }
    if (!store_generation_advance(store) || !store_unlink(path)) {
        store_process_unlock(store);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "advance and quarantine invalid CAS %s", path);
    }
    store_cas_remove(store, hash);
    store_packages_touch_hash(store, hash);
    store_process_unlock(store);
    LOG_RETURN(observed, STORE_LOG, "quarantined invalid CAS %s", path);
}

static struct store_package *store_catalog_find_metadata(
    struct vcs_package_store *store, const uint8_t root[32])
{
    size_t lo = 0, hi = store->pkg_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = memcmp(store->pkgs[store->root_order[mid]].root, root, 32);
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
    if (lo == store->pkg_count) return NULL;
    struct store_package *pkg = &store->pkgs[store->root_order[lo]];
    return memcmp(pkg->root, root, 32) == 0 ? pkg : NULL;
}

static enum vcs_package_store_page_result store_catalog_validate_known(
    const struct vcs_package_store *store, bool check_pin)
{
    if (!store || store->catalog_incomplete)
        return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    for (size_t i = 0; i < store->pkg_count; i++) {
        enum vcs_package_store_page_result result =
            store_catalog_validate_package(store, &store->pkgs[i], check_pin);
        if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
    }
    for (size_t i = 0; i < store->cas_count; i++) {
        char path[STORE_PATH_MAX];
        store_cas_path(store, store->cas[i], path, sizeof(path));
        if (!store_path_exists(path))
            return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    }
    return VCS_PACKAGE_STORE_PAGE_OK;
}

enum vcs_package_store_page_result store_catalog_validate_observed_disk(
    const struct vcs_package_store *store)
{
    /* Pin markers may legitimately change through another locked handle.
     * The rebuild below replays those markers from disk; immutable manifests
     * and CAS bytes already observed by this handle must still exist. */
    return store_catalog_validate_known(store, false);
}

enum vcs_package_store_page_result store_catalog_validate_disk(
    const struct vcs_package_store *store)
{
    enum vcs_package_store_page_result result =
        store_catalog_validate_known(store, true);
    if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
    result = store_catalog_names_match(store, true);
    if (result != VCS_PACKAGE_STORE_PAGE_OK) return result;
    return store_catalog_names_match(store, false);
}

bool vcs_package_store_refresh(struct vcs_package_store *store)
{
    if (!store) return false;
    pthread_mutex_lock(&store->lock);
    if (!store_process_lock(store)) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(false, STORE_LOG, "lock store for catalog refresh");
    }
    uint64_t disk_generation = 0;
    if (!store_generation_read(store, &disk_generation)) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(false, STORE_LOG, "read store generation for refresh");
    }
    if (disk_generation == store->shared_generation &&
        !store->catalog_incomplete) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        return true;
    }
    if (store_catalog_validate_observed_disk(store) !=
        VCS_PACKAGE_STORE_PAGE_OK) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(false, STORE_LOG,
                   "refresh refused: previously observed catalog changed");
    }

    struct vcs_package_store rebuilt = {0};
    memcpy(rebuilt.root, store->root, sizeof(rebuilt.root));
    rebuilt.preexisting_root = true;
    rebuilt.process_lock_fd = store->process_lock_fd;
    rebuilt.quota = store->quota;
    rebuilt.shared_generation = disk_generation;
    rebuilt.logical_clock = store->logical_clock;
    rebuilt.next_mutation_generation = store->next_mutation_generation;
    rebuilt.hot_clock = store->hot_clock;
    bool ready = store_open_recover(&rebuilt) &&
        store_generation_check(&rebuilt) &&
        store_catalog_validate_disk(&rebuilt) == VCS_PACKAGE_STORE_PAGE_OK;
    if (!ready) {
        store_partial_catalog_free(&rebuilt);
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(false, STORE_LOG, "rebuild stale package catalog");
    }
    /* Preserve this handle's quota class and eviction hints for retained
     * immutable roots. Pins are disk authority and come from the rebuild. */
    for (size_t i = 0; i < store->pkg_count; i++) {
        const struct store_package *old = &store->pkgs[i];
        struct store_package *same =
            store_catalog_find_metadata(&rebuilt, old->root);
        if (!same) {
            store_partial_catalog_free(&rebuilt);
            store_process_unlock(store);
            pthread_mutex_unlock(&store->lock);
            LOG_RETURN(false, STORE_LOG,
                       "refresh refused: observed manifest missing from rebuild");
        }
        same->class_ = old->class_;
        same->replicas = old->replicas;
        same->access_count = old->access_count;
        same->last_access = old->last_access;
        same->mutation_generation = old->mutation_generation;
    }
    struct vcs_package_store retired = {
        .pkgs = store->pkgs, .pkg_count = store->pkg_count,
        .root_order = store->root_order, .cas = store->cas,
    };
    store->pkgs = rebuilt.pkgs;
    store->pkg_count = rebuilt.pkg_count;
    store->pkg_cap = rebuilt.pkg_cap;
    store->root_order = rebuilt.root_order;
    store->hot_count = rebuilt.hot_count;
    store->hot_clock = rebuilt.hot_clock;
    store->catalog_incomplete = rebuilt.catalog_incomplete;
    store->cas = rebuilt.cas;
    store->cas_count = rebuilt.cas_count;
    store->cas_cap = rebuilt.cas_cap;
    store->manifest_bytes_total = rebuilt.manifest_bytes_total;
    store->logical_clock = rebuilt.logical_clock;
    store->next_mutation_generation = rebuilt.next_mutation_generation;
    store->shared_generation = rebuilt.shared_generation;
    store->gc_orphans_total += rebuilt.gc_orphans_total;
    store_partial_catalog_free(&retired);
    store_process_unlock(store);
    pthread_mutex_unlock(&store->lock);
    return true;
}

enum vcs_package_store_result vcs_package_store_put_manifest_resync(
    struct vcs_package_store *store, const uint8_t *wire, size_t wire_len,
    uint8_t root_out[32])
{
    if (!vcs_package_store_refresh(store))
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "refresh package catalog before manifest admission");
    return vcs_package_store_put_manifest(store, wire, wire_len, root_out);
}

static void store_fill_summary(struct vcs_package_store *store,
                               struct store_package *pkg,
                               struct vcs_package_store_summary *s)
{
    memset(s, 0, sizeof(*s));
    memcpy(s->root, pkg->root, 32);
    s->manifest_bytes = (uint32_t)pkg->manifest_wire_len;
    s->file_count = pkg->file_count;
    s->total_bytes = pkg->total_bytes;
    s->total_chunks = pkg->total_chunks;
    s->complete = store_package_complete(store, pkg);
    s->pinned = pkg->pinned;
}

static size_t store_page_after(const struct vcs_package_store *store,
                               const uint8_t after_root[32])
{
    if (!after_root) return 0;
    size_t lo = 0, hi = store->pkg_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = memcmp(store->pkgs[store->root_order[mid]].root,
                         after_root, 32);
        if (cmp <= 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

enum vcs_package_store_page_result vcs_package_store_page_summaries(
    struct vcs_package_store *store, const uint8_t after_root[32],
    size_t limit, uint64_t expected_generation,
    struct vcs_package_store_summary *rows,
    struct vcs_package_store_page *page)
{
    if (!store || !rows || !page || !limit ||
        limit > VCS_PACKAGE_STORE_PAGE_MAX ||
        (!after_root && expected_generation))
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_INPUT, STORE_LOG,
                   "invalid package page arguments");
    memset(page, 0, sizeof(*page));
    pthread_mutex_lock(&store->lock);
    if (!store_process_lock(store)) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_IO, STORE_LOG,
                   "lock store for catalog page");
    }
    if (!store_generation_check(store)) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_STALE, STORE_LOG,
                   "package catalog changed in another handle");
    }
    if (store->catalog_incomplete) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_INCOMPLETE, STORE_LOG,
                   "package catalog is incomplete");
    }
    if (after_root && expected_generation != store->shared_generation) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_STALE, STORE_LOG,
                   "package page generation changed");
    }
    page->generation = store->shared_generation;
    size_t lo = store_page_after(store, after_root);
    for (size_t i = lo; i < store->pkg_count && page->count < limit; i++) {
        struct store_package *pkg = &store->pkgs[store->root_order[i]];
        store_fill_summary(store, pkg, &rows[page->count++]);
        memcpy(page->next_root, pkg->root, 32);
    }
    page->has_more = lo + page->count < store->pkg_count;
    store_process_unlock(store);
    pthread_mutex_unlock(&store->lock);
    return VCS_PACKAGE_STORE_PAGE_OK;
}

static enum vcs_package_store_page_result store_validate_chunks(
    const struct vcs_package_store *store,
    const uint8_t (*hashes)[32], size_t count)
{
    for (size_t i = 0; i < count; i++) {
        char path[STORE_PATH_MAX];
        store_cas_path(store, hashes[i], path, sizeof(path));
        size_t len = 0;
        uint8_t *wire = store_read_file(path, &len);
        if (!wire) return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
        uint8_t actual[32];
        bool valid = vcs_package_chunk_hash(wire, len, actual) &&
                     memcmp(actual, hashes[i], sizeof(actual)) == 0;
        free(wire);
        if (!valid) return VCS_PACKAGE_STORE_PAGE_INCOMPLETE;
    }
    return VCS_PACKAGE_STORE_PAGE_OK;
}

enum vcs_package_store_page_result vcs_package_store_publish_checked(
    struct vcs_package_store *store, uint64_t generation,
    const uint8_t (*chunk_hashes)[32], size_t chunk_count,
    void (*publish)(void *context), void *context)
{
    if (!store || !publish || (chunk_count && !chunk_hashes))
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_INPUT, STORE_LOG,
                   "invalid guarded publish arguments");
    pthread_mutex_lock(&store->lock);
    if (!store_process_lock(store)) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_IO, STORE_LOG,
                   "lock store for guarded projection publication");
    }
    if (!store_generation_check(store)) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_STALE, STORE_LOG,
                   "store changed in another handle before publish");
    }
    if (generation != store->shared_generation) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_PAGE_STALE, STORE_LOG,
                   "stale guarded publication");
    }
    enum vcs_package_store_page_result disk =
        store_catalog_validate_disk(store);
    if (disk == VCS_PACKAGE_STORE_PAGE_OK)
        disk = store_validate_chunks(store, chunk_hashes, chunk_count);
    if (disk != VCS_PACKAGE_STORE_PAGE_OK) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(disk, STORE_LOG, "store changed before projection publish");
    }
    publish(context);
    store_process_unlock(store);
    pthread_mutex_unlock(&store->lock);
    return VCS_PACKAGE_STORE_PAGE_OK;
}

enum vcs_package_store_page_result vcs_package_store_publish_if_generation(
    struct vcs_package_store *store, uint64_t generation,
    void (*publish)(void *context), void *context)
{
    return vcs_package_store_publish_checked(store, generation, NULL, 0,
                                             publish, context);
}

size_t vcs_package_store_list_summaries(
    struct vcs_package_store *store, bool complete_only,
    struct vcs_package_store_summary *out, size_t max)
{
    if (!store || (!out && max > 0))
        LOG_RETURN(0, STORE_LOG, "null store/summaries out");
    size_t n = 0;
    pthread_mutex_lock(&store->lock);
    for (size_t i = 0; i < store->pkg_count && n < max; i++) {
        struct store_package *pkg = &store->pkgs[i];
        bool complete = store_package_complete(store, pkg);
        if (complete_only && !complete)
            continue;
        store_fill_summary(store, pkg, &out[n++]);
    }
    pthread_mutex_unlock(&store->lock);
    return n;
}

/* ── operator: pins and class ─────────────────────────────────────── */
static enum vcs_package_store_result store_pin_change_locked(
    struct vcs_package_store *store, const uint8_t package_root[32],
    struct store_package *pkg, bool pinned)
{
    char pin[STORE_PATH_MAX];
    snprintf(pin, sizeof(pin), "%s/pins/%s", store->root, pkg->root_hex);
    if (!store_process_lock(store))
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "lock store to change package pin");
    if (!store_generation_check(store) ||
        store_catalog_validate_disk(store) != VCS_PACKAGE_STORE_PAGE_OK) {
        store_process_unlock(store);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "pin change refused: package catalog changed");
    }
    if (pkg->pinned == pinned) {
        store_process_unlock(store);
        return VCS_PACKAGE_STORE_OK;
    }
    if (pinned) {
        uint64_t bytes = 0;
        store_package_present(store, pkg, NULL, &bytes);
        if (!store_ensure_room(store, VCS_PACKAGE_STORE_POOL_PINS, bytes,
                               package_root)) {
            store->quota_rejects_total++;
            store_process_unlock(store);
            return VCS_PACKAGE_STORE_ERR_QUOTA;
        }
        if (!store_generation_advance(store) ||
            !store_atomic_write(pin, NULL, 0)) {
            store_process_unlock(store);
            LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                       "write package pin %s", pin);
        }
        pkg = store_find(store, package_root, NULL);
        if (!pkg) {
            store_process_unlock(store);
            LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                       "pinned package vanished mid-pin");
        }
    } else {
        if (!store_generation_advance(store) || !store_unlink(pin)) {
            store_process_unlock(store);
            LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                       "unlink pin %s: %s", pin, strerror(errno));
        }
    }
    pkg->pinned = pinned;
    store_package_touch(store, pkg);
    store_process_unlock(store);
    return VCS_PACKAGE_STORE_OK;
}

enum vcs_package_store_result vcs_package_store_pin(
    struct vcs_package_store *store, const uint8_t package_root[32],
    bool pinned)
{
    if (!store || !package_root)
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_NULL, STORE_LOG,
                   "null store/root");
    pthread_mutex_lock(&store->lock);
    struct store_package *pkg = store_find(store, package_root, NULL);
    if (store->catalog_incomplete) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "pin refused: catalog incomplete");
    }
    if (!pkg) {
        pthread_mutex_unlock(&store->lock);
        return VCS_PACKAGE_STORE_ERR_UNKNOWN_PACKAGE;
    }
    enum vcs_package_store_result result =
        store_pin_change_locked(store, package_root, pkg, pinned);
    pthread_mutex_unlock(&store->lock);
    return result;
}

static enum vcs_package_store_result store_class_room_locked(
    struct vcs_package_store *store, const uint8_t package_root[32],
    enum vcs_package_store_class class_, struct store_package **pkg_ptr)
{
    struct store_package *pkg = *pkg_ptr;
    if (pkg->class_ == class_ || !store_package_complete(store, pkg) ||
        pkg->pinned)
        return VCS_PACKAGE_STORE_OK;
    uint64_t bytes = 0;
    store_package_present(store, pkg, NULL, &bytes);
    enum vcs_package_store_pool pool =
        class_ == VCS_PACKAGE_STORE_CLASS_HOT
            ? VCS_PACKAGE_STORE_POOL_HOT : VCS_PACKAGE_STORE_POOL_RARE;
    if (!store_ensure_room(store, pool, bytes, package_root)) {
        if (store->catalog_incomplete)
            LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                       "reclassification stopped: catalog incomplete");
        store->quota_rejects_total++;
        return VCS_PACKAGE_STORE_ERR_QUOTA;
    }
    pkg = store_find(store, package_root, NULL);
    if (!pkg)
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "package vanished mid-reclassify");
    *pkg_ptr = pkg;
    return VCS_PACKAGE_STORE_OK;
}

enum vcs_package_store_result vcs_package_store_set_class(
    struct vcs_package_store *store, const uint8_t package_root[32],
    enum vcs_package_store_class class_, uint32_t replicas)
{
    if (!store || !package_root)
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_NULL, STORE_LOG,
                   "null store/root");
    pthread_mutex_lock(&store->lock);
    struct store_package *pkg = store_find(store, package_root, NULL);
    if (store->catalog_incomplete) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "reclassify refused: catalog incomplete");
    }
    if (!pkg) {
        pthread_mutex_unlock(&store->lock);
        return VCS_PACKAGE_STORE_ERR_UNKNOWN_PACKAGE;
    }
    if (!store_process_lock(store)) {
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "lock store to reclassify package");
    }
    if (!store_generation_check(store) ||
        store_catalog_validate_disk(store) != VCS_PACKAGE_STORE_PAGE_OK) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "reclassification refused: package catalog changed");
    }
    if (pkg->class_ == class_ && pkg->replicas == replicas) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        return VCS_PACKAGE_STORE_OK;
    }
    if (!store_generation_advance(store)) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        LOG_RETURN(VCS_PACKAGE_STORE_ERR_IO, STORE_LOG,
                   "advance store generation for reclassification");
    }
    /* A complete class change charges the target pool. Replica updates and
     * incomplete packages retain their current pool accounting. */
    enum vcs_package_store_result room = store_class_room_locked(
        store, package_root, class_, &pkg);
    if (room != VCS_PACKAGE_STORE_OK) {
        store_process_unlock(store);
        pthread_mutex_unlock(&store->lock);
        return room;
    }
    pkg->class_ = class_;
    pkg->replicas = replicas;
    store_package_touch(store, pkg);
    store_process_unlock(store);
    pthread_mutex_unlock(&store->lock);
    return VCS_PACKAGE_STORE_OK;
}

/* ── status + introspection ───────────────────────────────────────── */
