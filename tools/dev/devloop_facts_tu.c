/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Decide one translation unit of the facts universe: its identities, whether a changed declaration, macro, layout or position reaches its code, and why. */
#include "devloop_facts_consumer.h"

#include "sha3/sha3.h"
#include "util/safe_alloc.h"
#include "vcs/build_action.h"
#include "vcs/semantic_manifest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One TU's manifest pair and its indexes. */
struct fxc_pair {
    const char *path;
    uint8_t *b, *a;
    size_t blen, alen;
    struct fxi *xb, *xa;
    uint8_t *fb, *fa; /* fxi_dirty bits per entity of each side */
};

static void fxc_pair_free(struct fxc_pair *p)
{
    fxi_free(p->xb);
    fxi_free(p->xa);
    free(p->b);
    free(p->a);
    free(p->fb);
    free(p->fa);
    memset(p, 0, sizeof(*p));
}

static bool fxc_set(struct zcl_devloop_facts_tu_verdict *t, bool affected,
                    bool broadened, const char *reason, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));
static bool fxc_set(struct zcl_devloop_facts_tu_verdict *t, bool affected,
                    bool broadened, const char *reason, const char *fmt, ...)
{
    va_list ap;
    t->affected = affected;
    t->broadened = broadened;
    t->reason = reason;
    va_start(ap, fmt);
    (void)vsnprintf(t->detail, sizeof(t->detail), fmt, ap);
    va_end(ap);
    return true;
}

static bool fxc_load_pair(struct fxc *c, const char *path, struct fxc_pair *p)
{
    const char *why;
    p->path = path;
    if (!zcl_devloop_facts_read(c->root, c->facts_dir, path, ".after.zsm",
                                VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &p->a,
                                &p->alen))
        return false;
    p->xa = fxi_open(p->a, p->alen, &why);
    if (zcl_devloop_facts_read(c->root, c->facts_dir, path, ".before.zsm",
                               VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &p->b,
                               &p->blen))
        p->xb = fxi_open(p->b, p->blen, &why);
    return p->xa != NULL;
}

/* Some side of the pair read a changed file. */
static bool fxc_member(const struct fxc *c, const struct fxc_pair *p)
{
    for (size_t k = 0; k < c->nfiles; k++)
        if (fxi_file_digest(p->xa, c->files[k]) != NULL ||
            (p->xb != NULL && fxi_file_digest(p->xb, c->files[k]) != NULL))
            return true;
    return false;
}

/* Every id a REFS ADDRESS record names, over every manifest here: a seed
 * among them is called through a pointer the name walk cannot follow. */
static bool fxc_collect_addresses(struct fxc *c, const struct fxi *x)
{
    for (size_t k = 0; x != NULL && k < fxi_edge_count(x); k++) {
        size_t from, to;
        uint8_t kind;
        fxi_edge_at(x, k, &from, &to, &kind);
        if (kind == VCS_SEMANTIC_REF_V1_ADDRESS &&
            !fxc_strs_has(&c->address, fxi_id(x, to)) &&
            !fxc_strs_add(&c->address, fxi_id(x, to)))
            return false;
    }
    return true;
}

/* ---- identities ------------------------------------------------------------------ */

static void fxc_identities(struct fxc *c, const struct fxc_pair *p,
                           struct zcl_devloop_facts_tu_verdict *t)
{
    struct fxi_roots r;
    uint8_t *bytes = NULL;
    size_t len = 0;
    char why[128];
    fxi_roots(p->xa, &r);
    memcpy(t->source, r.source, 32);
    memcpy(t->fact, r.fact, 32);
    memcpy(t->interface, r.interface, 32);
    memcpy(t->implementation, r.implementation, 32);
    t->has_roots = true;
    t->action_reason = "action-evidence-absent";
    if (zcl_devloop_facts_read(c->root, c->facts_dir, p->path, ".after.action",
                               1u << 20, &bytes, &len)) {
        t->has_action = vcs_action_root_v2_from_bytes(bytes, len, t->action,
                                                      why, sizeof(why));
        t->action_reason = t->has_action ? "" : "action-evidence-invalid";
        free(bytes);
    }
    if (zcl_devloop_facts_read(c->root, c->facts_dir, p->path, ".after.o",
                               256u << 20, &bytes, &len)) {
        struct sha3_256_ctx h;
        sha3_256_init(&h);
        sha3_256_write(&h, (const unsigned char *)"zcl.semantic.tu.artifact.v1",
                       strlen("zcl.semantic.tu.artifact.v1") + 1);
        sha3_256_write(&h, bytes, len);
        sha3_256_finalize(&h, t->artifact);
        t->has_artifact = true;
        free(bytes);
    }
}

/* ---- coarse evidence -------------------------------------------------------------- */

static bool fxc_same_section(const struct fxc_pair *p, int s)
{
    uint8_t b[32], a[32];
    fxi_section_digest(p->xb, s, b);
    fxi_section_digest(p->xa, s, a);
    return memcmp(b, a, 32) == 0;
}

static bool fxc_same_file_set(const struct fxc_pair *p)
{
    if (fxi_file_count(p->xb) != fxi_file_count(p->xa))
        return false;
    for (size_t k = 0; k < fxi_file_count(p->xa); k++)
        if (fxi_file_digest(p->xb, fxi_file_path(p->xa, k)) == NULL)
            return false;
    return true;
}

static const char *fxc_producer_check(struct fxc *c, const struct fxc_pair *p)
{
    static const uint8_t zero[32] = {0};
    if (memcmp(fxi_producer(p->xa), zero, 32) == 0 ||
        memcmp(fxi_producer(p->xb), zero, 32) == 0)
        return "producer-unknown";
    if (memcmp(fxi_producer(p->xa), fxi_producer(p->xb), 32) != 0)
        return "producer-mismatch";
    if (!c->have_producer) {
        memcpy(c->producer, fxi_producer(p->xa), 32);
        c->have_producer = true;
    } else if (memcmp(c->producer, fxi_producer(p->xa), 32) != 0) {
        c->mixed = true;
        return "producer-mismatch";
    }
    return NULL;
}

/* The first file this pair read whose bytes changed without being in the
 * request: the plan would then be about a different change. */
static const char *fxc_unrequested(const struct fxc *c, const struct fxc_pair *p)
{
    for (size_t k = 0; k < fxi_file_count(p->xa); k++) {
        const char *f = fxi_file_path(p->xa, k);
        const uint8_t *b = fxi_file_digest(p->xb, f);
        const uint8_t *a = fxi_file_digest(p->xa, f);
        if (b != NULL && memcmp(a, b, 32) != 0 && !fxc_is_changed(c, f))
            return f;
    }
    return NULL;
}

/* The evidence itself: complete, one named producer, one compile identity
 * and one include resolution on both sides. True when it broadened. */
static bool fxc_coarse_evidence(struct fxc *c, const struct fxc_pair *p,
                                struct zcl_devloop_facts_tu_verdict *t)
{
    const char *why;
    if (!fxi_complete(p->xa) || !fxi_complete(p->xb))
        return fxc_set(t, true, true, "truncated", "a producer cap cut a section");
    if ((why = fxc_producer_check(c, p)) != NULL)
        return fxc_set(t, true, true, why, "FACTS producer digests");
    if (!fxc_same_section(p, VCS_SEMANTIC_SECTION_V1_IDENTITY) ||
        strcmp(fxi_main(p->xa), fxi_main(p->xb)) != 0)
        return fxc_set(t, true, true, "identity-drift",
                       "compiler, target, flags or environment changed");
    if (!fxc_same_section(p, VCS_SEMANTIC_SECTION_V1_LOOKUPS) ||
        !fxc_same_section(p, VCS_SEMANTIC_SECTION_V1_PROBES) ||
        !fxc_same_file_set(p))
        return fxc_set(t, true, true, "include-resolution-change",
                       "an include resolved differently or the file set changed");
    return false;
}

/* Everything that broadens the whole TU; true when one did. */
static bool fxc_coarse(struct fxc *c, const struct fxc_pair *p,
                       struct zcl_devloop_facts_tu_verdict *t)
{
    const char *main = fxi_main(p->xa), *reason, *stale;
    char detail[192];
    struct zcl_devloop_facts_tu tu = {.source = p->path, .after = p->a,
                                      .after_len = p->alen};
    if (fxc_coarse_evidence(c, p, t))
        return true;
    if (memcmp(fxi_file_digest(p->xa, main), fxi_file_digest(p->xb, main), 32))
        return fxc_set(t, true, true, "source-changed", "%s", main);
    if ((fxi_revision(p->xa) < 2 || fxi_revision(p->xb) < 2) &&
        !fxc_same_section(p, VCS_SEMANTIC_SECTION_V1_MACROS))
        return fxc_set(t, true, true, "macro-unattributed",
                       "a revision-1 manifest cannot attribute a macro change");
    if ((stale = fxc_unrequested(c, p)) != NULL)
        return fxc_set(t, true, true, "unrequested-change", "%s", stale);
    if (!zcl_devloop_facts_bind_after(c->root, &tu, &reason, detail,
                                      sizeof(detail)))
        return fxc_set(t, true, true, reason ? reason : "after-stale", "%s",
                       detail);
    return false;
}

/* ---- the changed files' text ---------------------------------------------------- */

static bool fxc_digest_is(const uint8_t *bytes, size_t n, const uint8_t *want)
{
    uint8_t d[32];
    zcl_sha3_256(bytes, n, d);
    return want != NULL && memcmp(d, want, 32) == 0;
}

/* Load <facts>/<path>.before and the tree's <path> once; reason stays NULL
 * when both exist and diff. */
static struct fxc_hdr *fxc_hdr_load(struct fxc *c, const char *path)
{
    struct fxc_hdr *h = fxc_hdr_of(c, path);
    if (h == NULL || h->loaded)
        return h;
    h->loaded = true;
    h->path = path;
    if (!zcl_devloop_facts_read(c->root, c->facts_dir, path, ".before",
                                16u << 20, &h->before, &h->blen) ||
        !zcl_devloop_facts_read(c->root, NULL, path, "", 16u << 20, &h->after,
                                &h->alen)) {
        h->reason = "position-unknown";
        return h;
    }
    zcl_sha3_256(h->before, h->blen, h->bdigest);
    zcl_sha3_256(h->after, h->alen, h->adigest);
    if (!fxh_diff(h->before, h->blen, h->after, h->alen, &h->diff))
        h->reason = "out-of-memory";
    return h;
}

static bool fxc_name_in(const char *bare, char **names, size_t n)
{
    for (size_t k = 0; k < n; k++)
        if (strcmp(bare, names[k]) == 0)
            return true;
    return false;
}

/* Mark every entity of x that `path` declares and whose name is in names. */
static size_t fxc_mark_names(const struct fxi *x, uint8_t *flags,
                             const char *path, char **names, size_t n)
{
    size_t hits = 0;
    for (size_t e = 0; e < fxi_count(x); e++) {
        if (!fxc_name_in(fxi_bare(x, e), names, n) || !fxi_has_path(x, e, path))
            continue;
        flags[e] |= FXI_DIRTY_CHUNK;
        hits++;
    }
    return hits;
}

/* Attribute the header's changed chunks to ids, or name the broadening. */
static const char *fxc_chunks(const struct fxc_hdr *h, struct fxc_pair *p,
                              char *detail, size_t cap)
{
    for (size_t k = 0; k < h->diff.nchanged; k++) {
        const struct fxh_chunk *ch = &h->diff.changed[k];
        size_t hits;
        if (ch->kind == FXH_COND || ch->kind == FXH_INCLUDE ||
            ch->kind == FXH_OTHER) {
            (void)snprintf(detail, cap, "a %s directive of %s changed",
                           ch->kind == FXH_COND      ? "conditional"
                           : ch->kind == FXH_INCLUDE ? "include"
                                                     : "preprocessor",
                           h->path);
            return ch->kind == FXH_COND      ? "macro-conditional"
                   : ch->kind == FXH_INCLUDE ? "include-resolution-change"
                                             : "header-unattributed";
        }
        hits = fxc_mark_names(p->xa, p->fa, h->path, ch->names, ch->nnames) +
               fxc_mark_names(p->xb, p->fb, h->path, ch->names, ch->nnames);
        if (hits == 0 && ch->kind != FXH_DEFINE) {
            (void)snprintf(detail, cap, "a changed text of %s names no id it "
                           "declares (%s)", h->path,
                           ch->nnames > 0 ? ch->names[0] : "no name");
            return "header-unattributed";
        }
    }
    return NULL;
}

/* -g1 debug information records the declaration line and column of every
 * function and external variable a TU defines or references, and the
 * lines of every header function it emits. */
static void fxc_mark_positions(const struct fxc_hdr *h, const struct fxi *x,
                               uint8_t *flags, bool after)
{
#if defined(ZCL_TESTING)
    if (zcl_devloop_test_consumer_mutant == ZCL_DEVLOOP_MUTANT_NO_POSITION)
        return;
#endif
    for (size_t e = 0; e < fxi_count(x); e++) {
        const char *id = fxi_id(x, e);
        uint32_t lo, hi;
        if (fxi_span(x, e, h->path, &lo, &hi) &&
            fxh_span_dirty(&h->diff, lo, hi, after)) {
            /* Its code, not only its debug lines: __LINE__ in it moves. */
            flags[e] |= FXI_DIRTY_POSITION | FXI_DIRTY_SPAN;
            continue;
        }
        if ((id[0] == 'f' || id[0] == 'v') && id[1] == ':' &&
            fxi_has_path(x, e, h->path) &&
            fxh_position_dirty(&h->diff, fxi_bare(x, e)))
            flags[e] |= FXI_DIRTY_POSITION;
    }
}

/* One changed file this TU read: its text evidence, chunks and positions. */
static const char *fxc_changed_file(struct fxc *c, struct fxc_pair *p,
                                    const char *path, char *detail, size_t cap)
{
    struct fxc_hdr *h = fxc_hdr_load(c, path);
    const char *why;
    if (h == NULL)
        return NULL;
    if (h->reason != NULL ||
        !fxc_digest_is(h->before, h->blen, fxi_file_digest(p->xb, path)) ||
        !fxc_digest_is(h->after, h->alen, fxi_file_digest(p->xa, path))) {
        (void)snprintf(detail, cap, "no before text of %s that binds the "
                       "manifests", path);
        return "position-unknown";
    }
    why = fxc_chunks(h, p, detail, cap);
    if (why != NULL)
        return why;
    fxc_mark_positions(h, p->xb, p->fb, false);
    fxc_mark_positions(h, p->xa, p->fa, true);
    return NULL;
}

/* ---- dirty ids and their reach ------------------------------------------------ */

static void fxc_mark_digests(const struct fxi *x, const struct fxi *other,
                             uint8_t *flags)
{
    for (size_t e = 0; e < fxi_count(x); e++) {
        size_t o;
        if (!fxi_find(other, fxi_id(x, e), &o) ||
            memcmp(fxi_digest(x, e), fxi_digest(other, o), 32) != 0)
            flags[e] |= FXI_DIRTY_DIGEST;
    }
}

/* New or removed external ids: the same-name rule's input. */
static bool fxc_new_ids(struct fxc *c, const struct fxi *x,
                        const struct fxi *other)
{
    for (size_t e = 0; e < fxi_count(x); e++) {
        size_t o;
        if (!fxi_external(x, e) || fxi_find(other, fxi_id(x, e), &o) ||
            fxc_strs_has(&c->new_ids, fxi_id(x, e)))
            continue;
        if (!fxc_strs_add(&c->new_ids, fxi_id(x, e)) ||
            (!fxc_strs_has(&c->new_names, fxi_bare(x, e)) &&
             !fxc_strs_add(&c->new_names, fxi_bare(x, e))))
            return false;
    }
    return true;
}

/* The first root of x that reaches an entity with `mask` bits; SIZE_MAX
 * when none does. *dirty names the entity it reaches. */
static size_t fxc_first_root(const struct fxi *x, const uint8_t *flags,
                             uint8_t mask, size_t *via, size_t *dirty)
{
    uint8_t *m = zcl_calloc(fxi_count(x) + 1, 1, "facts_tu.mask");
    size_t hit = SIZE_MAX;
    if (m == NULL)
        return SIZE_MAX - 1;
    for (size_t e = 0; e < fxi_count(x); e++)
        m[e] = flags[e] & mask;
    if (!fxi_taint(x, m, via))
        hit = SIZE_MAX - 1;
    for (size_t e = 0; hit == SIZE_MAX && e < fxi_count(x); e++)
        if (fxi_root(x, e) && via[e] != SIZE_MAX) {
            hit = e;
            *dirty = via[e];
        }
    free(m);
    return hit;
}

/* Seeds: every function the compile may re-emit. The functions the TU
 * defines that reach a semantic change, grown by fxi_codegen_closure under
 * the optimizer the identity names (unbounded: "inline-closure-unknown");
 * main-file functions among them seed the walk. Any other root that
 * reaches a change, or a header definition every reader emits among them,
 * seeds the whole file instead. */
static bool fxc_seeds(struct fxc *c, const struct fxi *x, const size_t *via,
                      struct zcl_devloop_facts_tu_verdict *t)
{
    uint8_t *mark = zcl_calloc(fxi_count(x) + 1, 1, "facts_tu.codegen");
    const char *token;
    enum fxi_codegen model = fxi_codegen_model(x, &token);
    bool ok = mark != NULL;
    for (size_t e = 0; ok && e < fxi_count(x); e++) {
        if (via[e] == SIZE_MAX)
            continue;
        if (fxi_defined_function(x, e))
            mark[e] = 1;
        else if (fxi_root(x, e))
            t->broadened = true;
    }
    if (ok && model == FXI_CODEGEN_UNBOUNDED)
        fxc_refuse(c, "inline-closure-unknown",
                   "the compile identity names", token);
    ok = ok && fxi_codegen_closure(x, model, mark);
    for (size_t e = 0; ok && e < fxi_count(x); e++) {
        if (!mark[e] || !fxi_defined_function(x, e))
            continue;
        if (fxi_main_function(x, e))
            ok = fxc_seed_add(c, x, e);
        else if (fxi_root(x, e))
            t->broadened = true;
    }
    free(mark);
    return ok;
}

static const char *fxc_dirty_reason(const struct fxi *x, size_t root,
                                    const uint8_t *flags, size_t dirty)
{
    if (flags[dirty] & FXI_DIRTY_DIGEST)
        return fxi_is_site(x, root, VCS_SEMANTIC_FACTS_COND_SITE)
                   ? "macro-conditional"
                   : "interface";
    if (flags[dirty] & FXI_DIRTY_CHUNK)
        return "header-text";
    return (flags[dirty] & FXI_DIRTY_SPAN) ? "code-moved" : "position";
}

/* Semantic reach on either side; true when the TU is affected by it. */
static bool fxc_semantic(struct fxc *c, struct fxc_pair *p,
                         struct zcl_devloop_facts_tu_verdict *t, bool *ok)
{
    const uint8_t sem = FXI_DIRTY_DIGEST | FXI_DIRTY_CHUNK | FXI_DIRTY_SPAN;
    struct fxi *side[2] = {p->xa, p->xb};
    uint8_t *flags[2] = {p->fa, p->fb};
    bool affected = false;
    for (int s = 0; *ok && s < 2; s++) {
        size_t n = fxi_count(side[s]), dirty = 0, root;
        size_t *via = zcl_calloc(n + 1, sizeof(*via), "facts_tu.via");
        root = via != NULL ? fxc_first_root(side[s], flags[s], sem, via, &dirty)
                           : SIZE_MAX - 1;
        *ok = root != SIZE_MAX - 1;
        if (*ok && root != SIZE_MAX && !affected) {
            affected = true;
            fxc_set(t, true, false,
                    fxc_dirty_reason(side[s], root, flags[s], dirty),
                    "%s reaches %s", fxi_id(side[s], root),
                    fxi_id(side[s], dirty));
        }
        if (*ok && root != SIZE_MAX)
            *ok = fxc_seeds(c, side[s], via, t);
        free(via);
    }
    return affected;
}

static bool fxc_position(struct fxc_pair *p,
                         struct zcl_devloop_facts_tu_verdict *t, bool *ok)
{
    struct fxi *side[2] = {p->xa, p->xb};
    uint8_t *flags[2] = {p->fa, p->fb};
    for (int s = 0; *ok && s < 2; s++) {
        size_t n = fxi_count(side[s]), dirty = 0, root;
        size_t *via = zcl_calloc(n + 1, sizeof(*via), "facts_tu.pvia");
        root = via != NULL ? fxc_first_root(side[s], flags[s],
                                            FXI_DIRTY_POSITION, via, &dirty)
                           : SIZE_MAX - 1;
        free(via);
        *ok = root != SIZE_MAX - 1;
        if (*ok && root != SIZE_MAX)
            return fxc_set(t, true, false, "position",
                           "%s reaches %s, whose declaration moved",
                           fxi_id(side[s], root), fxi_id(side[s], dirty));
    }
    return false;
}

/* The roots differ although no reached id is dirty: never narrowed. */
static bool fxc_roots_differ(const struct fxc_pair *p,
                            struct zcl_devloop_facts_tu_verdict *t)
{
    struct fxi_roots rb, ra;
    fxi_roots(p->xb, &rb);
    fxi_roots(p->xa, &ra);
    if (memcmp(rb.interface, ra.interface, 32) != 0)
        return fxc_set(t, true, true, "interface-changed",
                       "the reached interface differs");
    if (memcmp(rb.implementation, ra.implementation, 32) != 0)
        return fxc_set(t, true, true, "implementation-changed",
                       "a main-file record differs");
    return false;
}

static bool fxc_fine(struct fxc *c, struct fxc_pair *p,
                     struct zcl_devloop_facts_tu_verdict *t)
{
    char detail[192];
    bool ok = true;
    p->fa = zcl_calloc(fxi_count(p->xa) + 1, 1, "facts_tu.fa");
    p->fb = zcl_calloc(fxi_count(p->xb) + 1, 1, "facts_tu.fb");
    if (p->fa == NULL || p->fb == NULL)
        return false;
    fxc_mark_digests(p->xa, p->xb, p->fa);
    fxc_mark_digests(p->xb, p->xa, p->fb);
    for (size_t k = 0; k < c->nfiles; k++) {
        const uint8_t *b = fxi_file_digest(p->xb, c->files[k]);
        const uint8_t *a = fxi_file_digest(p->xa, c->files[k]);
        const char *why;
        if (a == NULL || b == NULL || memcmp(a, b, 32) == 0)
            continue;
        why = fxc_changed_file(c, p, c->files[k], detail, sizeof(detail));
        if (why != NULL)
            return fxc_set(t, true, true, why, "%s", detail);
    }
    if (!fxc_new_ids(c, p->xa, p->xb) || !fxc_new_ids(c, p->xb, p->xa))
        return false;
    if (fxc_semantic(c, p, t, &ok) || !ok)
        return ok;
    if (fxc_position(p, t, &ok) || !ok)
        return ok;
    if (fxc_roots_differ(p, t))
        return true;
    return fxc_set(t, false, false, "unaffected",
                   "no changed id reaches its code");
}

bool fxc_tu_eval(struct fxc *c, const char *path)
{
    struct fxc_pair p = {0};
    struct zcl_devloop_facts_tu_verdict *t;
    bool ok = true;
    if (!fxc_load_pair(c, path, &p)) {
        fxc_pair_free(&p);
        return true; /* unreadable: the depfile cross-check names it */
    }
    if (!fxc_collect_addresses(c, p.xa) || !fxc_collect_addresses(c, p.xb)) {
        fxc_pair_free(&p);
        return false;
    }
    if (!fxc_member(c, &p)) {
        fxc_pair_free(&p);
        return true;
    }
    t = fxc_tu_new(c, path);
    ok = t != NULL;
    if (ok)
        fxc_identities(c, &p, t);
    if (ok && p.xb == NULL)
        fxc_set(t, true, true, "facts-missing", "no valid before manifest");
    else if (ok && !fxc_coarse(c, &p, t))
        ok = fxc_fine(c, &p, t);
    fxc_pair_free(&p);
    return ok;
}

/* ---- pass 2: the same-name rule --------------------------------------------------- */

/* A TU pass 1 left unaffected that has an id with the name of a new or
 * removed external id (a same-name static, macro or variable): its name
 * lookup may now resolve elsewhere, so it is affected. */
static bool fxc_collides(const struct fxc *c, const struct fxi *x,
                         const char **hit)
{
    for (size_t e = 0; x != NULL && e < fxi_count(x); e++) {
        if (fxc_strs_has(&c->new_names, fxi_bare(x, e)) &&
            !fxc_strs_has(&c->new_ids, fxi_id(x, e))) {
            *hit = fxi_id(x, e);
            return true;
        }
    }
    return false;
}

bool fxc_name_collisions(struct fxc *c)
{
    for (size_t k = 0; c->new_names.n > 0 && k < c->report->ntus; k++) {
        struct zcl_devloop_facts_tu_verdict *t = &c->report->tus[k];
        struct fxc_pair p = {0};
        const char *hit = NULL;
        if (t->affected || !fxc_load_pair(c, t->path, &p)) {
            fxc_pair_free(&p);
            continue;
        }
        if (fxc_collides(c, p.xa, &hit) || fxc_collides(c, p.xb, &hit))
            fxc_set(t, true, true, "name-collision",
                    "%s shares its name with a new or removed external id",
                    hit);
        fxc_pair_free(&p);
    }
    return true;
}
