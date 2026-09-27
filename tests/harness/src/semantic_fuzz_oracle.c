/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts fuzz oracle: plan one change set in process from its facts directory, then hold the plan to the objects (an unaffected TU's object is byte-identical; a function with new bytes is a seed or covered). */
#include "semantic_fuzz_case_priv.h"

#include "base/safe_alloc.h"
#include "platform/clock.h"
#include "vcs/semantic_manifest.h"

#include "devloop_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The dev.change.plan document reads each changed .c's pair and its two
 * source texts itself; so does this. */
#define SFZ_SOURCE_MAX (16u * 1024u * 1024u)

struct sfz_plan {
    struct zcl_devloop_plan plan;
    struct zcl_devloop_facts_verdict v;
    struct zcl_devloop_facts_report report;
    struct zcl_devloop_facts_tu tus[64];
};

static bool load_pair(const char *root, const char *file,
                      struct zcl_devloop_facts_tu *tu)
{
    uint8_t *b = NULL, *a = NULL, *bs = NULL, *as = NULL;
    bool ok = zcl_devloop_facts_read(root, "facts", file, ".before.zsm",
                                     VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &b,
                                     &tu->before_len) &&
              zcl_devloop_facts_read(root, "facts", file, ".after.zsm",
                                     VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &a,
                                     &tu->after_len) &&
              zcl_devloop_facts_read(root, "facts", file, ".before",
                                     SFZ_SOURCE_MAX, &bs, &tu->before_src_len) &&
              zcl_devloop_facts_read(root, NULL, file, "", SFZ_SOURCE_MAX, &as,
                                     &tu->after_src_len);
    if (!ok) {
        /* a partial pair is dropped whole: the caller passes the file
         * without its evidence */
        free(b);
        free(a);
        free(bs);
        free(as);
        memset(tu, 0, sizeof(*tu));
        return false;
    }
    tu->source = file;
    tu->before = b;
    tu->after = a;
    tu->before_src = bs;
    tu->after_src = as;
    return true;
}

static void unload_pairs(struct sfz_plan *p, size_t n)
{
    for (size_t k = 0; k < n; k++) {
        free((void *)p->tus[k].before);
        free((void *)p->tus[k].after);
        free((void *)p->tus[k].before_src);
        free((void *)p->tus[k].after_src);
    }
}

/* Plan r->changed as dev.change.plan with {"facts":"facts"} does: a change
 * set of .c files passes their pairs (a file without its full evidence is
 * passed without it and falls back as "no-manifest"), any other goes to the
 * declaration-identity consumer. */
static bool plan(struct sfz_run *r, struct sfz_plan *p)
{
    const char *const *files = (const char *const *)r->changed.v;
    size_t n = r->changed.n;
    bool all_c = !r->header_changed && n <= sizeof(p->tus) / sizeof(p->tus[0]);
    bool ok;
    int64_t t0 = clock_now_monotonic_ns();
    if (!zcl_devloop_plan_files(files, n, &p->plan))
        return false;
    for (size_t k = 0; all_c && k < n; k++)
        (void)load_pair(r->tree, files[k], &p->tus[k]);
    ok = zcl_devloop_facts_consume(r->tree, files, n, "facts",
                                   all_c ? p->tus : NULL, &p->plan, &p->v,
                                   &p->report);
    if (all_c)
        unload_pairs(p, n);
    r->out->plan_s = (double)(clock_now_monotonic_ns() - t0) / 1e9;
    return ok;
}

static const struct zcl_devloop_facts_tu_verdict *
tu_of(const struct sfz_plan *p, const char *path)
{
    for (size_t k = 0; k < p->report.ntus; k++)
        if (strcmp(p->report.tus[k].path, path) == 0)
            return &p->report.tus[k];
    return NULL;
}

/* A seed by its bare name: "t0_s1.constprop.0" is t0_s1's code. */
static bool is_seed(const struct sfz_plan *p, const char *name, bool bare)
{
    size_t len = bare ? strcspn(name, ".") : strlen(name);
    for (size_t k = 0; k < p->v.seeds_len; k++)
        if (strlen(p->v.seeds[k]) == len && strncmp(p->v.seeds[k], name, len) == 0)
            return true;
    return false;
}

/* One TU with a changed object: how its plan covers it. */
struct tu_judge {
    const char *tu;
    bool predicted, broadened;
    struct sfz_funcs before, after;
};

static bool func_changed(const struct tu_judge *j, const char *name, size_t k)
{
    const struct sfz_func *a = sfz_func_find(&j->before, name, k);
    const struct sfz_func *b = sfz_func_find(&j->after, name, k);
    return a == NULL || b == NULL || memcmp(a->full, b->full, 32) != 0;
}

static bool reloc_only(const struct tu_judge *j, const char *name, size_t k)
{
    const struct sfz_func *a = sfz_func_find(&j->before, name, k);
    const struct sfz_func *b = sfz_func_find(&j->after, name, k);
    return a != NULL && b != NULL && memcmp(a->noadd, b->noadd, 32) == 0;
}

/* Every other function at f's address in the after object runs f's new
 * bytes: each must be a seed too. */
static void judge_aliases(struct sfz_run *r, const struct sfz_plan *p,
                          const struct tu_judge *j, const struct sfz_func *f)
{
    for (size_t q = 0; f != NULL && q < j->after.n; q++) {
        const struct sfz_func *o = &j->after.v[q];
        if (o == f || o->shndx != f->shndx || o->value != f->value ||
            strcmp(o->name, f->name) == 0 || is_seed(p, o->name, false))
            continue;
        r->out->notcov++;
        sfz_why(r->out, "  %s %s alias-of-%s ALIAS-NOT-COVERED\n", j->tu,
                o->name, f->name);
    }
}

static void judge_func(struct sfz_run *r, const struct sfz_plan *p,
                       const struct tu_judge *j, const char *name, size_t k)
{
    struct sfz_outcome *o = r->out;
    o->cfun++;
    if (!p->v.narrowed) {
        o->covered_other++; /* covered-fallback: the file-seeded plan */
        return;
    }
    if (!j->predicted) {
        o->notcov++;
        sfz_why(o, "  %s %s tu-missed\n", j->tu, name);
        return;
    }
    if (r->header_changed && j->broadened) {
        o->covered_other++; /* covered-tu-broadened: its whole file */
        return;
    }
    if (is_seed(p, name, true))
        o->covered_seed++;
    else if (reloc_only(j, name, k)) {
        o->reloc++;
        return;
    } else {
        o->notcov++;
        sfz_why(o, "  %s %s NOT-COVERED%s\n", j->tu, name,
                p->v.seeds_total > p->v.seeds_len ? " (seed list truncated)" : "");
    }
    judge_aliases(r, p, j, sfz_func_find(&j->after, name, k));
}

/* The k-th occurrence index of v[i]'s name among v[0..i). */
static size_t ordinal(const struct sfz_funcs *f, size_t i)
{
    size_t k = 0;
    for (size_t q = 0; q < i; q++)
        k += strcmp(f->v[q].name, f->v[i].name) == 0;
    return k;
}

static void judge_side(struct sfz_run *r, const struct sfz_plan *p,
                       const struct tu_judge *j, const struct sfz_funcs *side,
                       bool before)
{
    for (size_t i = 0; i < side->n; i++) {
        size_t k = ordinal(side, i);
        /* a function on both sides is judged once, from the after side */
        if (before && sfz_func_find(&j->after, side->v[i].name, k) != NULL)
            continue;
        if (func_changed(j, side->v[i].name, k))
            judge_func(r, p, j, side->v[i].name, k);
    }
}

static bool read_funcs(struct sfz_run *r, const char *dir, const char *tu,
                       struct sfz_funcs *out)
{
    char path[PATH_MAX], err[128] = "";
    uint8_t *img = NULL;
    size_t n = 0;
    bool ok;
    sfz_object(path, sizeof(path), dir, tu);
    ok = sfz_slurp(path, &img, &n) && sfz_elf_funcs(img, n, out, err, sizeof(err));
    if (!ok)
        sfz_why(r->out, "cannot read the functions of %s: %s\n", path, err);
    free(img);
    return ok;
}

static bool judge_funcs(struct sfz_run *r, const struct sfz_plan *p,
                        struct tu_judge *j)
{
    bool ok = read_funcs(r, r->ob, j->tu, &j->before) &&
              read_funcs(r, r->oa, j->tu, &j->after);
    if (ok) {
        judge_side(r, p, j, &j->after, false);
        judge_side(r, p, j, &j->before, true);
    }
    sfz_funcs_free(&j->before);
    sfz_funcs_free(&j->after);
    return ok;
}

static bool same_object(const struct sfz_run *r, const char *tu)
{
    char a[PATH_MAX], b[PATH_MAX];
    uint8_t *x = NULL, *y = NULL;
    size_t xn = 0, yn = 0;
    bool same;
    sfz_object(a, sizeof(a), r->ob, tu);
    sfz_object(b, sizeof(b), r->oa, tu);
    same = sfz_slurp(a, &x, &xn) && sfz_slurp(b, &y, &yn) && xn == yn &&
           memcmp(x, y, xn) == 0;
    free(x);
    free(y);
    return same;
}

static bool judge_tu(struct sfz_run *r, const struct sfz_plan *p, const char *tu)
{
    const struct zcl_devloop_facts_tu_verdict *t = tu_of(p, tu);
    struct tu_judge j = {.tu = tu, .predicted = t != NULL && t->affected,
                         .broadened = t != NULL && t->broadened};
    struct sfz_outcome *o = r->out;
    bool same = same_object(r, tu);
    o->tus++;
    o->predicted += j.predicted;
    o->changed += !same;
    o->over += same && j.predicted;
    if (!same && !j.predicted) {
        o->missed++;
        sfz_why(o, "  %s object changed, planned unaffected (%s)\n", tu,
                t != NULL ? t->reason : "not in the universe");
    }
    return same || judge_funcs(r, p, &j);
}

bool sfz_plan_and_judge(struct sfz_run *r)
{
    struct sfz_plan *p = zcl_calloc(1, sizeof(*p), "sfz.plan");
    struct sfz_outcome *o = r->out;
    bool ok = p != NULL && plan(r, p);
    if (ok) {
        o->narrowed = p->v.narrowed;
        (void)snprintf(o->vreason, sizeof(o->vreason), "%s",
                       p->v.narrowed ? "" : p->v.reason);
        o->seeds = p->v.seeds_total;
    }
    for (size_t k = 0; ok && k < r->tus.n; k++)
        ok = judge_tu(r, p, r->tus.v[k]);
    if (ok)
        o->status = o->missed == 0 && o->notcov == 0 ? SFZ_PASS : SFZ_FAIL;
    if (p != NULL)
        zcl_devloop_facts_report_free(&p->report);
    free(p);
    return ok;
}
