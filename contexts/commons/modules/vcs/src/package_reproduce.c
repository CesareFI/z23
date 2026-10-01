/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * package_reproduce — implementation of the bit-identical reproduction
 * verdict declared in vcs/package_reproduce.h. Pure evaluation over two
 * parsed build receipts, plus one bounded directory scan (the package_index
 * / package_verify_policy load() precedent). No compiler, no execution, no
 * network, no trust decisions of its own. */

#include "vcs/package_reproduce.h"

#include "base/hex.h"
#include "base/log_macros.h"
#include "base/safe_alloc.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPRO_LOG "vcs.reproduce"

const char *vcs_reproduce_rule_string(enum vcs_reproduce_rule rule)
{
    switch (rule) {
    case VCS_REPRODUCE_MATCH: return "match";
    case VCS_REPRODUCE_REFERENCE_INVALID: return "reference-invalid";
    case VCS_REPRODUCE_REBUILD_INVALID: return "rebuild-invalid";
    case VCS_REPRODUCE_REFERENCE_NOT_INSTALLABLE:
        return "reference-not-installable";
    case VCS_REPRODUCE_REBUILD_NOT_INSTALLABLE:
        return "rebuild-not-installable";
    case VCS_REPRODUCE_PACKAGE_ROOT_MISMATCH: return "package-root-mismatch";
    case VCS_REPRODUCE_RECIPE_ROOT_MISMATCH: return "recipe-root-mismatch";
    case VCS_REPRODUCE_LOCK_ROOT_MISMATCH: return "lock-root-mismatch";
    case VCS_REPRODUCE_DEP_SET_MISMATCH: return "dependency-set-mismatch";
    case VCS_REPRODUCE_OUTPUT_MISSING: return "output-missing";
    case VCS_REPRODUCE_OUTPUT_UNEXPECTED: return "output-unexpected";
    case VCS_REPRODUCE_OUTPUT_HASH_MISMATCH: return "output-hash-mismatch";
    case VCS_REPRODUCE_OUTPUT_SIZE_MISMATCH: return "output-size-mismatch";
    }
    return "unknown-rule";
}

static void repro_verdict(struct vcs_reproduce_verdict *out,
                          enum vcs_reproduce_rule rule, const char *fmt,
                          ...)
{
    out->reproduced = rule == VCS_REPRODUCE_MATCH;
    out->rule = (uint8_t)rule;
    va_list ap;
    va_start(ap, fmt);
    if (fmt)
        (void)vsnprintf(out->detail, sizeof(out->detail), fmt, ap);
    else
        out->detail[0] = '\0';
    va_end(ap);
}

/* Path plus truncated expected/actual hashes: full hashes live in the
 * receipts themselves; the detail only needs to name the divergence
 * loudly inside VCS_REPRODUCE_DETAIL_MAX. */
static void repro_hash_detail(char *out, size_t cap, const char *path,
                              const uint8_t expected[32],
                              const uint8_t actual[32])
{
    char want[17];
    char got[17];
    zcl_hex_encode(expected, 8, want);
    zcl_hex_encode(actual, 8, got);
    (void)snprintf(out, cap, "%.80s: expected sha3 %s..., got %s...", path,
                   want, got);
}

void vcs_package_reproduce_compare(
    const struct vcs_package_build_receipt *reference,
    const struct vcs_package_build_receipt *rebuild,
    struct vcs_reproduce_verdict *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!reference ||
        vcs_package_build_validate(reference) != VCS_PACKAGE_BUILD_OK) {
        repro_verdict(out, VCS_REPRODUCE_REFERENCE_INVALID,
                      "the reference receipt does not validate");
        return;
    }
    if (!rebuild ||
        vcs_package_build_validate(rebuild) != VCS_PACKAGE_BUILD_OK) {
        repro_verdict(out, VCS_REPRODUCE_REBUILD_INVALID,
                      "the rebuild receipt does not validate");
        return;
    }
    /* A build that did not pass has nothing to reproduce: byte-identity of
     * an empty output set would be vacuous, so the verdict names it. */
    if (!vcs_package_build_installable(reference)) {
        repro_verdict(out, VCS_REPRODUCE_REFERENCE_NOT_INSTALLABLE,
                      "reference verdict %s is not a passing build",
                      vcs_package_build_result_string(
                          (enum vcs_package_build_result)
                              reference->result_class));
        return;
    }
    if (!vcs_package_build_installable(rebuild)) {
        repro_verdict(out, VCS_REPRODUCE_REBUILD_NOT_INSTALLABLE,
                      "rebuild verdict %s is not a passing build",
                      vcs_package_build_result_string(
                          (enum vcs_package_build_result)
                              rebuild->result_class));
        return;
    }
    if (memcmp(reference->package_root, rebuild->package_root, 32) != 0) {
        repro_verdict(out, VCS_REPRODUCE_PACKAGE_ROOT_MISMATCH,
                      "the receipts name different package roots");
        return;
    }
    if (memcmp(reference->recipe_root, rebuild->recipe_root, 32) != 0) {
        repro_verdict(out, VCS_REPRODUCE_RECIPE_ROOT_MISMATCH,
                      "the receipts name different recipe roots");
        return;
    }
    if (memcmp(reference->lock_root, rebuild->lock_root, 32) != 0) {
        repro_verdict(out, VCS_REPRODUCE_LOCK_ROOT_MISMATCH,
                      "the receipts name different dependency locks");
        return;
    }
    if (reference->dep_count != rebuild->dep_count ||
        memcmp(reference->dep_roots, rebuild->dep_roots,
               reference->dep_count * 32u) != 0) {
        repro_verdict(out, VCS_REPRODUCE_DEP_SET_MISMATCH,
                      "the locked dependency sets differ (%zu vs %zu roots)",
                      reference->dep_count, rebuild->dep_count);
        return;
    }
    /* Both output lists are strictly ascending by path (the receipt
     * grammar), so a merge walk finds the first divergence in one pass. */
    size_t i = 0;
    size_t j = 0;
    while (i < reference->output_count || j < rebuild->output_count) {
        if (i == reference->output_count) {
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_UNEXPECTED,
                          "%.120s: the rebuild emitted an output the "
                          "reference does not commit",
                          rebuild->outputs[j].path);
            return;
        }
        if (j == rebuild->output_count) {
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_MISSING,
                          "%.120s: committed by the reference, not emitted "
                          "by the rebuild",
                          reference->outputs[i].path);
            return;
        }
        int cmp = strcmp(reference->outputs[i].path,
                         rebuild->outputs[j].path);
        if (cmp < 0) {
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_MISSING,
                          "%.120s: committed by the reference, not emitted "
                          "by the rebuild",
                          reference->outputs[i].path);
            return;
        }
        if (cmp > 0) {
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_UNEXPECTED,
                          "%.120s: the rebuild emitted an output the "
                          "reference does not commit",
                          rebuild->outputs[j].path);
            return;
        }
        if (memcmp(reference->outputs[i].sha3, rebuild->outputs[j].sha3,
                   32) != 0) {
            char detail[VCS_REPRODUCE_DETAIL_MAX];
            repro_hash_detail(detail, sizeof(detail),
                              reference->outputs[i].path,
                              reference->outputs[i].sha3,
                              rebuild->outputs[j].sha3);
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_HASH_MISMATCH, "%s",
                          detail);
            return;
        }
        if (reference->outputs[i].bytes != rebuild->outputs[j].bytes) {
            repro_verdict(out, VCS_REPRODUCE_OUTPUT_SIZE_MISMATCH,
                          "%.100s: %llu bytes committed, %llu emitted",
                          reference->outputs[i].path,
                          (unsigned long long)reference->outputs[i].bytes,
                          (unsigned long long)rebuild->outputs[j].bytes);
            return;
        }
        i++;
        j++;
    }
    repro_verdict(out, VCS_REPRODUCE_MATCH, NULL);
}

/* ── the receipts-directory scan ────────────────────────────────────── */

struct repro_entry {
    uint8_t id[32];
    struct vcs_package_build_receipt receipt;
    bool matched; /* reference row, or MATCH against it — set in the row pass */
};

static int repro_entry_cmp(const void *a, const void *b)
{
    const struct repro_entry *ea = a;
    const struct repro_entry *eb = b;
    return memcmp(ea->id, eb->id, 32);
}

/* Read one bounded file fully (NULL on any failure/oversize). */
static uint8_t *repro_read_file(const char *path, size_t *out_len)
{
    *out_len = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    uint8_t *buf = zcl_malloc(VCS_REPRODUCE_MAX_WIRE_BYTES,
                              "reproduce_receipt");
    if (!buf) {
        fclose(f);
        LOG_NULL(REPRO_LOG, "alloc receipt buffer for %s", path);
    }
    size_t len = fread(buf, 1, VCS_REPRODUCE_MAX_WIRE_BYTES, f);
    bool bad = ferror(f) || !feof(f) || len == 0;
    if (fclose(f) != 0)
        bad = true;
    if (bad) {
        free(buf);
        return NULL;
    }
    *out_len = len;
    return buf;
}

struct repro_scan {
    const char *receipts_dir;
    const uint8_t *package_root;
    const uint8_t *recipe_root;
    struct vcs_reproduce_report *out;
    struct repro_entry *entries;
    size_t count;
    size_t scanned_entries;
    size_t scan_limit;
    size_t matching_limit;
};

static bool repro_receipt_matches(const char *path,
                                  const uint8_t package_root[32],
                                  const uint8_t recipe_root[32],
                                  struct vcs_package_build_receipt *receipt)
{
    size_t wire_len = 0;
    uint8_t *wire = repro_read_file(path, &wire_len);
    if (!wire)
        return false;
    enum vcs_package_build_error perr =
        vcs_package_build_parse(wire, wire_len, receipt);
    free(wire);
    return perr == VCS_PACKAGE_BUILD_OK &&
           memcmp(receipt->package_root, package_root, 32) == 0 &&
           memcmp(receipt->recipe_root, recipe_root, 32) == 0 &&
           vcs_package_build_installable(receipt);
}

static bool repro_scan_entry(struct repro_scan *scan, const char *name)
{
    uint8_t named_id[32];
    if (!zcl_hex_decode_lower(name, named_id, sizeof(named_id)))
        return true;
    scan->out->scanned++;
    char path[4400];
    int n = snprintf(path, sizeof(path), "%s/%s", scan->receipts_dir, name);
    if (n < 0 || (size_t)n >= sizeof(path))
        return true;
    struct vcs_package_build_receipt receipt;
    if (!repro_receipt_matches(path, scan->package_root, scan->recipe_root,
                               &receipt))
        return true;
    uint8_t receipt_id[32];
    if (vcs_package_build_id(&receipt, receipt_id) != VCS_PACKAGE_BUILD_OK ||
        memcmp(named_id, receipt_id, sizeof(receipt_id)) != 0)
        return true;
    if (scan->count >= scan->matching_limit) {
        scan->out->rows_truncated = true;
        LOG_ERROR(REPRO_LOG, "matching receipt budget exhausted");
        return false;
    }
    if (!scan->entries) {
        scan->entries = zcl_malloc(
            scan->matching_limit * sizeof(*scan->entries),
            "reproduce_entries");
        if (!scan->entries) {
            LOG_ERROR(REPRO_LOG, "alloc %zu receipt entries",
                      scan->matching_limit);
            return false;
        }
    }
    memcpy(scan->entries[scan->count].id, receipt_id, sizeof(receipt_id));
    scan->entries[scan->count].receipt = receipt;
    scan->entries[scan->count].matched = false;
    scan->count++;
    return true;
}

static bool repro_collect(DIR *dir, struct repro_scan *scan)
{
    for (;;) {
        errno = 0;
        struct dirent *ent = readdir(dir);
        if (!ent)
            return errno == 0;
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;
        if (scan->scanned_entries >= scan->scan_limit) {
            LOG_ERROR(REPRO_LOG, "receipt directory scan budget exhausted");
            return false;
        }
        scan->scanned_entries++;
        if (!repro_scan_entry(scan, ent->d_name))
            return false;
    }
}

static bool repro_fill_rows(struct repro_entry *entries, size_t count,
                            struct vcs_reproduce_report *out)
{
    if (count > 0)
        qsort(entries, count, sizeof(*entries), repro_entry_cmp);
    bool all_match = count >= 2;
    for (size_t i = 0; i < count; i++) {
        struct vcs_reproduce_verdict v = {
            .reproduced = true,
            .rule = (uint8_t)VCS_REPRODUCE_MATCH,
        };
        if (i > 0)
            vcs_package_reproduce_compare(&entries[0].receipt,
                                          &entries[i].receipt, &v);
        if (!v.reproduced)
            all_match = false;
        entries[i].matched = v.reproduced;
        struct vcs_reproduce_row *row = &out->rows[out->row_count++];
        memcpy(row->receipt_id, entries[i].id, sizeof(row->receipt_id));
        row->reference = i == 0;
        row->rule = v.rule;
        snprintf(row->detail, sizeof(row->detail), "%s", v.detail);
        row->has_toolchain_capsule =
            entries[i].receipt.has_toolchain_capsule;
        memcpy(row->toolchain_capsule_root,
               entries[i].receipt.toolchain_capsule_root,
               sizeof(row->toolchain_capsule_root));
    }
    return all_match;
}

static void repro_count_toolchains(const struct repro_entry *entries,
                                   size_t count,
                                   struct vcs_reproduce_report *out)
{
    for (size_t i = 0; i < count; i++) {
        if (!entries[i].matched ||
            !entries[i].receipt.has_toolchain_capsule)
            continue;
        const uint8_t *root = entries[i].receipt.toolchain_capsule_root;
        bool zero = true;
        for (size_t b = 0; b < 32 && zero; b++)
            zero = root[b] == 0;
        if (zero)
            continue;
        bool seen = false;
        for (size_t j = 0; j < i && !seen; j++)
            if (entries[j].matched &&
                entries[j].receipt.has_toolchain_capsule &&
                memcmp(entries[j].receipt.toolchain_capsule_root, root,
                       32) == 0)
                seen = true;
        if (!seen)
            out->distinct_toolchains++;
    }
    out->cross_toolchain = out->distinct_toolchains >= 2;
}

static bool repro_scan_bounded(const char *receipts_dir,
                               const uint8_t package_root[32],
                               const uint8_t recipe_root[32],
                               struct vcs_reproduce_report *out,
                               size_t scan_limit, size_t matching_limit)
{
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    if (!receipts_dir || !package_root || !recipe_root)
        return false;
    DIR *dir = opendir(receipts_dir);
    if (!dir)
        return errno == ENOENT; /* no receipts recorded: an empty report */

    struct repro_scan scan = {
        .receipts_dir = receipts_dir,
        .package_root = package_root,
        .recipe_root = recipe_root,
        .out = out,
        .scan_limit = scan_limit,
        .matching_limit = matching_limit,
    };
    bool complete = repro_collect(dir, &scan);
    if (closedir(dir) != 0)
        complete = false;
    if (!complete) {
        free(scan.entries);
        return false;
    }
    out->matching = (uint32_t)scan.count;
    out->reproduced = repro_fill_rows(scan.entries, scan.count, out);
    repro_count_toolchains(scan.entries, scan.count, out);
    free(scan.entries);
    return true;
}

bool vcs_package_reproduce_scan(const char *receipts_dir,
                                const uint8_t package_root[32],
                                const uint8_t recipe_root[32],
                                struct vcs_reproduce_report *out)
{
    return repro_scan_bounded(receipts_dir, package_root, recipe_root, out,
                              VCS_REPRODUCE_MAX_SCAN_ENTRIES,
                              VCS_REPRODUCE_MAX_MATCHING_RECEIPTS);
}

bool vcs_package_reproduce_test_scan_bounded(
    const char *receipts_dir, const uint8_t package_root[32],
    const uint8_t recipe_root[32], struct vcs_reproduce_report *out,
    size_t scan_limit, size_t matching_limit)
{
    if (scan_limit == 0 || scan_limit > VCS_REPRODUCE_MAX_SCAN_ENTRIES ||
        matching_limit == 0 ||
        matching_limit > VCS_REPRODUCE_MAX_MATCHING_RECEIPTS)
        return false;
    return repro_scan_bounded(receipts_dir, package_root, recipe_root, out,
                              scan_limit, matching_limit);
}
