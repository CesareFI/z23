/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts differential fuzz generator: from a seed, a random multi-TU C23 project before and after one mutation, deterministic from the seed and profile. */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "test/semantic_fuzz.h"

#include "semantic_fuzz_gen_priv.h"

#include "base/log_macros.h"
#include "base/safe_alloc.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- rng (splitmix64) -------------------------------------------------------- */

struct sfz_gen {
    uint64_t rng;
    bool noctr, noline, noflag;
};

static uint64_t rnd(struct sfz_gen *g)
{
    uint64_t z = (g->rng += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static int ri(struct sfz_gen *g, int lo, int hi)
{
    return lo + (int)(rnd(g) % (uint64_t)(hi - lo + 1));
}

static bool rp(struct sfz_gen *g, int pct)
{
    return ri(g, 0, 99) < pct;
}

/* ---- text buffer --------------------------------------------------------------- */

void sfz_bp(struct sfz_buf *b, const char *fmt, ...)
{
    va_list ap;
    while (!b->bad) {
        va_start(ap, fmt);
        int w = vsnprintf(b->p ? b->p + b->n : NULL, b->p ? b->cap - b->n : 0,
                          fmt, ap);
        va_end(ap);
        if (w < 0) {
            b->bad = true;
            return;
        }
        if (b->p && (size_t)w < b->cap - b->n) {
            b->n += (size_t)w;
            return;
        }
        size_t cap = b->cap * 2 + (size_t)w + 4096;
        char *p = zcl_realloc(b->p, cap, "sfz.text");
        if (p == NULL) {
            b->bad = true;
            return;
        }
        b->p = p;
        b->cap = cap;
    }
}

/* ---- model ----------------------------------------------------------------------- */

/* the first included header of TU t, or -1 */
static int first_inc(const struct sfz_tu *t, int nh)
{
    for (int k = 0; k < nh; k++)
        if (t->inc[k])
            return k;
    return -1;
}

/* a random included header of t */
static int pick_inc(struct sfz_gen *g, const struct sfz_tu *t, int nh)
{
    int ks[SFZ_MAXH], n = 0;
    for (int k = 0; k < nh; k++)
        if (t->inc[k])
            ks[n++] = k;
    return n ? ks[ri(g, 0, n - 1)] : -1;
}

static void gen_header(struct sfz_gen *g, struct sfz_model *m, int k)
{
    struct sfz_hdr *h = &m->h[k];
    h->dir = rp(g, 25) ? 1 : 2;
    h->inc_prev = k > 0 && rp(g, 40);
    h->A = ri(g, 1, 6);
    h->Bplus = ri(g, 1, 5);
    h->Fmul = ri(g, 2, 5);
    h->Fadd = ri(g, 0, 9);
    h->C_opt = ri(g, 10, 19);
    h->C_else = ri(g, 20, 29);
    h->D_thr = ri(g, 1, 6);
    h->opt_defined = rp(g, 30);
    h->i0type = ri(g, 0, 3);
    h->in_type = ri(g, 0, 2) == 0 ? 2 : 0;
    h->has_union = rp(g, 70);
    h->has_anon = rp(g, 60);
    h->E0 = ri(g, 0, 4);
    h->E2 = ri(g, 8, 15);
    h->fn2_param = ri(g, 0, 1);
    h->inl_add = ri(g, 0, 9);
    h->inl_ctr = ri(g, 0, 1);
    h->inl2_add = ri(g, 0, 9);
    h->sa_min = ri(g, 1, 4);
    h->ctr_add = ri(g, 0, 5);
    h->owner = ri(g, 0, m->nt - 1);
    h->var_init = ri(g, 0, 99);
    h->cvar_init = ri(g, 0, 99);
    h->weak_fn = rp(g, 15);
    h->X_thr = ri(g, 1, 6);
    h->tab1 = ri(g, 0, 99);
    h->ctr_macro_n = 1;
    h->opt_file = rp(g, 30);
    h->opt_inc = rp(g, 50);
    h->K = ri(g, 2, 9);
    if (g->noctr)
        h->inl_ctr = 0;
}

static void gen_tu(struct sfz_gen *g, struct sfz_model *m, int i)
{
    struct sfz_tu *t = &m->t[i];
    for (int k = 0; k < m->nh; k++)
        t->inc[k] = rp(g, 60) ? (rp(g, 20) ? 2 : 1) : 0;
    if (first_inc(t, m->nh) < 0)
        t->inc[ri(g, 0, m->nh - 1)] = 1;
    t->sysinc = rp(g, 40);
    t->cross = ri(g, 0, m->nt - 1);
    if (t->cross == i)
        t->cross = (i + 1) % m->nt;
    t->m_thr = ri(g, 1, 6);
    t->ctr_j = -1;
    for (int j = 0; j < SFZ_NTEMPL; j++) {
        t->on[j] = rp(g, 45);
        t->k[j] = pick_inc(g, t, m->nh);
        t->c[j] = ri(g, 1, 50);
    }
    t->on[T_HELPER] = t->on[T_S1] = t->on[T_E1] = 1;
}

static void gen_model(struct sfz_gen *g, struct sfz_model *m)
{
    memset(m, 0, sizeof(*m));
    m->noctr = g->noctr;
    m->noline = g->noline;
    m->nh = ri(g, 1, SFZ_MAXH);
    m->nt = ri(g, 3, SFZ_MAXT);
    m->shadow_k = -1;
    m->util_owner = ri(g, 0, m->nt - 1);
    (void)snprintf(m->flags, sizeof(m->flags), "%s",
                   rp(g, 30) ? "-DPROJ_MODE=1" : "");
    for (int k = 0; k < m->nh; k++)
        gen_header(g, m, k);
    for (int i = 0; i < m->nt; i++)
        gen_tu(g, m, i);
    /* each header's owner defines its functions and variables */
    for (int k = 0; k < m->nh; k++) {
        struct sfz_tu *o = &m->t[m->h[k].owner];
        o->inc[k] = o->inc[k] ? o->inc[k] : 1;
    }
    /* re-pick header refs now that owners include theirs */
    for (int i = 0; i < m->nt; i++)
        for (int j = 0; j < SFZ_NTEMPL; j++)
            if (m->t[i].k[j] < 0)
                m->t[i].k[j] = pick_inc(g, &m->t[i], m->nh);
}

/* ---- files ----------------------------------------------------------------------- */

bool sfz_mkdirs(const char *path)
{
    char tmp[4096];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp))
        LOG_FAIL("sfz", "path too long: %s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = 0;
        if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
            LOG_FAIL("sfz", "mkdir %s: %s", tmp, strerror(errno));
        *p = '/';
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST)
        LOG_FAIL("sfz", "mkdir %s: %s", tmp, strerror(errno));
    return true;
}

bool sfz_put(const char *dir, const char *rel, const char *text, size_t n)
{
    char path[4096], d[4096];
    FILE *f;
    bool ok;
    if (snprintf(path, sizeof(path), "%s/%s", dir, rel) >= (int)sizeof(path))
        LOG_FAIL("sfz", "path too long: %s/%s", dir, rel);
    memcpy(d, path, sizeof(d));
    *strrchr(d, '/') = 0;
    if (!sfz_mkdirs(d))
        return false;
    f = fopen(path, "wb");
    if (f == NULL)
        LOG_FAIL("sfz", "open %s: %s", path, strerror(errno));
    ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0 || !ok)
        LOG_FAIL("sfz", "write %s: %s", path, strerror(errno));
    return true;
}

bool sfz_symlink(const char *dir, const char *rel, const char *target)
{
    char path[4096], d[4096];
    if (snprintf(path, sizeof(path), "%s/%s", dir, rel) >= (int)sizeof(path))
        LOG_FAIL("sfz", "path too long: %s/%s", dir, rel);
    memcpy(d, path, sizeof(d));
    *strrchr(d, '/') = 0;
    if (!sfz_mkdirs(d))
        return false;
    if (unlink(path) != 0 && errno != ENOENT)
        LOG_FAIL("sfz", "unlink %s: %s", path, strerror(errno));
    if (symlink(target, path) != 0)
        LOG_FAIL("sfz", "symlink %s -> %s: %s", path, target, strerror(errno));
    return true;
}

static bool put_buf(const char *dir, const char *rel, const struct sfz_buf *b)
{
    if (b->bad)
        LOG_FAIL("sfz", "rendering %s ran out of memory", rel);
    return sfz_put(dir, rel, b->p ? b->p : "", b->n);
}

static bool render_opt_and_shadow(const struct sfz_model *m, const char *dir,
                                  struct sfz_buf *b)
{
    char rel[256];
    bool ok = true;
    for (int k = 0; ok && k < m->nh; k++) {
        if (!m->h[k].opt_file)
            continue;
        b->n = 0;
        sfz_bp(b, "#define H%d_OPTFILE %d\n", k, m->h[k].opt_file);
        (void)snprintf(rel, sizeof(rel), "inc1/h%d_opt.h", k);
        ok = put_buf(dir, rel, b);
    }
    if (!ok || m->shadow_k < 0)
        return ok;
    struct sfz_hdr sh = m->h[m->shadow_k];
    if (m->shadow_variant == 1)
        sh.A++;
    else if (m->shadow_variant == 2)
        sh.inl_add++;
    b->n = 0;
    sfz_render_header(m, m->shadow_k, &sh, b);
    (void)snprintf(rel, sizeof(rel), "%s/h%d.h",
                   m->shadow_where == 0 ? "inc1" : "src", m->shadow_k);
    return put_buf(dir, rel, b);
}

static bool render_files(const struct sfz_model *m, const char *dir,
                         struct sfz_buf *b)
{
    char rel[256];
    bool ok = true;
    for (int k = 0; ok && k < m->nh; k++) {
        b->n = 0;
        sfz_render_header(m, k, &m->h[k], b);
        (void)snprintf(rel, sizeof(rel), "inc%d/h%d.h", m->h[k].dir, k);
        ok = put_buf(dir, rel, b);
    }
    for (int i = 0; ok && i < m->nt; i++) {
        b->n = 0;
        sfz_render_tu(m, i, b);
        (void)snprintf(rel, sizeof(rel), "src/t%d.c", i);
        ok = put_buf(dir, rel, b);
    }
    ok = ok && render_opt_and_shadow(m, dir, b);
    /* keep both -I dirs present */
    ok = ok && sfz_put(dir, "inc1/.keep", "keep\n", 5) &&
         sfz_put(dir, "inc2/.keep", "keep\n", 5);
    b->n = 0;
    sfz_bp(b, "# generated project\nCFLAGS_EXTRA = %s\n", m->flags);
    return ok && put_buf(dir, "Makefile", b);
}

static bool render(const struct sfz_model *m, const char *dir)
{
    struct sfz_buf b = {0};
    bool ok = sfz_mkdirs(dir) && render_files(m, dir, &b);
    free(b.p);
    return ok;
}

/* ---- mutations ------------------------------------------------------------------- */

static const char *const k_kinds[] = {
    "body_static",   "body_extern", "typedef",   "layout",       "macro_value",
    "macro_cond",    "signature",   "shadow",    "flag",         "comment_ws",
    "header_inline", "enum_value",  "macro_new", "header_const", "counter_c",
    "hasinc",        "multi",
    /* the data layer: after "multi", so a seed draws its kind as before */
    "data_string",   "data_table",  "data_hconst", "data_index",
};
#define NKINDS (sizeof(k_kinds) / sizeof(k_kinds[0]))
#define KIND_FLAG 8
#define KIND_COUNTER_C 14
#define KIND_MULTI 16
#define KIND_DATA 17 /* the first data_* kind */

/* One mutation of header k / TU i (both drawn before the kind's own draws). */
struct mut {
    struct sfz_gen *g;
    struct sfz_model *m;
    int k, i;
    char *detail;
    size_t cap;
};

static bool in_set(int j, const int *set, size_t n)
{
    for (size_t q = 0; q < n; q++)
        if (set[q] == j)
            return true;
    return false;
}

/* a TU with template j on, or -1 */
static int tu_with(struct sfz_gen *g, const struct sfz_model *m, int j)
{
    int c[SFZ_MAXT], n = 0;
    for (int i = 0; i < m->nt; i++)
        if (m->t[i].on[j] && m->t[i].k[j] >= 0)
            c[n++] = i;
    return n ? c[ri(g, 0, n - 1)] : -1;
}

static void mut_body_static(struct mut *u)
{
    static const int js[] = {T_HELPER, T_S1, T_S2, T_CTOR, T_UTIL, T_KC, T_CLEAN};
    struct sfz_tu *t = &u->m->t[u->i];
    int j = js[ri(u->g, 0, 6)];
    if (j == T_UTIL && u->i == u->m->util_owner)
        j = T_HELPER;
    if (!t->on[j] || (j != T_HELPER && j != T_S1 && t->k[j] < 0))
        j = T_HELPER;
    t->c[j] += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "src/t%d.c template %d constant", u->i, j);
}

static void mut_body_extern(struct mut *u)
{
    static const int js[] = {T_E1, T_INL, T_MAC, T_LINE, T_CROSS, T_PTR,
                             T_WEAK, T_HFN, T_GEN, T_KC, T_GV, T_UTIL, T_F,
                             T_XD, T_TAB, T_LIM, T_ALIAS, T_HAS, T_GC, T_K, T_HST};
    /* templates this mutation may pick although their header is absent */
    static const int nohdr[] = {T_E1, T_LINE, T_CROSS, T_PTR, T_WEAK,
                                T_KC, T_GV, T_UTIL, T_LIM};
    struct sfz_tu *t = &u->m->t[u->i];
    int j = js[ri(u->g, 0, 20)];
    if (j == T_UTIL && u->i != u->m->util_owner)
        j = T_E1;
    if (!t->on[j] ||
        (t->k[j] < 0 && !in_set(j, nohdr, sizeof(nohdr) / sizeof(nohdr[0]))))
        j = T_E1;
    t->c[j] += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "src/t%d.c template %d constant", u->i, j);
}

static void mut_typedef(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    int v = h->i0type;
    while (h->i0type == v)
        h->i0type = ri(u->g, 0, 5);
    (void)snprintf(u->detail, u->cap, "h%d_i0 %s -> %s", u->k, k_sfz_types[v],
                   k_sfz_types[h->i0type]);
}

static void mut_layout(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    const char *what;
    switch (ri(u->g, 0, 4)) {
    case 0: h->extra_field ^= 1; what = "extra field"; break;
    case 1: h->swap_fields ^= 1; what = "field swap"; break;
    case 2: h->in_type = h->in_type == 2 ? 0 : 2; what = "in.s type"; break;
    case 3: h->has_union ^= 1; what = "anon union toggled"; break;
    default: h->has_anon ^= 1; what = "anon struct toggled"; break;
    }
    (void)snprintf(u->detail, u->cap, "h%d_s %s", u->k, what);
}

static void mut_macro_value(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    int k = u->k;
    switch (ri(u->g, 0, 4)) {
    case 0: h->A += ri(u->g, 1, 2); (void)snprintf(u->detail, u->cap, "H%d_A -> %d", k, h->A); return;
    case 1: h->Bplus += 1; (void)snprintf(u->detail, u->cap, "H%d_B body", k); return;
    case 2: h->Fadd += 1; (void)snprintf(u->detail, u->cap, "H%d_F body", k); return;
    case 3: h->ctr_add += 1; (void)snprintf(u->detail, u->cap, "H%d_CTR body", k); return;
    default: break;
    }
    if (u->g->noctr) {
        h->A += 1;
        (void)snprintf(u->detail, u->cap, "H%d_A -> %d", k, h->A);
        return;
    }
    h->ctr_macro_n = 3 - h->ctr_macro_n;
    (void)snprintf(u->detail, u->cap, "H%d_CTR __COUNTER__ uses %d", k,
                   h->ctr_macro_n);
}

static void mut_opt_toggle(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    h->opt_defined ^= 1;
    (void)snprintf(u->detail, u->cap, "H%d_OPT defined=%d", u->k, h->opt_defined);
}

static void mut_main_threshold(struct mut *u)
{
    int ti = tu_with(u->g, u->m, T_COND);
    if (ti < 0) {
        mut_opt_toggle(u);
        return;
    }
    u->m->t[ti].m_thr += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "src/t%d.c main-file #if threshold", ti);
}

static void mut_macro_cond(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    int k = u->k;
    switch (ri(u->g, 0, 4)) {
    case 0: mut_opt_toggle(u); break;
    case 1:
        h->D_thr += ri(u->g, -2, 2) | 1;
        (void)snprintf(u->detail, u->cap, "H%d_D threshold -> %d", k, h->D_thr);
        break;
    case 2: mut_main_threshold(u); break;
    case 3:
        h->C_opt += 1;
        h->C_else += 1;
        (void)snprintf(u->detail, u->cap, "H%d_C both branches", k);
        break;
    default:
        h->X_thr += ri(u->g, -2, 2) | 1;
        (void)snprintf(u->detail, u->cap, "H%d_X threshold -> %d", k, h->X_thr);
        break;
    }
}

static void mut_signature(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    if (rp(u->g, 50)) {
        h->fn_long ^= 1;
        (void)snprintf(u->detail, u->cap, "h%d_fn returns %s", u->k,
                       h->fn_long ? "long" : "int");
        return;
    }
    h->fn2_param = h->fn2_param == 0 ? 1 : 0;
    (void)snprintf(u->detail, u->cap, "h%d_fn2 param %s", u->k,
                   k_sfz_types[h->fn2_param]);
}

static void mut_shadow(struct mut *u)
{
    struct sfz_model *m = u->m;
    m->shadow_k = u->k;
    m->shadow_where = m->h[u->k].dir == 1 ? 1 : ri(u->g, 0, 1);
    m->shadow_variant = ri(u->g, 0, 2);
    (void)snprintf(u->detail, u->cap, "new %s/h%d.h variant %d",
                   m->shadow_where == 0 ? "inc1" : "src", u->k, m->shadow_variant);
}

static void mut_flag(struct mut *u)
{
    static const char *const fl[] = {"-DH0_OPT=1", "-DPROJ_MODE=1",
                                     "-fwrapv", "-funsigned-char",
                                     "-DUNUSED_FLAG=3", "-fno-strict-aliasing",
                                     "-fno-inline"};
    char *flags = u->m->flags;
    size_t cap = sizeof(u->m->flags), len = strlen(flags);
    const char *f = fl[ri(u->g, 0, 6)];
    if (strstr(flags, f) != NULL)
        flags[0] = '\0';
    else
        (void)snprintf(flags + len, cap - len, "%s%s", flags[0] ? " " : "", f);
    (void)snprintf(u->detail, u->cap, "CFLAGS_EXTRA -> '%s'", flags);
}

static void mut_comment_ws(struct mut *u)
{
    struct sfz_tu *t = &u->m->t[u->i];
    int js[SFZ_NTEMPL], n = 0;
    if (rp(u->g, 40)) {
        u->m->h[u->k].comment = ri(u->g, 1, 5);
        (void)snprintf(u->detail, u->cap, "h%d.h comment mode %d", u->k,
                       u->m->h[u->k].comment);
        return;
    }
    t->comment = ri(u->g, 1, 5);
    for (int j = 0; j < SFZ_NTEMPL; j++)
        if (t->on[j])
            js[n++] = j;
    t->comment_t = js[ri(u->g, 0, n - 1)];
    (void)snprintf(u->detail, u->cap, "src/t%d.c comment mode %d template %d",
                   u->i, t->comment, t->comment_t);
}

static void mut_header_inline(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    int k = u->k;
    switch (ri(u->g, 0, 2)) {
    case 0: h->inl_add += 1; (void)snprintf(u->detail, u->cap, "h%d_inl constant", k); break;
    case 1: h->inl_ctr ^= 1; (void)snprintf(u->detail, u->cap, "h%d_inl __COUNTER__ uses %d", k, h->inl_ctr); break;
    default: h->inl2_add += 1; (void)snprintf(u->detail, u->cap, "h%d_inl2 constant", k); break;
    }
}

static void mut_enum_value(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    if (rp(u->g, 50))
        h->E0 += 1;
    else
        h->E2 += 1;
    (void)snprintf(u->detail, u->cap, "h%d_e values", u->k);
}

/* the header defines T_LIM, which a TU #ifndef-guards */
static void mut_macro_new(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    h->new_lim = h->new_lim ? 0 : ri(u->g, 60, 90);
    (void)snprintf(u->detail, u->cap, "h%d.h T_LIM -> %d", u->k, h->new_lim);
}

static void mut_header_const(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    if (rp(u->g, 50)) {
        h->K += 1;
        (void)snprintf(u->detail, u->cap, "h%d_K -> %d", u->k, h->K);
        return;
    }
    h->tab1 += 1;
    (void)snprintf(u->detail, u->cap, "h%d_tab[1] -> %d", u->k, h->tab1);
}

/* a main-file body gains or loses a __COUNTER__ */
static void mut_counter_c(struct mut *u)
{
    static const int js[] = {T_HELPER, T_S1, T_E1};
    struct sfz_tu *t = &u->m->t[u->i];
    t->ctr_j = t->ctr_j >= 0 ? -1 : js[ri(u->g, 0, 2)];
    (void)snprintf(u->detail, u->cap, "src/t%d.c template %d __COUNTER__ %s", u->i,
                   t->ctr_j >= 0 ? t->ctr_j : 0,
                   t->ctr_j >= 0 ? "added" : "removed");
}

/* create or delete the file a header probes */
static void mut_hasinc(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    h->opt_file ^= 1;
    (void)snprintf(u->detail, u->cap, "inc1/h%d_opt.h %s (header includes it: %d)",
                   u->k, h->opt_file ? "created" : "deleted", h->opt_inc);
}

/* ---- the data layer ------------------------------------------------------------ */

/* Its starting values, drawn only for a data_* kind so every other kind's
 * projects stay byte-identical for a seed. */
static void gen_data(struct sfz_gen *g, struct sfz_model *m)
{
    m->data = true;
    for (int k = 0; k < m->nh; k++) {
        m->h[k].idx = ri(g, 0, 3);
        m->h[k].dtab1 = ri(g, 10, 99);
    }
    for (int i = 0; i < m->nt; i++) {
        m->t[i].str_n = ri(g, 0, 9);
        m->t[i].str_pad = ri(g, 0, 3);
        for (int q = 0; q < 4; q++)
            m->t[i].dt[q] = ri(g, 1, 99);
    }
}

/* a string literal in tN_str: the same length, or longer (tN_str2's
 * literal then moves) */
static void mut_data_string(struct mut *u)
{
    struct sfz_tu *t = &u->m->t[u->i];
    if (rp(u->g, 50)) {
        t->str_n = (t->str_n + ri(u->g, 1, 8)) % 10;
        (void)snprintf(u->detail, u->cap, "src/t%d.c t%d_str literal digit", u->i,
                       u->i);
        return;
    }
    t->str_pad += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "src/t%d.c t%d_str literal grows to pad %d",
                   u->i, u->i, t->str_pad);
}

static void mut_data_table(struct mut *u)
{
    struct sfz_tu *t = &u->m->t[u->i];
    int q = ri(u->g, 0, 3);
    t->dt[q] += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "src/t%d.c t%d_dt[%d] -> %d", u->i, u->i,
                   q, t->dt[q]);
}

static void mut_data_hconst(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    h->dtab1 += ri(u->g, 1, 3);
    (void)snprintf(u->detail, u->cap, "h%d_dtab[1] -> %d", u->k, h->dtab1);
}

static void mut_data_index(struct mut *u)
{
    struct sfz_hdr *h = &u->m->h[u->k];
    h->idx = (h->idx + ri(u->g, 1, 3)) % 4;
    (void)snprintf(u->detail, u->cap, "H%d_IDX -> %d", u->k, h->idx);
}

static void (*const k_mut[NKINDS])(struct mut *u) = {
    mut_body_static,   mut_body_extern, mut_typedef,     mut_layout,
    mut_macro_value,   mut_macro_cond,  mut_signature,   mut_shadow,
    mut_flag,          mut_comment_ws,  mut_header_inline, mut_enum_value,
    mut_macro_new,     mut_header_const, mut_counter_c,  mut_hasinc,
    [KIND_DATA] = mut_data_string, mut_data_table, mut_data_hconst,
    mut_data_index,
};

static void mutate(struct sfz_gen *g, struct sfz_model *m, int kind,
                   char *detail, size_t cap)
{
    struct mut u = {.g = g, .m = m, .detail = detail, .cap = cap};
    u.k = ri(g, 0, m->nh - 1);
    u.i = ri(g, 0, m->nt - 1);
    k_mut[kind](&u);
}

/* two or three single mutations (never the flag one) */
static void mutate_multi(struct sfz_gen *g, struct sfz_model *m, char *detail,
                         size_t cap)
{
    int n = ri(g, 2, 3), q = 0;
    size_t w = 0;
    for (int r = 0; r < n && w < cap; r++) {
        do
            q = ri(g, 0, 15);
        while (q == KIND_FLAG || (g->noctr && q == KIND_COUNTER_C));
        (void)snprintf(detail + w, cap - w, "%s%s:", r ? "; " : "", k_kinds[q]);
        w = strlen(detail);
        mutate(g, m, q, detail + w, cap - w);
        w = strlen(detail);
    }
}

/* The kind: drawn over the kinds before the data layer exactly as before
 * it existed, so a forced kind leaves the draws that follow unchanged; an
 * unforced draw then takes a data_* kind one time in five. */
static int pick_kind(struct sfz_gen *g, const char *want)
{
    int kind;
    do
        kind = ri(g, 0, KIND_DATA - 1);
    while ((g->noflag && kind == KIND_FLAG) ||
           (g->noctr && kind == KIND_COUNTER_C));
    if (want == NULL && rp(g, 20))
        kind = ri(g, KIND_DATA, (int)NKINDS - 1);
    for (size_t q = 0; want != NULL && q < NKINDS; q++)
        if (strcmp(want, k_kinds[q]) == 0)
            kind = (int)q;
    return kind;
}

static bool write_meta(const char *dir, uint64_t seed, const struct sfz_meta *meta)
{
    struct sfz_buf b = {0};
    bool ok;
    sfz_bp(&b, "seed=%llu\nkind=%s\ndetail=%s\ntus=", (unsigned long long)seed,
           meta->kind, meta->detail);
    for (int i = 0; i < meta->ntus; i++)
        sfz_bp(&b, "%ssrc/t%d.c", i ? " " : "", i);
    sfz_bp(&b, "\n");
    ok = put_buf(dir, "meta.txt", &b);
    free(b.p);
    return ok;
}

bool sfz_generate(uint64_t seed, unsigned profile, const char *kind_name,
                  const char *dir, struct sfz_meta *meta)
{
    struct sfz_gen g = {.rng = seed * 0x2545f4914f6cdd1dull + 7,
                        .noctr = (profile & SFZ_NO_CTR) != 0,
                        .noline = (profile & SFZ_NO_LINE) != 0,
                        .noflag = (profile & SFZ_NO_FLAG) != 0};
    struct sfz_model *m = zcl_calloc(1, sizeof(*m), "sfz.model");
    char path[4096];
    bool ok = m != NULL;
    int kind = 0;
    memset(meta, 0, sizeof(*meta));
    if (ok) {
        gen_model(&g, m);
        kind = pick_kind(&g, kind_name);
        if (kind >= KIND_DATA)
            gen_data(&g, m);
        (void)snprintf(path, sizeof(path), "%s/before", dir);
        ok = render(m, path);
    }
    if (ok && kind == KIND_MULTI)
        mutate_multi(&g, m, meta->detail, sizeof(meta->detail));
    else if (ok)
        mutate(&g, m, kind, meta->detail, sizeof(meta->detail));
    if (ok) {
        (void)snprintf(path, sizeof(path), "%s/after", dir);
        (void)snprintf(meta->kind, sizeof(meta->kind), "%s", k_kinds[kind]);
        meta->ntus = m->nt;
        ok = render(m, path) && write_meta(dir, seed, meta);
    }
    free(m);
    return ok;
}

bool sfz_kind_known(const char *kind)
{
    for (size_t q = 0; q < NKINDS; q++)
        if (strcmp(kind, k_kinds[q]) == 0)
            return true;
    return false;
}
