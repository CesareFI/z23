/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Facts-narrowed change closure: decide from semantic manifests which functions changed, then walk callers from those alone. */
#include "devloop_facts.h"

#include "base/serialize_le.h"
#include "codeindex/codeindex.h"
#include "sha3/sha3.h"
#include "util/safe_alloc.h"
#include "vcs/semantic_manifest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The evidence rules, in order. The first that fails names the fallback;
 * the file-seeded closure then runs unchanged.
 *   0. No changed file is a source compiled into a producer.
 *   1. Every changed file is a .c with a before/after manifest pair.
 *   2. Each manifest is valid, carries the facts extension and is complete,
 *      and names its producer; both name the same one (a different sensor
 *      or front end build is "producer-changed").
 *   3. Each manifest's main file is the changed file and its FILES digest
 *      is the sha3 of the source bytes given with it.
 *   4. Only FILES, FUNCTIONS, SPANS, REFS, UNKNOWNS and FACTS differ; every
 *      other section names the fallback (identity, lookups, macros, decls,
 *      layouts, enums, symbols, probes, truncation).
 *   5. No function was added or removed; no included file changed.
 *   6. The main file's text outside its function definitions is unchanged
 *      (devloop_facts_text.c), and so is each definition's head: the text
 *      before its body, where attributes and storage class live.
 *   7. Every seed resolved to its id; no seed has an unresolved effect other
 *      than an external call, and no seed's address is taken in either
 *      manifest.
 *   8. REFS and UNKNOWNS outside the seeds are identical.
 *   9. The after manifest is the compile of the tree under root: every other
 *      file it read has the same bytes and every include slot it saw absent
 *      is still absent (devloop_facts_bind.c).
 * The seeds are the functions whose FUNCTIONS record changed. */

#define FX_ID_MAX (ZCL_DEVLOOP_PATH_MAX + ZCL_DEVLOOP_FACTS_NAME_MAX + 8)

struct fx_ctx {
    const char *root; /* the tree rule 9 binds the after manifests to */
    const struct zcl_devloop_facts_tu *tu;
    struct zcl_devloop_facts_verdict *v;
    size_t first_seed; /* this TU's seeds start here in v->seeds */
    char ids[ZCL_DEVLOOP_FACTS_MAX_SEEDS][FX_ID_MAX];
    bool added, removed, outside, overflow;
    uint8_t src_digest[32];
    bool main_seen;
    struct sha3_256_ctx rest; /* records a narrowing must leave unchanged */
    struct zcl_devloop_facts_range *ranges;
    size_t nranges, capranges;
};

static bool fx_fail(struct fx_ctx *c, const char *reason, const char *fmt, ...)
{
    va_list ap;
    if (c->v->reason[0])
        return false;
    c->v->reason = reason;
    va_start(ap, fmt);
    (void)vsnprintf(c->v->detail, sizeof(c->v->detail), fmt, ap);
    va_end(ap);
    return false;
}

static bool fx_eq(const char *t, size_t n, const char *s)
{
    return n == strlen(s) && memcmp(t, s, n) == 0;
}

static void fx_write_text(struct sha3_256_ctx *h, const char *t, size_t n)
{
    uint8_t len[8];
    zcl_write_u64_le(len, (uint64_t)n);
    sha3_256_write(h, len, sizeof(len));
    sha3_256_write(h, (const unsigned char *)t, n);
}

static void fx_write_num(struct sha3_256_ctx *h, uint64_t v)
{
    uint8_t b[8];
    zcl_write_u64_le(b, v);
    sha3_256_write(h, b, sizeof(b));
}

static bool fx_is_seed_id(const struct fx_ctx *c, const char *t, size_t n)
{
    for (size_t k = c->first_seed; k < c->v->seeds_len; k++)
        if (fx_eq(t, n, c->ids[k]))
            return true;
    return false;
}

/* ---- rule 0: not the producer's own change ------------------------------- */

/* The repo sources compiled into the producer: the Makefile's
 * CLANG_MANIFEST_SRCS and every repo header they include (the prefixes
 * below cover each such file). A producer never judges its own change. */
static bool fx_producer_source(const char *f)
{
    static const char *const k[] = {
        "tools/sensors/",
        "contexts/commons/modules/vcs/src/semantic_",
        "contexts/commons/modules/vcs/include/vcs/semantic_",
        "contexts/commons/modules/vcs/include/vcs/vcs_manifest.h",
        "contexts/commons/modules/vcs/src/vcs_path_policy.c",
        "platform/modules/base/src/safe_alloc.c",
        "platform/modules/base/include/base/safe_alloc.h",
        "platform/modules/base/include/base/serialize_le.h",
        "platform/modules/base/include/base/hex.h",
        "platform/modules/platform/src/os_proc.c",
        "platform/modules/platform/include/platform/os_proc.h",
        "platform/modules/sha3/",
    };
    for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); i++)
        if (strncmp(f, k[i], strlen(k[i])) == 0)
            return true;
    return false;
}

/* ---- rules 2-4: validity, binding, sections --------------------------- */

static const char *fx_section_reason(uint32_t changed)
{
    static const struct {
        enum vcs_semantic_section_v1 s;
        const char *reason;
    } k[] = {
        {VCS_SEMANTIC_SECTION_V1_IDENTITY, "identity-changed"},
        {VCS_SEMANTIC_SECTION_V1_LOOKUPS, "include-resolution-changed"},
        {VCS_SEMANTIC_SECTION_V1_MACROS, "macro-changed"},
        {VCS_SEMANTIC_SECTION_V1_DECLS, "declaration-changed"},
        {VCS_SEMANTIC_SECTION_V1_LAYOUTS, "layout-changed"},
        {VCS_SEMANTIC_SECTION_V1_ENUMS, "enum-changed"},
        {VCS_SEMANTIC_SECTION_V1_SYMBOLS, "symbols-changed"},
        {VCS_SEMANTIC_SECTION_V1_PROBES, "namespace-probes-changed"},
        {VCS_SEMANTIC_SECTION_V1_TRUNCATED, "manifest-truncated"},
    };
    for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); i++)
        if (changed & (1u << k[i].s))
            return k[i].reason;
    return NULL;
}

static bool fx_manifest_ok(struct fx_ctx *c, const uint8_t *m, size_t n,
                           const char *which, uint8_t producer[32])
{
    static const uint8_t zero[32] = {0};
    struct vcs_semantic_facts_info_v1 info;
    if (m == NULL || !vcs_semantic_facts_v1_info(m, n, &info))
        return fx_fail(c, "invalid-manifest", "%s manifest of %s is invalid",
                       which, c->tu->source);
    if (!info.present)
        return fx_fail(c, "no-facts", "%s manifest of %s has no facts",
                       which, c->tu->source);
    if (!info.complete)
        return fx_fail(c, "manifest-truncated",
                       "%s manifest of %s hit a producer cap", which,
                       c->tu->source);
    if (memcmp(info.producer, zero, 32) == 0)
        return fx_fail(c, "producer-unknown",
                       "%s manifest of %s names no producer", which,
                       c->tu->source);
    memcpy(producer, info.producer, 32);
    return true;
}

/* FILES: the main file binds the given source; every other file is folded
 * into `rest` so an included file's change is caught. */
static bool fx_files_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct fx_ctx *c = ctx;
    if (f->ntext < 1 || f->nnum < 1 || f->ndigest < 1)
        return false;
    if (f->num[0] == VCS_SEMANTIC_ORIGIN_V1_MAIN) {
        c->main_seen = fx_eq(f->text[0], f->text_len[0], c->tu->source) &&
                       memcmp(f->digest[0], c->src_digest, 32) == 0;
        return true;
    }
    fx_write_text(&c->rest, f->text[0], f->text_len[0]);
    sha3_256_write(&c->rest, f->digest[0], 32);
    fx_write_num(&c->rest, f->num[0]);
    return true;
}

static void fx_fn_cb(void *ctx, enum vcs_semantic_fn_change_v1 change,
                     const char *path, size_t path_len, const char *name,
                     size_t name_len)
{
    struct fx_ctx *c = ctx;
    struct zcl_devloop_facts_verdict *v = c->v;
    if (change == VCS_SEMANTIC_FN_V1_ADDED)
        c->added = true;
    else if (change == VCS_SEMANTIC_FN_V1_REMOVED)
        c->removed = true;
    else if (!fx_eq(path, path_len, c->tu->source))
        c->outside = true;
    else if (v->seeds_len >= ZCL_DEVLOOP_FACTS_MAX_SEEDS ||
             name_len >= ZCL_DEVLOOP_FACTS_NAME_MAX)
        c->overflow = true;
    else {
        memcpy(v->seeds[v->seeds_len], name, name_len);
        v->seeds[v->seeds_len][name_len] = '\0';
        c->ids[v->seeds_len][0] = '\0';
        v->seeds_len++;
    }
}

static bool fx_diff(struct fx_ctx *c)
{
    const struct zcl_devloop_facts_tu *t = c->tu;
    struct vcs_semantic_diff_v1 d;
    const char *why;
    if (!vcs_semantic_manifest_v1_diff(t->before, t->before_len, t->after,
                                       t->after_len, &d, fx_fn_cb, c))
        return fx_fail(c, "invalid-manifest", "cannot diff %s", t->source);
    why = fx_section_reason(d.changed_sections);
    if (why != NULL)
        return fx_fail(c, why, "%s", t->source);
    if (c->added || c->removed)
        return fx_fail(c, c->added ? "function-added" : "function-removed",
                       "%s", t->source);
    if (c->outside)
        return fx_fail(c, "seed-outside-source", "%s", t->source);
    if (c->overflow)
        return fx_fail(c, "too-many-seeds", "%s", t->source);
    return true;
}

/* ---- rule 6: file-scope text ------------------------------------------- */

static bool fx_spans_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct fx_ctx *c = ctx;
    struct zcl_devloop_facts_range *grown;
    if (f->ntext < 1 || f->nnum < 4)
        return false;
    if (!fx_eq(f->text[0], f->text_len[0], c->tu->source))
        return true;
    if (c->nranges == c->capranges) {
        size_t next = c->capranges ? c->capranges * 2 : 64;
        grown = zcl_realloc(c->ranges, next * sizeof(*grown), "facts.ranges");
        if (grown == NULL)
            return false;
        c->ranges = grown;
        c->capranges = next;
    }
    c->ranges[c->nranges].begin = (size_t)f->num[2];
    c->ranges[c->nranges].end = (size_t)f->num[3];
    c->nranges++;
    return true;
}

static int fx_range_cmp(const void *a, const void *b)
{
    const struct zcl_devloop_facts_range *x = a, *y = b;
    if (x->begin != y->begin)
        return x->begin < y->begin ? -1 : 1;
    return x->end < y->end ? -1 : x->end > y->end;
}

/* What one side of a pair must leave unchanged for a narrowing. */
struct fx_digests {
    uint8_t scope[32]; /* file-scope text outside every definition */
    uint8_t heads[32]; /* each definition's head, in source order */
    uint8_t rest[32];  /* included files, refs and unknowns outside seeds */
};

/* Each definition's head: the text before its body, where storage class,
 * GNU __attribute__ and C23 [[attributes]], return type and declarator live.
 * A constructor, weak, alias, section or visibility attribute changes what
 * the program does without touching the body's tokens, so any head change
 * refuses the narrowing. */
static void fx_heads_digest(const uint8_t *src,
                            const struct zcl_devloop_facts_range *r, size_t n,
                            uint8_t out[32])
{
    struct sha3_256_ctx h;
    sha3_256_init(&h);
    for (size_t k = 0; k < n; k++) {
        uint8_t d[32];
        zcl_devloop_facts_head_digest(src, r[k].begin, r[k].end, d);
        sha3_256_write(&h, d, sizeof(d));
    }
    sha3_256_finalize(&h, out);
}

static bool fx_scope_digest(struct fx_ctx *c, const uint8_t *m, size_t n,
                            const uint8_t *src, size_t src_len,
                            struct fx_digests *out)
{
    size_t w = 0;
    c->nranges = 0;
    if (!vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_SPANS,
                                      fx_spans_cb, c))
        return fx_fail(c, "invalid-manifest", "spans of %s", c->tu->source);
    qsort(c->ranges, c->nranges, sizeof(*c->ranges), fx_range_cmp);
    for (size_t k = 0; k < c->nranges; k++) {
        struct zcl_devloop_facts_range r = c->ranges[k];
        if (r.end > src_len || r.begin > r.end)
            return fx_fail(c, "source-mismatch",
                           "a span of %s lies outside its source",
                           c->tu->source);
        if (w > 0 && r.begin <= c->ranges[w - 1].end) {
            if (r.end > c->ranges[w - 1].end)
                c->ranges[w - 1].end = r.end;
            continue;
        }
        c->ranges[w++] = r;
    }
    zcl_devloop_facts_text_digest(src, src_len, c->ranges, w, out->scope);
    fx_heads_digest(src, c->ranges, w, out->heads);
    return true;
}

/* ---- rules 7-8: seed effects and the rest of the facts ------------------ */

#if defined(ZCL_TESTING)
bool zcl_devloop_test_facts_unresolved = false;
#endif

static bool fx_ids_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct fx_ctx *c = ctx;
    char path[ZCL_DEVLOOP_PATH_MAX];
    if (f->ntext < 2 || f->nnum < 1)
        return false;
#if defined(ZCL_TESTING)
    if (zcl_devloop_test_facts_unresolved)
        return true;
#endif
    if (!fx_eq(f->text[0], f->text_len[0], c->tu->source) ||
        f->text_len[0] >= sizeof(path))
        return true;
    memcpy(path, f->text[0], f->text_len[0]);
    path[f->text_len[0]] = '\0';
    for (size_t k = c->first_seed; k < c->v->seeds_len; k++) {
        if (!fx_eq(f->text[1], f->text_len[1], c->v->seeds[k]))
            continue;
        if (!vcs_semantic_id_v1('f', path, c->v->seeds[k],
                                f->num[0] == 3 || f->num[0] == 4, c->ids[k],
                                sizeof(c->ids[k])))
            return false;
    }
    return true;
}

static bool fx_refs_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct fx_ctx *c = ctx;
    if (f->ntext < 2 || f->nnum < 1)
        return false;
    if (f->num[0] == VCS_SEMANTIC_REF_V1_ADDRESS &&
        fx_is_seed_id(c, f->text[1], f->text_len[1]))
        return fx_fail(c, "address-taken", "%.*s names %.*s outside a call",
                       (int)f->text_len[0], f->text[0], (int)f->text_len[1],
                       f->text[1]);
    if (fx_is_seed_id(c, f->text[0], f->text_len[0]))
        return true;
    fx_write_text(&c->rest, f->text[0], f->text_len[0]);
    fx_write_num(&c->rest, f->num[0]);
    fx_write_text(&c->rest, f->text[1], f->text_len[1]);
    return true;
}

static bool fx_unknowns_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct fx_ctx *c = ctx;
    if (f->ntext < 2 || f->nnum < 2)
        return false;
    if (fx_is_seed_id(c, f->text[0], f->text_len[0])) {
        if (f->num[0] == VCS_SEMANTIC_UNKNOWN_V1_EXTERNAL_CALL)
            return true;
        return fx_fail(c, "unknown-effect", "%.*s: kind %u (%.*s)",
                       (int)f->text_len[0], f->text[0], (unsigned)f->num[0],
                       (int)f->text_len[1], f->text[1]);
    }
    fx_write_text(&c->rest, f->text[0], f->text_len[0]);
    fx_write_num(&c->rest, f->num[0]);
    fx_write_text(&c->rest, f->text[1], f->text_len[1]);
    fx_write_num(&c->rest, f->num[1]);
    return true;
}

/* One side of the pair: its binding, text digests and `rest` digest. */
static bool fx_side(struct fx_ctx *c, const uint8_t *m, size_t n,
                    const uint8_t *src, size_t src_len, struct fx_digests *out)
{
    zcl_sha3_256(src, src_len, c->src_digest);
    c->main_seen = false;
    sha3_256_init(&c->rest);
    if (!vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_FILES,
                                      fx_files_cb, c))
        return fx_fail(c, "invalid-manifest", "files of %s", c->tu->source);
    if (!c->main_seen)
        return fx_fail(c, "source-mismatch",
                       "a manifest of %s does not bind the given source",
                       c->tu->source);
    if (!vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_REFS,
                                      fx_refs_cb, c) ||
        !vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_UNKNOWNS,
                                      fx_unknowns_cb, c))
        return fx_fail(c, "invalid-manifest", "facts of %s", c->tu->source);
    sha3_256_finalize(&c->rest, out->rest);
    return fx_scope_digest(c, m, n, src, src_len, out);
}

/* Every seed must have resolved to its canonical id; a seed that matched no
 * FUNCTIONS record would exempt nothing and escape rules 7-8 silently. */
static bool fx_seeds_resolved(struct fx_ctx *c)
{
    for (size_t k = c->first_seed; k < c->v->seeds_len; k++)
        if (c->ids[k][0] == '\0')
            return fx_fail(c, "seed-unresolved", "%s: %s", c->tu->source,
                           c->v->seeds[k]);
    return true;
}

static bool fx_compare(struct fx_ctx *c, const struct fx_digests *b,
                       const struct fx_digests *a)
{
    const char *src = c->tu->source;
    if (memcmp(b->scope, a->scope, 32) != 0)
        return fx_fail(c, "file-scope-changed",
                       "%s changed outside its function definitions", src);
    if (memcmp(b->heads, a->heads, 32) != 0)
        return fx_fail(c, "function-head-changed",
                       "%s: a definition's attributes, storage class or "
                       "signature changed", src);
    if (memcmp(b->rest, a->rest, 32) != 0)
        return fx_fail(c, "facts-changed-outside-seeds",
                       "%s: an included file, reference or unknown effect "
                       "outside the changed functions differs",
                       src);
    return true;
}

/* Rules 2-3a: complete facts from one named producer, and source bytes. */
static bool fx_evidence_ok(struct fx_ctx *c)
{
    const struct zcl_devloop_facts_tu *t = c->tu;
    uint8_t producer[2][32];
    if (!fx_manifest_ok(c, t->before, t->before_len, "before", producer[0]) ||
        !fx_manifest_ok(c, t->after, t->after_len, "after", producer[1]))
        return false;
    if (memcmp(producer[0], producer[1], 32) != 0)
        return fx_fail(c, "producer-changed",
                       "the manifests of %s come from different producers",
                       t->source);
    if (t->before_src == NULL || t->after_src == NULL)
        return fx_fail(c, "source-mismatch", "no source bytes for %s",
                       t->source);
    return true;
}

/* Rule 9: the after manifest is the compile of the tree under root. */
static bool fx_bind(struct fx_ctx *c)
{
    const char *reason = NULL;
    char detail[sizeof(c->v->detail)];
    if (zcl_devloop_facts_bind_after(c->root, c->tu, &reason, detail,
                                     sizeof(detail)))
        return true;
    return fx_fail(c, reason != NULL ? reason : "after-stale", "%s", detail);
}

static bool fx_check_tu(struct fx_ctx *c)
{
    const struct zcl_devloop_facts_tu *t = c->tu;
    struct fx_digests side[2];
    if (!fx_evidence_ok(c) || !fx_diff(c))
        return false;
    if (!vcs_semantic_section_v1_each(t->after, t->after_len,
                                      VCS_SEMANTIC_SECTION_V1_FUNCTIONS,
                                      fx_ids_cb, c))
        return fx_fail(c, "invalid-manifest", "functions of %s", t->source);
    return fx_seeds_resolved(c) &&
           fx_side(c, t->before, t->before_len, t->before_src,
                   t->before_src_len, &side[0]) &&
           fx_side(c, t->after, t->after_len, t->after_src, t->after_src_len,
                   &side[1]) &&
           fx_compare(c, &side[0], &side[1]) && fx_bind(c);
}

static bool fx_ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static const struct zcl_devloop_facts_tu *
fx_find_tu(const struct zcl_devloop_facts_tu *tus, size_t n, const char *f)
{
    for (size_t k = 0; k < n; k++)
        if (tus[k].source != NULL && strcmp(tus[k].source, f) == 0)
            return &tus[k];
    return NULL;
}

/* Rules 1-9 over every changed file; fills verdict->seeds. */
static bool fx_decide(const char *root, const char *const *files, size_t n,
                      const struct zcl_devloop_facts_tu *tus, size_t ntus,
                      struct zcl_devloop_facts_verdict *v)
{
    struct fx_ctx *c = zcl_calloc(1, sizeof(*c), "facts.ctx");
    bool ok = c != NULL;
    if (c == NULL) {
        v->reason = "out-of-memory";
        return false;
    }
    c->v = v;
    c->root = root;
    if (n == 0)
        ok = fx_fail(c, "no-changed-files", "%s", "");
    for (size_t k = 0; ok && k < n; k++) {
        c->tu = fx_find_tu(tus, ntus, files[k]);
        c->first_seed = v->seeds_len;
        c->added = c->removed = c->outside = c->overflow = false;
        if (fx_producer_source(files[k]))
            ok = fx_fail(c, "producer-source-changed", "%s", files[k]);
        else if (!fx_ends_with(files[k], ".c"))
            ok = fx_fail(c, "not-c-source", "%s", files[k]);
        else if (c->tu == NULL)
            ok = fx_fail(c, "no-manifest", "%s", files[k]);
        else
            ok = fx_check_tu(c);
    }
    free(c->ranges);
    free(c);
    return ok;
}

/* ---- the narrowed walk ---------------------------------------------------- */

/* The same bounds as the file-seeded walk (codeindex_impact.c): a caller
 * batch that fills, or the symbol cap, means the answer is not whole, and a
 * narrowing never keeps a partial answer. */
#define FX_BATCH 4096
#define FX_MAX_SYMS 50000

/* A set of fixed-width strings, insertion-ordered, open-addressed. */
struct fx_set {
    char *items;
    size_t width, len, cap;
    uint32_t *slots;
    size_t nslots;
};

static uint64_t fx_hash(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

static const char *fx_at(const struct fx_set *s, size_t k)
{
    return s->items + k * s->width;
}

static bool fx_rehash(struct fx_set *s)
{
    size_t n = s->nslots ? s->nslots * 2 : 1024;
    uint32_t *slots = zcl_calloc(n, sizeof(*slots), "facts.set");
    if (slots == NULL)
        return false;
    for (size_t k = 0; k < s->len; k++) {
        size_t j = (size_t)fx_hash(fx_at(s, k)) & (n - 1);
        while (slots[j])
            j = (j + 1) & (n - 1);
        slots[j] = (uint32_t)(k + 1);
    }
    free(s->slots);
    s->slots = slots;
    s->nslots = n;
    return true;
}

static bool fx_push(struct fx_set *s, const char *key)
{
    if (s->len == s->cap) {
        size_t next = s->cap ? s->cap * 2 : 256;
        char *grown = zcl_realloc(s->items, next * s->width, "facts.set");
        if (grown == NULL)
            return false;
        s->items = grown;
        s->cap = next;
    }
    (void)snprintf(s->items + s->len * s->width, s->width, "%s", key);
    s->len++;
    return true;
}

/* Adds `key` (empty keys and keys wider than the set are refused). */
static bool fx_add(struct fx_set *s, const char *key, bool *added)
{
    size_t j;
    *added = false;
    if (strlen(key) >= s->width)
        return false;
    if (s->len * 2 >= s->nslots && !fx_rehash(s))
        return false;
    j = (size_t)fx_hash(key) & (s->nslots - 1);
    for (; s->slots[j]; j = (j + 1) & (s->nslots - 1))
        if (strcmp(fx_at(s, s->slots[j] - 1), key) == 0)
            return true;
    if (!fx_push(s, key))
        return false;
    s->slots[j] = (uint32_t)s->len;
    *added = true;
    return true;
}

static void fx_set_free(struct fx_set *s)
{
    free(s->items);
    free(s->slots);
}

enum fx_walk_rc { FX_WALK_OK, FX_WALK_BOUNDED, FX_WALK_ERROR };

static enum fx_walk_rc fx_expand(struct codeindex *ci, const char *sym,
                                 struct fx_set *names, struct fx_set *files,
                                 struct ci_ref *buf)
{
    int nc = codeindex_callers(ci, sym, buf, FX_BATCH);
    bool added;
    if (nc < 0)
        return FX_WALK_ERROR;
    if (nc == FX_BATCH)
        return FX_WALK_BOUNDED;
    for (int i = 0; i < nc; i++) {
        if (buf[i].ref_file[0] && !fx_add(files, buf[i].ref_file, &added))
            return FX_WALK_ERROR;
        if (zcl_devloop_plan_proof_owner(buf[i].ref_file) ||
            !buf[i].enclosing[0])
            continue;
        if (names->len >= FX_MAX_SYMS)
            return FX_WALK_BOUNDED;
        if (!fx_add(names, buf[i].enclosing, &added))
            return FX_WALK_ERROR;
    }
    return FX_WALK_OK;
}

/* Callers of the seeds, then callers of callers, CI_CLOSURE_DEFAULT_DEPTH
 * levels deep, exactly as codeindex_impact_closure_bounded() expands a
 * file's symbols, with the planner's proof-owner files as terminals. */
static enum fx_walk_rc fx_walk(struct codeindex *ci, struct fx_set *names,
                               struct fx_set *files)
{
    struct ci_ref *buf = zcl_malloc(sizeof(*buf) * FX_BATCH, "facts.refs");
    enum fx_walk_rc rc = buf != NULL ? FX_WALK_OK : FX_WALK_ERROR;
    size_t lo = 0;
    for (int d = 0; rc == FX_WALK_OK && d < CI_CLOSURE_DEFAULT_DEPTH &&
                    lo < names->len;
         d++) {
        size_t hi = names->len;
        for (size_t k = lo; rc == FX_WALK_OK && k < hi; k++)
            rc = fx_expand(ci, fx_at(names, k), names, files, buf);
        lo = hi;
    }
    free(buf);
    return rc;
}

static int fx_path_cmp(const void *a, const void *b)
{
    return strcmp(a, b);
}

/* Replace the plan's closure with the narrowed file set. */
static bool fx_fold(struct zcl_devloop_plan *plan, struct fx_set *files)
{
    qsort(files->items, files->len, files->width, fx_path_cmp);
    plan->closure_attempted = true;
    plan->closure_snapshot = false;
    plan->closure_universal = false;
    plan->closure_groups_len = 0;
    for (size_t k = 0; k < files->len; k++)
        if (!zcl_devloop_plan_fold_file(plan, fx_at(files, k),
                                        ZCL_DEVLOOP_DIM_SEMANTIC))
            return false;
    /* Feedback only, never proof: see zcl_devloop_plan_proof_admissible(). */
    plan->dims[ZCL_DEVLOOP_DIM_SEMANTIC].status = ZCL_DEVLOOP_DIM_INCOMPLETE;
    plan->dims[ZCL_DEVLOOP_DIM_SEMANTIC].reason = "facts-narrowed";
    plan->dims[ZCL_DEVLOOP_DIM_INCLUDE].status =
        ZCL_DEVLOOP_DIM_NOT_APPLICABLE;
    plan->dims[ZCL_DEVLOOP_DIM_INCLUDE].reason = "";
    plan->closure_truncated = true;
    return true;
}

static bool fx_narrow(const char *root, const char *const *files, size_t n,
                      struct zcl_devloop_plan *plan,
                      struct zcl_devloop_facts_verdict *v)
{
    struct fx_set names = {.width = ZCL_DEVLOOP_FACTS_NAME_MAX};
    struct fx_set reached = {.width = 256};
    /* The file-seeded closure's own index: codeindex_open() rebuilds it when
     * the sources moved past it, or refuses (a live resident owns the
     * rebuild) and the plan falls back. Never a stale snapshot. */
    struct codeindex *ci = codeindex_open(root);
    enum fx_walk_rc rc = FX_WALK_ERROR;
    bool added, ok = true;
    if (ci == NULL) {
        v->reason = "no-code-index";
        return false;
    }
    for (size_t k = 0; ok && k < n; k++)
        ok = fx_add(&reached, files[k], &added);
    for (size_t k = 0; ok && k < v->seeds_len; k++)
        ok = fx_add(&names, v->seeds[k], &added);
    if (ok)
        rc = fx_walk(ci, &names, &reached);
    codeindex_close(ci);
    v->reason = rc == FX_WALK_OK      ? ""
                : rc == FX_WALK_BOUNDED ? "closure-bounded"
                                        : "closure-query-error";
    v->reached_files = reached.len;
    ok = rc == FX_WALK_OK && fx_fold(plan, &reached);
    if (rc == FX_WALK_OK && !ok)
        v->reason = "group-cap";
    fx_set_free(&names);
    fx_set_free(&reached);
    return ok;
}

bool zcl_devloop_plan_add_closure_facts(
    const char *repo_root, const char *const *files, size_t file_count,
    const struct zcl_devloop_facts_tu *tus, size_t tu_count,
    struct zcl_devloop_plan *plan, struct zcl_devloop_facts_verdict *verdict)
{
    const char *root = repo_root && repo_root[0] ? repo_root : ".";
    struct zcl_devloop_plan *saved;
    if (!plan || !verdict || (file_count > 0 && !files) ||
        (tu_count > 0 && !tus) || file_count > ZCL_DEVLOOP_MAX_FILES)
        return false;
    memset(verdict, 0, sizeof(*verdict));
    verdict->reason = "";
    saved = zcl_malloc(sizeof(*saved), "facts.plan");
    if (saved == NULL)
        return false;
    memcpy(saved, plan, sizeof(*saved));
    verdict->narrowed = fx_decide(root, files, file_count, tus, tu_count,
                                  verdict) &&
                        fx_narrow(root, files, file_count, plan, verdict);
    if (!verdict->narrowed) {
        /* The file-seeded closure, from the plan exactly as it was given. */
        memcpy(plan, saved, sizeof(*saved));
        verdict->seeds_len = 0;
        free(saved);
        return zcl_devloop_plan_add_closure(root, files, file_count, plan);
    }
    free(saved);
    return true;
}
