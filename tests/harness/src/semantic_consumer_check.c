/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Run the declaration-identity consumer on one consumer-fixture variant from before/after manifests and compare its universe and obligations with the edit table. */
#include "test/semantic_consumer_fixture.h"

#include "base/safe_alloc.h"

#include "devloop_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCX_FACTS "facts"

static bool scx_put_bytes(const char *root, const char *rel,
                          const char *suffix, const uint8_t *b, size_t n)
{
    char path[4096];
    FILE *fp;
    bool ok;
    if (snprintf(path, sizeof(path), "%s/" SCX_FACTS "/%s%s", root, rel,
                 suffix) >= (int)sizeof(path))
        return false;
    for (char *p = path + strlen(root) + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        (void)scx_mkdir(path);
        *p = '/';
    }
    fp = fopen(path, "wb");
    ok = fp != NULL && fwrite(b, 1, n, fp) == n;
    if (fp != NULL && fclose(fp) != 0)
        ok = false;
    return ok;
}

static bool scx_is_c(const char *path)
{
    size_t n = strlen(path);
    return n > 2 && strcmp(path + n - 2, ".c") == 0;
}

static size_t scx_nchanged(const struct scx_edit *e)
{
    return e->changed[1] != NULL ? 2 : e->changed[0] != NULL ? 1 : 0;
}

/* The facts directory: every TU's before and after manifest the evidence
 * has, and the base text of every changed file the base has. */
static bool scx_write_facts(const char *root, enum scx_variant v,
                            const struct scx_evidence *ev)
{
    const struct scx_edit *e = &k_scx_edits[v];
    bool ok = true;
    for (size_t k = 0; ok && k < SCX_TU_COUNT; k++) {
        if (ev->before[k] != NULL)
            ok = scx_put_bytes(root, k_scx_tus[k], ".before.zsm",
                               ev->before[k], ev->before_len[k]);
        if (ok && ev->after[k] != NULL)
            ok = scx_put_bytes(root, k_scx_tus[k], ".after.zsm",
                               ev->after[k], ev->after_len[k]);
    }
    for (size_t k = 0; ok && k < scx_nchanged(e); k++) {
        size_t n = 0;
        char *text = scx_text(SCX_BASE, e->changed[k], &n);
        if (text != NULL)
            ok = scx_put_bytes(root, e->changed[k], ".before",
                               (const uint8_t *)text, n);
        free(text);
    }
    return ok;
}

/* The .c path takes each changed file's pair and source texts itself. */
static bool scx_tus(enum scx_variant v, const struct scx_evidence *ev,
                    struct zcl_devloop_facts_tu *tus, char **texts)
{
    const struct scx_edit *e = &k_scx_edits[v];
    for (size_t k = 0; k < scx_nchanged(e); k++) {
        size_t bn = 0, an = 0, t = SCX_TU_COUNT;
        for (size_t i = 0; i < SCX_TU_COUNT; i++)
            if (strcmp(k_scx_tus[i], e->changed[k]) == 0)
                t = i;
        if (t == SCX_TU_COUNT || ev->before[t] == NULL || ev->after[t] == NULL)
            return false;
        texts[2 * k] = scx_text(SCX_BASE, e->changed[k], &bn);
        texts[2 * k + 1] = scx_text(v, e->changed[k], &an);
        tus[k] = (struct zcl_devloop_facts_tu){
            .source = e->changed[k], .before = ev->before[t],
            .after = ev->after[t], .before_len = ev->before_len[t],
            .after_len = ev->after_len[t],
            .before_src = (const uint8_t *)texts[2 * k],
            .after_src = (const uint8_t *)texts[2 * k + 1],
            .before_src_len = bn, .after_src_len = an};
        if (texts[2 * k] == NULL || texts[2 * k + 1] == NULL)
            return false;
    }
    return true;
}

bool scx_consume(const char *root, enum scx_variant v,
                 const struct scx_evidence *ev, struct scx_result *out)
{
    const struct scx_edit *e = &k_scx_edits[v];
    struct zcl_devloop_facts_tu tus[2] = {0};
    char *texts[4] = {0};
    bool all_c = scx_nchanged(e) > 0, ok;
    for (size_t k = 0; k < scx_nchanged(e); k++)
        all_c = all_c && scx_is_c(e->changed[k]);
    memset(out, 0, sizeof(*out));
    ok = scx_write_tree(root, v) &&
         (ev->no_depfiles || scx_write_depfiles(root, v)) &&
         scx_write_facts(root, v, ev) &&
         zcl_devloop_plan_files(e->changed, scx_nchanged(e), &out->plan) &&
         (!all_c || scx_tus(v, ev, tus, texts)) &&
         zcl_devloop_facts_consume(root, e->changed, scx_nchanged(e), SCX_FACTS,
                                   all_c ? tus : NULL, &out->plan,
                                   &out->verdict, &out->report);
    for (size_t k = 0; k < 4; k++)
        free(texts[k]);
    return ok;
}

const struct zcl_devloop_facts_tu_verdict *
scx_tu_of(const struct scx_result *r, const char *path)
{
    for (size_t k = 0; k < r->report.ntus; k++)
        if (strcmp(r->report.tus[k].path, path) == 0)
            return &r->report.tus[k];
    return NULL;
}

/* One TU against its row of the edit table: 0 when they agree. */
static size_t scx_compare_tu(const struct scx_edit *e, size_t k,
                             const struct scx_result *r, size_t *unsafe,
                             FILE *why)
{
    const struct zcl_devloop_facts_tu_verdict *t = scx_tu_of(r, k_scx_tus[k]);
    bool got_aff = t != NULL && t->affected;
    const char *got = t != NULL ? t->reason : NULL;
    bool same = e->reason[k] == NULL
                    ? t == NULL
                    : t != NULL && got_aff == e->affected[k] &&
                          strcmp(got, e->reason[k]) == 0;
    if (e->affected[k] && !got_aff)
        (*unsafe)++;
    if (same)
        return 0;
    if (why != NULL)
        fprintf(why, "  %s %s: want %s/%s, got %s/%s (%s)\n", e->name,
                k_scx_tus[k], e->affected[k] ? "affected" : "unaffected",
                e->reason[k] ? e->reason[k] : "absent",
                got_aff ? "affected" : "unaffected", got ? got : "absent",
                t != NULL ? t->detail : "");
    return 1;
}

static const char *scx_or_empty(const char *s)
{
    return s != NULL ? s : "";
}

/* The universe and the obligations against the edit table: 0 on agreement. */
static size_t scx_compare_whole(const struct scx_edit *e,
                                const struct scx_result *r, FILE *why)
{
    const char *want_ob = scx_or_empty(e->obligations);
    const char *got_ob = r->verdict.narrowed ? "" : scx_or_empty(r->verdict.reason);
    const char *want_u = scx_or_empty(e->incomplete);
    const char *got_u = r->report.complete ? "" : scx_or_empty(r->report.reason);
    if (strcmp(want_u, got_u) == 0 && strcmp(want_ob, got_ob) == 0)
        return 0;
    if (why != NULL)
        fprintf(why, "  %s obligations: want \"%s\", got \"%s\" (%s; universe %s %s)\n",
                e->name, want_ob, got_ob, r->verdict.detail,
                r->report.complete ? "complete" : "incomplete",
                scx_or_empty(r->report.reason));
    return 1;
}

/* One line per disagreement with the edit table on `why` (when not NULL);
 * *unsafe counts TUs the table says are affected that the consumer calls
 * unaffected or leaves out. Returns the number of disagreements. */
size_t scx_compare(enum scx_variant v, const struct scx_result *r,
                   size_t *unsafe, FILE *why)
{
    const struct scx_edit *e = &k_scx_edits[v];
    size_t bad = 0;
    *unsafe = 0;
    for (size_t k = 0; k < SCX_TU_COUNT; k++)
        bad += scx_compare_tu(e, k, r, unsafe, why);
    return bad + scx_compare_whole(e, r, why);
}

void scx_result_free(struct scx_result *r)
{
    zcl_devloop_facts_report_free(&r->report);
}
