/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Bound the functions of one TU whose code a compile may re-emit when some of them change, over the facts reference graph, under the optimizer the TU's compile identity names. */
#include "devloop_facts_index_priv.h"

#include "devloop.h"

#include "base/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Flags whose transforms the facts cannot bound. -O2 and above add clones
 * specialized by their callers (ipa-cp, ipa-sra, ipa-vrp, ipa-bit-cp),
 * caller-saved conventions (ipa-ra), merges of unrelated identical
 * functions (ipa-icf) and an inliner with unit-wide budgets; LTO,
 * whole-program and profile feedback decide from code or data no manifest
 * of this TU carries. Matched anywhere in the IDENTITY record, so a
 * definition or path that merely spells one also refuses. */
static const char *const k_fxg_unbounded[] = {
    "-O2",           "-O3",         "-O4",
    "-Os",           "-Oz",         "-Ofast",
    "-flto",         "-fwhole-program", "-fipa-cp",
    "-fipa-sra",     "-fipa-vrp",   "-fipa-bit-cp",
    "-fipa-ra",      "-fipa-icf",   "-fipa-pta",
    "-finline-small-functions",     "-finline-functions",
    "-fprofile-use", "-fauto-profile", "-fprofile-sample-use",
};

static const uint8_t *fxg_find(const uint8_t *s, size_t n, const char *tok,
                               size_t from)
{
    size_t k = strlen(tok);
    for (size_t i = from; k > 0 && i + k <= n; i++)
        if (memcmp(s + i, tok, k) == 0)
            return s + i;
    return NULL;
}

/* Every "-O" spelling but -O0 selects a level with interprocedural
 * propagation; a bare "-O" is -O1. */
static bool fxg_optimizes(const uint8_t *s, size_t n)
{
    for (const uint8_t *at = fxg_find(s, n, "-O", 0); at != NULL;
         at = fxg_find(s, n, "-O", (size_t)(at - s) + 2)) {
        size_t next = (size_t)(at - s) + 2;
        if (next >= n || s[next] != '0')
            return true;
    }
    return false;
}

enum fxi_codegen fxi_codegen_model_of(const uint8_t *identity, size_t len,
                                      const char **token)
{
    *token = "";
    if (identity == NULL) {
        *token = "no identity record";
        return FXI_CODEGEN_UNBOUNDED;
    }
    for (size_t k = 0; k < sizeof(k_fxg_unbounded) / sizeof(k_fxg_unbounded[0]);
         k++) {
        if (fxg_find(identity, len, k_fxg_unbounded[k], 0) != NULL) {
            *token = k_fxg_unbounded[k];
            return FXI_CODEGEN_UNBOUNDED;
        }
    }
    return fxg_optimizes(identity, len) ? FXI_CODEGEN_COMPONENT
                                        : FXI_CODEGEN_CALLERS;
}

bool fxi_object_cc_known(const struct fxi *x)
{
    static const char key[] = "; object-cc ", unknown[] = "unknown";
    const size_t kn = sizeof(key) - 1, un = sizeof(unknown) - 1;
    for (size_t k = 0; x->compiler != NULL && k + kn <= x->compiler_len; k++) {
        if (memcmp(x->compiler + k, key, kn) != 0)
            continue;
        k += kn;
        return !(x->compiler_len - k >= un &&
                 memcmp(x->compiler + k, unknown, un) == 0);
    }
    return false;
}

enum fxi_codegen fxi_codegen_model(const struct fxi *x, const char **token)
{
    if (!fxi_object_cc_known(x)) {
        *token = "no known object compiler";
        return FXI_CODEGEN_UNBOUNDED;
    }
    return fxi_codegen_model_of(x->identity, x->identity_len, token);
}

bool fxi_defined_function(const struct fxi *x, size_t e)
{
    return x->ents[e].defined_fn;
}

/* ---- the closure ------------------------------------------------------------ */

static bool fxg_internal(const struct fxi_ent *t)
{
    return t->id[1] == ':' && strchr(t->id + 2, ':') != NULL;
}

/* A function the TU defines, or a static variable it defines (in the main
 * file or a header: a header static is internal to every includer):
 * the code units whose bytes, or whose facts other code folds, can move. */
static bool fxg_node(const struct fxi *x, uint32_t e)
{
    const struct fxi_ent *t = &x->ents[e];
    return t->defined_fn ||
           (t->id[0] == 'v' && fxg_internal(t));
}

static bool fxg_step(enum fxi_codegen model, uint8_t kind)
{
    if (model == FXI_CODEGEN_CALLERS)
        return kind == VCS_SEMANTIC_REF_V1_CALL;
    return kind == VCS_SEMANTIC_REF_V1_CALL ||
           kind == VCS_SEMANTIC_REF_V1_ADDRESS ||
           kind == VCS_SEMANTIC_REF_V1_VARIABLE;
}

static void fxg_push(uint8_t *mark, uint32_t *q, size_t *tail, uint32_t e)
{
    if (mark[e])
        return;
    mark[e] = 2;
    q[(*tail)++] = e;
}

/* Up: every code unit that names e (a caller may inline it or fold its
 * summary). Down, beyond -O0: every internal unit e names (a callee only
 * this TU calls takes e's constants, coldness and dropped arguments; a
 * static variable takes e's stores and address uses to its readers). */
static void fxg_expand(const struct fxi *x, enum fxi_codegen model,
                       uint8_t *mark, uint32_t *q, size_t *tail, uint32_t e)
{
    for (uint32_t k = x->in_off[e]; k < x->in_off[e + 1]; k++) {
        const struct fxi_edge *g = &x->edges[x->in_edge[k]];
        if (fxg_step(model, g->kind) && fxg_node(x, g->from))
            fxg_push(mark, q, tail, g->from);
    }
    if (model != FXI_CODEGEN_COMPONENT)
        return;
    for (uint32_t k = x->out_off[e]; k < x->out_off[e + 1]; k++) {
        const struct fxi_edge *g = &x->edges[x->out_edge[k]];
        if (fxg_step(model, g->kind) && fxg_node(x, g->to) &&
            fxg_internal(&x->ents[g->to]))
            fxg_push(mark, q, tail, g->to);
    }
}

bool fxi_codegen_closure(const struct fxi *x, enum fxi_codegen model,
                         uint8_t *mark)
{
    uint32_t *q;
    size_t head = 0, tail = 0;
#if defined(ZCL_TESTING)
    if (zcl_devloop_test_consumer_mutant ==
        ZCL_DEVLOOP_MUTANT_NO_CODEGEN_CLOSURE)
        return true;
#endif
    q = zcl_calloc(x->nents + 1, sizeof(*q), "facts_codegen.queue");
    if (q == NULL)
        return false;
    for (size_t e = 0; e < x->nents; e++)
        if (mark[e])
            q[tail++] = (uint32_t)e;
    while (head < tail)
        fxg_expand(x, model, mark, q, &tail, q[head++]);
    free(q);
    return true;
}

/* ---- positions: builtins that expand to where or how often -------------- */

static bool fxg_ident_byte(unsigned char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

static bool fxg_body_names(const struct fxi_macro *m, const char *ident)
{
    size_t n = strlen(ident);
    for (size_t i = 0; i + n <= m->len; i++) {
        if (memcmp(m->body + i, ident, n) != 0)
            continue;
        if ((i == 0 || !fxg_ident_byte((unsigned char)m->body[i - 1])) &&
            (i + n == m->len || !fxg_ident_byte((unsigned char)m->body[i + n])))
            return true;
    }
    return false;
}

bool fxi_expands_builtin(const struct fxi *x, const char *ident, size_t *via)
{
    char id[64];
    uint32_t e;
    uint8_t *flags = zcl_calloc(x->nents + 1, 1, "facts_codegen.builtin");
    bool ok;
    if (flags == NULL)
        return false;
    int w = snprintf(id, sizeof(id), "m:@builtin:%s", ident);
    if (w > 0 && (size_t)w < sizeof(id) && fxi_lookup(x, id, (size_t)w, &e))
        flags[e] = 1;
    for (size_t k = 0; k < x->nmacros; k++)
        if (fxg_body_names(&x->macros[k], ident))
            flags[x->macros[k].ent] = flags[x->macros[k].group] = 1;
    ok = fxi_taint(x, flags, via);
    free(flags);
    return ok;
}
