/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Declaration-identity consumer: the universe of TUs that read a changed file (manifests cross-checked against the depfile graph), and the plan and report it drives. */
#include "devloop_facts_consumer.h"

#include "codeindex/codeindex.h"
#include "util/safe_alloc.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* A scan deeper than this, or with more manifests than the report holds,
 * does not know its universe. */
#define FXC_SCAN_DEPTH 32
#define FXC_READERS_MAX 4096

/* ---- small shared helpers ---------------------------------------------------- */

bool fxc_strs_add(struct fxc_strs *s, const char *v)
{
    char *d;
    if (s->n == s->cap) {
        size_t next = s->cap ? s->cap * 2 : 32;
        char **g = zcl_realloc(s->v, next * sizeof(*g), "facts_consumer.strs");
        if (g == NULL)
            return false;
        s->v = g;
        s->cap = next;
    }
    d = zcl_malloc(strlen(v) + 1, "facts_consumer.str");
    if (d == NULL)
        return false;
    memcpy(d, v, strlen(v) + 1);
    s->v[s->n++] = d;
    return true;
}

bool fxc_strs_has(const struct fxc_strs *s, const char *v)
{
    for (size_t k = 0; k < s->n; k++)
        if (strcmp(s->v[k], v) == 0)
            return true;
    return false;
}

void fxc_strs_free(struct fxc_strs *s)
{
    for (size_t k = 0; k < s->n; k++)
        free(s->v[k]);
    free(s->v);
    memset(s, 0, sizeof(*s));
}

bool fxc_is_changed(const struct fxc *c, const char *path)
{
    for (size_t k = 0; k < c->nfiles; k++)
        if (strcmp(c->files[k], path) == 0)
            return true;
    return false;
}

struct fxc_hdr *fxc_hdr_of(struct fxc *c, const char *path)
{
    for (size_t k = 0; k < c->nfiles; k++)
        if (strcmp(c->files[k], path) == 0)
            return &c->hdrs[k];
    return NULL;
}

struct zcl_devloop_facts_tu_verdict *fxc_tu_new(struct fxc *c, const char *path)
{
    struct zcl_devloop_facts_report *r = c->report;
    struct zcl_devloop_facts_tu_verdict *t;
    if (r->ntus == c->captus) {
        size_t next = c->captus ? c->captus * 2 : 64;
        struct zcl_devloop_facts_tu_verdict *g =
            zcl_realloc(r->tus, next * sizeof(*g), "facts_consumer.tus");
        if (g == NULL)
            return NULL;
        r->tus = g;
        c->captus = next;
    }
    t = &r->tus[r->ntus++];
    memset(t, 0, sizeof(*t));
    (void)snprintf(t->path, sizeof(t->path), "%s", path);
    t->reason = "";
    t->action_reason = "";
    return t;
}

static struct zcl_devloop_facts_tu_verdict *fxc_tu_find(struct fxc *c,
                                                        const char *path)
{
    for (size_t k = 0; k < c->report->ntus; k++)
        if (strcmp(c->report->tus[k].path, path) == 0)
            return &c->report->tus[k];
    return NULL;
}

int fxc_readers(struct fxc *c, const char *path, char (*out)[256], int cap)
{
    enum codeindex_include_dim dim = CODEINDEX_INCLUDE_DIM_UNAVAILABLE;
    int n;
    if (c->ci == NULL) {
        c->graph = CODEINDEX_INCLUDE_DIM_UNAVAILABLE;
        return -1;
    }
    n = codeindex_reverse_includes(c->ci, path, out, cap, &dim);
    c->graph = (int)dim;
    return n < 0 || dim != CODEINDEX_INCLUDE_DIM_COMPLETE ? -1 : n;
}

/* ---- candidates: every after manifest under facts_dir ------------------------ */

static bool fxc_ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static bool fxc_scan(struct fxc *c, const char *rel, int depth);

static bool fxc_scan_entry(struct fxc *c, const char *rel, const char *name,
                           int depth)
{
    char child[ZCL_DEVLOOP_PATH_MAX], full[ZCL_DEVLOOP_PATH_MAX * 2];
    struct stat st;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return true;
    if (snprintf(child, sizeof(child), "%s%s%s", rel, rel[0] ? "/" : "",
                 name) >= (int)sizeof(child) ||
        snprintf(full, sizeof(full), "%s/%s/%s", c->root, c->facts_dir,
                 child) >= (int)sizeof(full) ||
        lstat(full, &st) != 0)
        return false;
    if (S_ISDIR(st.st_mode))
        return depth < FXC_SCAN_DEPTH && fxc_scan(c, child, depth + 1);
    if (!S_ISREG(st.st_mode) || !fxc_ends_with(child, ".after.zsm"))
        return true;
    child[strlen(child) - strlen(".after.zsm")] = '\0';
    return c->cand.n < ZCL_DEVLOOP_FACTS_TU_MAX && fxc_strs_add(&c->cand, child);
}

static bool fxc_scan(struct fxc *c, const char *rel, int depth)
{
    char dir[ZCL_DEVLOOP_PATH_MAX * 2];
    struct dirent *de;
    DIR *d;
    bool ok = true;
    if (snprintf(dir, sizeof(dir), "%s/%s%s%s", c->root, c->facts_dir,
                 rel[0] ? "/" : "", rel) >= (int)sizeof(dir))
        return false;
    d = opendir(dir);
    if (d == NULL)
        return depth == 0; /* no facts directory: no candidates */
    while (ok && (de = readdir(d)) != NULL)
        ok = fxc_scan_entry(c, rel, de->d_name, depth);
    (void)closedir(d);
    return ok;
}

static int fxc_str_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ---- the depfile cross-check ---------------------------------------------------- */

void fxc_incomplete(struct fxc *c, const char *reason, const char *path)
{
    struct zcl_devloop_facts_report *r = c->report;
    if (!r->complete)
        return;
    r->complete = false;
    r->reason = reason;
    (void)snprintf(r->detail, sizeof(r->detail), "%s", path);
}

static const char *fxc_graph_reason(const struct fxc *c)
{
    if (c->ci == NULL)
        return "no-code-index";
    return c->graph == CODEINDEX_INCLUDE_DIM_TRUNCATED
               ? "include-graph-truncated"
               : "include-graph-unavailable";
}

/* A TU the depfile graph says reads `file` that no manifest pair here
 * describes as reading it is affected: without facts nothing narrows it.
 * When `file` is a .c it compiles as an include, nothing names the
 * functions it defines from it either, so the universe is incomplete. */
static bool fxc_reader(struct fxc *c, const char *file, const char *reader)
{
    struct zcl_devloop_facts_tu_verdict *t;
    if (!fxc_ends_with(reader, ".c") || fxc_tu_find(c, reader) != NULL)
        return true;
    t = fxc_tu_new(c, reader);
    if (t == NULL)
        return false;
    t->affected = true;
    t->broadened = true;
    t->reason = fxc_strs_has(&c->cand, reader) ? "include-resolution-change"
                                               : "facts-missing";
    (void)snprintf(t->detail, sizeof(t->detail),
                   "the depfile graph says it reads %s", file);
    if (fxc_ends_with(file, ".c") && strcmp(file, reader) != 0)
        fxc_incomplete(c, t->reason, reader);
    return true;
}

static bool fxc_cross_check(struct fxc *c)
{
    char(*readers)[256] = zcl_calloc(FXC_READERS_MAX, 256, "facts_consumer.rd");
    bool ok = readers != NULL;
    for (size_t k = 0; ok && k < c->nfiles; k++) {
        int n = fxc_readers(c, c->files[k], readers, FXC_READERS_MAX);
        if (n < 0) {
            fxc_incomplete(c, fxc_graph_reason(c), c->files[k]);
            continue;
        }
        for (int i = 0; ok && i < n; i++)
            ok = fxc_reader(c, c->files[k], readers[i]);
    }
    free(readers);
    return ok;
}

/* ---- build inputs: changed files no compile records reading -------------------- */

/* A changed file that is not C text or prose: a makefile, a flag file, a
 * catalog, a script, a fixture. No manifest records reading it. */
static bool fxc_build_input(const char *path)
{
    return !fxc_ends_with(path, ".c") && !fxc_ends_with(path, ".h") &&
           !fxc_ends_with(path, ".md") && strncmp(path, "docs/", 5) != 0;
}

/* ---- make inputs: the paths the makefile text names ---------------------------- */

/* Make reads a path when a makefile names it: its own name, a literal path
 * or basename, a directory ending in '/', or a glob or pattern ('*', '%')
 * outside a whole-line comment. A path-like variable (one definition, no
 * whitespace) is expanded, any other reference matches anything, and a
 * function call is its arguments. What the text cannot be read for is
 * UNKNOWN, and UNKNOWN is a make input. */
#define FXM_FILES_MAX 64
#define FXM_TEXT_MAX (64u << 20)
#define FXM_LINE_MAX (8u << 20)
#define FXM_ROUNDS 8
#define FXM_ANY '\x01'

struct fxm_buf {
    char *p;
    size_t n, cap;
};

struct fxm_var {
    char *name;
    char *value; /* NULL: not one path-like definition */
};

struct fxm {
    const char *root;
    struct fxc_strs files; /* makefiles read, repo-relative */
    char *text[FXM_FILES_MAX];
    struct fxm_var *vars;
    size_t nvars, capvars;
    bool unknown;
    const char *const *paths;
    const bool *want; /* the paths asked about */
    bool *make;       /* ...and those the text names */
    size_t npaths;
    struct fxm_buf line, a, b;
};

static bool fxm_put(struct fxm_buf *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) {
        size_t next = b->cap ? b->cap : 256;
        char *g;
        while (next < b->n + n + 1)
            next *= 2;
        if (next > FXM_LINE_MAX ||
            (g = zcl_realloc(b->p, next, "facts_consumer.mk")) == NULL)
            return false;
        b->p = g;
        b->cap = next;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
    return true;
}

static bool fxm_ident(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-';
}

static bool fxm_space(char ch)
{
    return ch == ' ' || ch == '\t';
}

/* The next logical line at *at, continuations joined by a space: 0 at the
 * end, -1 when it cannot be held. */
static int fxm_next_line(struct fxm_buf *line, const char **at, bool *recipe,
                         bool *comment)
{
    const char *p = *at, *q = p;
    bool more = true;
    if (*p == '\0')
        return 0;
    line->n = 0;
    *recipe = *p == '\t';
    while (fxm_space(*q))
        q++;
    *comment = !*recipe && *q == '#';
    while (more) {
        const char *e = strchr(p, '\n');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p), bs = 0;
        while (bs < n && p[n - 1 - bs] == '\\')
            bs++;
        more = e != NULL && bs % 2 == 1;
        if (!fxm_put(line, p, more ? n - 1 : n) || !fxm_put(line, " ", 1))
            return -1;
        p = e != NULL ? e + 1 : p + n;
    }
    *at = p;
    return 1;
}

static bool fxm_var_add(struct fxm *m, const char *name, size_t n,
                        const char *value)
{
    struct fxm_var *v;
    if (m->nvars == m->capvars) {
        size_t next = m->capvars ? m->capvars * 2 : 256;
        struct fxm_var *g =
            zcl_realloc(m->vars, next * sizeof(*g), "facts_consumer.mkvars");
        if (g == NULL)
            return false;
        m->vars = g;
        m->capvars = next;
    }
    v = &m->vars[m->nvars];
    v->name = zcl_malloc(n + 1, "facts_consumer.mkvar");
    v->value = value != NULL ? zcl_strdup(value, "facts_consumer.mkval") : NULL;
    if (v->name == NULL || (value != NULL && v->value == NULL)) {
        free(v->name);
        free(v->value);
        return false;
    }
    memcpy(v->name, name, n);
    v->name[n] = '\0';
    m->nvars++;
    return true;
}

/* The operator after a variable name: its length, 0 when there is none.
 * *path_like: the value can stand for one word ('+=' and '!=' cannot). */
static size_t fxm_op(const char *p, bool *path_like)
{
    static const char *const ops[] = {":::=", "::=", ":=", "?=", "=", "+=",
                                      "!="};
    for (size_t k = 0; k < sizeof(ops) / sizeof(ops[0]); k++)
        if (strncmp(p, ops[k], strlen(ops[k])) == 0) {
            *path_like = k < 5;
            return strlen(ops[k]);
        }
    return 0;
}

static const char *fxm_skip_prefixes(const char *p)
{
    static const char *const pre[] = {"override ", "export ", "private "};
    bool again = true;
    while (again) {
        again = false;
        while (fxm_space(*p))
            p++;
        for (size_t k = 0; k < sizeof(pre) / sizeof(pre[0]); k++)
            if (strncmp(p, pre[k], strlen(pre[k])) == 0) {
                p += strlen(pre[k]);
                again = true;
            }
    }
    return p;
}

/* A definition on a logical line: `define NAME` or `NAME op value`. */
static bool fxm_def(struct fxm *m, char *line, bool *in_define)
{
    const char *p = fxm_skip_prefixes(line), *name, *v;
    size_t n, op;
    bool path_like = false;
    char *end;
    if (strncmp(p, "define", 6) == 0 && fxm_space(p[6])) {
        *in_define = true;
        p = fxm_skip_prefixes(p + 6);
        for (n = 0; fxm_ident(p[n]); n++)
            ;
        return n == 0 || fxm_var_add(m, p, n, NULL);
    }
    for (name = p, n = 0; fxm_ident(p[n]); n++)
        ;
    for (p += n; fxm_space(*p); p++)
        ;
    if (n == 0 || (op = fxm_op(p, &path_like)) == 0)
        return true;
    for (v = p + op; fxm_space(*v); v++)
        ;
    end = line + strlen(line);
    while (end > v && fxm_space(end[-1]))
        *--end = '\0';
    if (!path_like || *v == '\0' || strpbrk(v, " \t") != NULL)
        v = NULL;
    return fxm_var_add(m, name, n, v);
}

static int fxm_var_cmp(const void *a, const void *b)
{
    const struct fxm_var *x = a, *y = b;
    return strcmp(x->name, y->name);
}

/* Sort by name; a name defined twice has no single value. */
static void fxm_vars_seal(struct fxm *m)
{
    size_t w = 0;
    qsort(m->vars, m->nvars, sizeof(*m->vars), fxm_var_cmp);
    for (size_t k = 0; k < m->nvars; k++) {
        if (w > 0 && strcmp(m->vars[w - 1].name, m->vars[k].name) == 0) {
            free(m->vars[w - 1].value);
            m->vars[w - 1].value = NULL;
            free(m->vars[k].name);
            free(m->vars[k].value);
            continue;
        }
        m->vars[w++] = m->vars[k];
    }
    m->nvars = w;
}

static const char *fxm_value(const struct fxm *m, const char *name, size_t n)
{
    size_t lo = 0, hi = m->nvars;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int d = strncmp(m->vars[mid].name, name, n);
        if (d == 0 && m->vars[mid].name[n] != '\0')
            d = 1;
        if (d == 0)
            return m->vars[mid].value;
        if (d < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/* The body of one reference: a function call yields its arguments, a
 * path-like variable its value, an automatic variable nothing, anything
 * else FXM_ANY. */
static bool fxm_inner(const struct fxm *m, const char *s, size_t n,
                      struct fxm_buf *out)
{
    const char *v;
    size_t k = 0;
    char any = FXM_ANY;
    while (k < n && fxm_ident(s[k]))
        k++;
    if (k > 0 && k < n && fxm_space(s[k]))
        return fxm_put(out, " ", 1) && fxm_put(out, s + k + 1, n - k - 1) &&
               fxm_put(out, " ", 1);
    if (n > 0 && strchr("@<^+*?|%", s[0]) != NULL)
        return true;
    v = k == n && k > 0 ? fxm_value(m, s, n) : NULL;
    return v != NULL ? fxm_put(out, v, strlen(v)) : fxm_put(out, &any, 1);
}

/* The reference at d ('$'): where the text resumes, NULL when out cannot
 * hold it. */
static const char *fxm_ref(const struct fxm *m, const char *d,
                           struct fxm_buf *out)
{
    char open = d[1], close = open == '(' ? ')' : '}';
    const char *q = d + 2;
    int depth = 1;
    if (open == '\0')
        return d + 1;
    if (open != '(' && open != '{')
        return fxm_inner(m, d + 1, 1, out) ? d + 2 : NULL;
    for (; *q != '\0' && depth > 0; q++)
        depth += *q == open ? 1 : *q == close ? -1 : 0;
    if (depth > 0)
        return fxm_inner(m, "", 0, out) ? q : NULL;
    return fxm_inner(m, d + 2, (size_t)(q - 1 - (d + 2)), out) ? q : NULL;
}

/* One expansion round of in into out; *left: a reference remains. */
static bool fxm_round(const struct fxm *m, const char *in, struct fxm_buf *out,
                      bool *left)
{
    const char *p = in;
    out->n = 0;
    if (!fxm_put(out, "", 0))
        return false;
    *left = false;
    while (p != NULL && *p != '\0') {
        const char *d = strchr(p, '$');
        if (d == NULL)
            return fxm_put(out, p, strlen(p));
        if (!fxm_put(out, p, (size_t)(d - p)))
            return false;
        *left = true;
        p = d[1] == '$' ? d + 2 : fxm_ref(m, d, out);
    }
    return p != NULL;
}

/* A glob against a path: '*', '%', '$' and FXM_ANY match any run of
 * characters, '/' included; a trailing '/' matches a whole directory. */
static bool fxm_wild(char ch)
{
    return ch == '*' || ch == '%' || ch == '$' || ch == FXM_ANY;
}

static bool fxm_glob(const char *p, const char *s)
{
    const char *star = NULL, *back = NULL;
    while (*s != '\0') {
        if (p[0] == '/' && p[1] == '\0' && *s == '/')
            return true;
        if (fxm_wild(*p)) {
            star = p++;
            back = s;
        } else if (*p == *s) {
            p++;
            s++;
        } else if (star != NULL) {
            p = star + 1;
            s = ++back;
        } else {
            return false;
        }
    }
    while (fxm_wild(*p))
        p++;
    return *p == '\0';
}

static bool fxm_named(const char *t, const char *path)
{
    const char *base = strrchr(path, '/');
    size_t tn = strlen(t), bn;
    base = base != NULL ? base + 1 : path;
    bn = strlen(base);
    if (tn > 1 && t[tn - 1] == '/' && strncmp(path, t, tn) == 0)
        return true;
    for (const char *h = strstr(t, base); h != NULL; h = strstr(h + 1, base))
        if ((h == t || h[-1] == '/') && !fxm_ident(h[bn]))
            return true;
    return false;
}

static bool fxm_literal_char(const char *t)
{
    for (; *t != '\0'; t++)
        if (fxm_ident(*t) && *t != '.' && *t != '-')
            return true;
    return false;
}

static void fxm_token(struct fxm *m, const char *t)
{
    bool glob;
    while (*t == '@' || *t == '-' || *t == '+')
        t++;
    if (t[0] == '.' && t[1] == '/')
        t += 2;
    glob = strpbrk(t, "*%$\x01") != NULL;
    if (!fxm_literal_char(t))
        return; /* only references: their definitions are scanned too */
    for (size_t k = 0; k < m->npaths; k++)
        if (m->want[k] && !m->make[k] &&
            (glob ? fxm_glob(t, m->paths[k]) : fxm_named(t, m->paths[k])))
            m->make[k] = true;
}

static bool fxm_sep(char ch)
{
    return ch != '\0' && strchr(" \t\r\n,(){};|'\"`=<>&!:", ch) != NULL;
}

static void fxm_tokens(struct fxm *m, char *s)
{
    while (*s != '\0') {
        char *t, save;
        while (fxm_sep(*s))
            s++;
        for (t = s; *s != '\0' && !fxm_sep(*s); s++)
            ;
        save = *s;
        *s = '\0';
        if (*t != '\0')
            fxm_token(m, t);
        *s = save;
    }
}

/* Expand a logical line (FXM_ROUNDS deep) and match its words. */
static void fxm_scan_line(struct fxm *m, const char *line)
{
    struct fxm_buf *in = &m->a, *out = &m->b;
    bool left = true;
    in->n = 0;
    if (!fxm_put(in, line, strlen(line))) {
        m->unknown = true;
        return;
    }
    for (int r = 0; left && r < FXM_ROUNDS; r++) {
        struct fxm_buf *t = in;
        if (!fxm_round(m, in->p, out, &left)) {
            m->unknown = true;
            return;
        }
        in = out;
        out = t;
    }
    fxm_tokens(m, in->p);
}

/* ---- make inputs: reading the makefiles ---------------------------------------- */

static bool fxm_load(struct fxm *m, const char *rel)
{
    uint8_t *b = NULL;
    size_t n = 0;
    if (fxc_strs_has(&m->files, rel))
        return true;
    if (m->files.n >= FXM_FILES_MAX ||
        !zcl_devloop_facts_read(m->root, NULL, rel, "", FXM_TEXT_MAX, &b, &n))
        return false;
    if (memchr(b, '\0', n) != NULL || !fxc_strs_add(&m->files, rel)) {
        free(b);
        return false;
    }
    m->text[m->files.n - 1] = (char *)b;
    return true;
}

static bool fxm_exists(const char *root, const char *rel)
{
    char full[ZCL_DEVLOOP_PATH_MAX * 2];
    struct stat st;
    return snprintf(full, sizeof(full), "%s/%s", root, rel) < (int)sizeof(full) &&
           stat(full, &st) == 0 && S_ISREG(st.st_mode);
}

/* One word of an include line: a literal repo file is read; a file that
 * does not exist is generated by a rule the text holds; a reference in a
 * mandatory include is UNKNOWN. */
static void fxm_include_word(struct fxm *m, const char *w, bool optional)
{
    if (strpbrk(w, "*%$\x01") != NULL) {
        m->unknown |= !optional;
        return;
    }
    if (w[0] == '/' || strstr(w, "..") != NULL)
        return; /* outside the tree: not a tracked input */
    if (fxm_exists(m->root, w) && !fxm_load(m, w))
        m->unknown = true;
}

static void fxm_include(struct fxm *m, const char *line)
{
    const char *p = line;
    bool optional = *p == '-' || *p == 's';
    bool left = true;
    struct fxm_buf *in = &m->a, *out = &m->b;
    while (*p != '\0' && !fxm_space(*p))
        p++;
    in->n = 0;
    if (!fxm_put(in, p, strlen(p)))
        m->unknown = true;
    for (int r = 0; !m->unknown && left && r < FXM_ROUNDS; r++) {
        struct fxm_buf *t = in;
        m->unknown |= !fxm_round(m, in->p, out, &left);
        in = out;
        out = t;
    }
    for (char *s = in->p; !m->unknown && s != NULL && *s != '\0';) {
        char *w;
        while (fxm_space(*s))
            s++;
        for (w = s; *s != '\0' && !fxm_space(*s); s++)
            ;
        if (*s != '\0')
            *s++ = '\0';
        if (*w != '\0')
            fxm_include_word(m, w, optional);
    }
}

static bool fxm_is_include(const char *p)
{
    return (strncmp(p, "include", 7) == 0 && fxm_space(p[7])) ||
           (strncmp(p, "-include", 8) == 0 && fxm_space(p[8])) ||
           (strncmp(p, "sinclude", 8) == 0 && fxm_space(p[8]));
}

/* Every definition of one makefile's text; its include lines, once. */
static void fxm_defs(struct fxm *m, size_t f, bool includes)
{
    const char *at = m->text[f];
    bool recipe, comment, in_define = false;
    int r;
    while (!m->unknown &&
           (r = fxm_next_line(&m->line, &at, &recipe, &comment)) > 0) {
        const char *p = fxm_skip_prefixes(m->line.p);
        if (comment || recipe)
            continue;
        if (in_define) {
            in_define = strncmp(p, "endef", 5) != 0;
            continue;
        }
        if (includes && fxm_is_include(p))
            fxm_include(m, p);
        else if (!includes && !fxm_def(m, m->line.p, &in_define))
            m->unknown = true;
    }
    m->unknown |= r < 0;
}

/* The root makefile make would pick, and every file it includes. */
static void fxm_read_all(struct fxm *m)
{
    static const char *const roots[] = {"GNUmakefile", "makefile", "Makefile"};
    size_t k = 0;
    while (k < 3 && !fxm_exists(m->root, roots[k]))
        k++;
    if (k == 3 || !fxm_load(m, roots[k])) {
        m->unknown = true;
        return;
    }
    for (size_t f = 0; !m->unknown && f < m->files.n; f++) {
        fxm_defs(m, f, false);
        fxm_vars_seal(m);
        fxm_defs(m, f, true);
    }
}

static void fxm_scan_text(struct fxm *m, size_t f)
{
    const char *at = m->text[f];
    bool recipe, comment;
    int r;
    while (!m->unknown &&
           (r = fxm_next_line(&m->line, &at, &recipe, &comment)) > 0)
        if (!comment)
            fxm_scan_line(m, m->line.p);
    m->unknown |= r < 0;
}

static void fxm_free(struct fxm *m)
{
    for (size_t k = 0; k < m->files.n; k++)
        free(m->text[k]);
    for (size_t k = 0; k < m->nvars; k++) {
        free(m->vars[k].name);
        free(m->vars[k].value);
    }
    free(m->vars);
    fxc_strs_free(&m->files);
    free(m->line.p);
    free(m->a.p);
    free(m->b.p);
}

static bool fxm_makefile_name(const char *path)
{
    const char *b = strrchr(path, '/');
    b = b != NULL ? b + 1 : path;
    return strcmp(b, "GNUmakefile") == 0 || strcmp(b, "makefile") == 0 ||
           strncmp(b, "Makefile", 8) == 0 || fxc_ends_with(b, ".mk") ||
           fxc_ends_with(b, ".make");
}

/* A directory above path, below the root, holds a makefile: a sub-make run
 * there may read the path by a name relative to it. */
static bool fxm_submake(const char *root, const char *path)
{
    static const char *const names[] = {"GNUmakefile", "makefile", "Makefile"};
    char dir[ZCL_DEVLOOP_PATH_MAX], rel[ZCL_DEVLOOP_PATH_MAX + 16];
    if (snprintf(dir, sizeof(dir), "%s", path) >= (int)sizeof(dir))
        return true;
    for (char *s = strrchr(dir, '/'); s != NULL; s = strrchr(dir, '/')) {
        *s = '\0';
        for (size_t k = 0; k < 3; k++)
            if (snprintf(rel, sizeof(rel), "%s/%s", dir, names[k]) >=
                    (int)sizeof(rel) ||
                fxm_exists(root, rel))
                return true;
    }
    return false;
}

/* For each path asked about (want), whether make reads it (make). */
static void fxm_classify(const char *root, const char *const *paths,
                         const bool *want, bool *make, size_t n)
{
    struct fxm m = {.root = root, .paths = paths, .want = want, .make = make,
                    .npaths = n};
    for (size_t k = 0; k < n; k++)
        make[k] = want[k] && (fxm_makefile_name(paths[k]) ||
                              fxm_submake(root, paths[k]));
    fxm_read_all(&m);
    for (size_t f = 0; !m.unknown && f < m.files.n; f++)
        fxm_scan_text(&m, f);
    for (size_t k = 0; k < n; k++)
        make[k] |= want[k] && (m.unknown || fxc_strs_has(&m.files, paths[k]));
    fxm_free(&m);
}

/* ---- build inputs: compiled, make-read, or data --------------------------------- */

/* The build inputs no manifest read and no depfile lists (want); *hit is
 * the first one the depfile graph cannot answer for. */
static bool fxc_unlisted(struct fxc *c, bool *want, const char **hit)
{
    char(*readers)[256] = zcl_calloc(FXC_READERS_MAX, 256, "facts_consumer.bi");
    if (readers == NULL)
        return false;
    for (size_t k = 0; *hit == NULL && k < c->nfiles; k++) {
        int n;
        if (c->hdrs[k].read || !fxc_build_input(c->files[k]))
            continue;
        n = fxc_readers(c, c->files[k], readers, FXC_READERS_MAX);
        if (n < 0)
            *hit = c->files[k];
        want[k] = n == 0;
    }
    free(readers);
    return true;
}

/* The first build input make reads, or the graph cannot answer for. A build
 * input a depfile lists is a compiled input: the cross-check and the header
 * path decide it, widening when it is unattributable. Any other is data no
 * compile reads; its path's groups stay in scope (fxc_data_groups). */
static bool fxc_make_hit(struct fxc *c, const char **hit)
{
    bool *want = zcl_calloc(2 * c->nfiles + 2, sizeof(*want), "facts_consumer.mk");
    bool *make, any = false;
    if (want == NULL || !fxc_unlisted(c, want, hit)) {
        free(want);
        return false;
    }
    make = want + c->nfiles + 1;
    for (size_t k = 0; k < c->nfiles; k++)
        any |= want[k];
    if (*hit == NULL && any)
        fxm_classify(c->root, c->files, want, make, c->nfiles);
    for (size_t k = 0; *hit == NULL && any && k < c->nfiles; k++)
        if (make[k])
            *hit = c->files[k];
    free(want);
    return true;
}

/* Every candidate is affected by a build input make reads, or one the
 * graph cannot place: nothing bounds what it changes, so the universe is
 * incomplete and every group is in scope. */
static bool fxc_build_inputs(struct fxc *c)
{
    const char *hit = NULL;
#if defined(ZCL_TESTING)
    if (zcl_devloop_test_consumer_mutant == ZCL_DEVLOOP_MUTANT_NO_OUTSIDER)
        return true;
#endif
    if (!fxc_make_hit(c, &hit))
        return false;
    if (hit == NULL)
        return true;
    fxc_incomplete(c, "build-input-changed", hit);
    c->universal = true;
    for (size_t k = 0; k < c->cand.n; k++) {
        struct zcl_devloop_facts_tu_verdict *t = fxc_tu_find(c, c->cand.v[k]);
        if (t == NULL && (t = fxc_tu_new(c, c->cand.v[k])) == NULL)
            return false;
        if (t->affected)
            continue;
        t->affected = t->broadened = true;
        t->reason = "build-input-changed";
        (void)snprintf(t->detail, sizeof(t->detail), "%s", hit);
    }
    return true;
}

/* ---- the entry point ----------------------------------------------------------- */

static int fxc_tu_cmp(const void *a, const void *b)
{
    return strcmp(((const struct zcl_devloop_facts_tu_verdict *)a)->path,
                  ((const struct zcl_devloop_facts_tu_verdict *)b)->path);
}

static bool fxc_universe(struct fxc *c)
{
    struct zcl_devloop_facts_report *r = c->report;
    bool ok = fxc_scan(c, "", 0);
    if (!ok || c->cand.n >= ZCL_DEVLOOP_FACTS_TU_MAX)
        fxc_incomplete(c, "facts-scan-bounded", c->facts_dir);
    qsort(c->cand.v, c->cand.n, sizeof(*c->cand.v), fxc_str_cmp);
    ok = true;
    for (size_t k = 0; ok && k < c->cand.n; k++)
        ok = fxc_tu_eval(c, c->cand.v[k]);
    ok = ok && fxc_build_inputs(c) && fxc_cross_check(c) &&
         fxc_name_collisions(c);
    if (c->mixed)
        fxc_incomplete(c, "producer-mismatch", "two producers in the universe");
    qsort(r->tus, r->ntus, sizeof(*r->tus), fxc_tu_cmp);
    for (size_t k = 0; k < r->ntus; k++)
        r->naffected += r->tus[k].affected;
    r->applied = true;
    return ok;
}

static void fxc_free(struct fxc *c)
{
    for (size_t k = 0; c->hdrs != NULL && k < c->nfiles; k++) {
        free(c->hdrs[k].before);
        free(c->hdrs[k].after);
        fxh_free(&c->hdrs[k].diff);
    }
    free(c->hdrs);
    free(c->seeds);
    fxc_strs_free(&c->cand);
    fxc_strs_free(&c->address);
    fxc_strs_free(&c->new_ids);
    fxc_strs_free(&c->new_names);
    fxc_strs_free(&c->checked);
    if (c->ci != NULL)
        codeindex_close(c->ci);
}

/* The .c path's seeds against the declaring headers' readers: each seed is
 * a function of one changed file, found in that file's after manifest. */
static void fxc_c_decls(struct fxc *c, const struct zcl_devloop_facts_tu *tus,
                        const struct zcl_devloop_facts_verdict *v)
{
    for (size_t k = 0; c->seed_reason == NULL && k < c->nfiles; k++) {
        const char *why;
        struct fxi *x = tus[k].after != NULL
                            ? fxi_open(tus[k].after, tus[k].after_len, &why)
                            : NULL;
        for (size_t s = 0; x != NULL && s < v->seeds_len; s++) {
            size_t e;
            if (v->seed_ids[s][0] != '\0' && fxi_find(x, v->seed_ids[s], &e))
                fxc_check_decl(c, x, e);
        }
        fxi_free(x);
    }
}

/* The .c path: the per-function rule chain, then the seed checks only the
 * facts directory can make. A failed check restores the file-seeded plan. */
static bool fxc_c_path(struct fxc *c, const struct zcl_devloop_facts_tu *tus,
                       const struct zcl_devloop_plan *given,
                       struct zcl_devloop_plan *plan,
                       struct zcl_devloop_facts_verdict *v)
{
    struct zcl_devloop_facts_seed *s;
    if (!c->report->complete) /* the universe is not known: as the header path */
        return fxc_fallback(c, given, plan, v);
    if (!zcl_devloop_facts_add_closure_in(c->root, c->files, c->nfiles, tus,
                                          c->nfiles, c->facts_dir, plan, v))
        return false;
    if (!v->narrowed)
        return true;
    s = zcl_calloc(v->seeds_len + 1, sizeof(*s), "facts_consumer.cseeds");
    if (s == NULL)
        return false;
    for (size_t k = 0; k < v->seeds_len; k++) {
        (void)snprintf(s[k].name, sizeof(s[k].name), "%s", v->seeds[k]);
        (void)snprintf(s[k].id, sizeof(s[k].id), "%s", v->seed_ids[k]);
    }
    fxc_check_addresses(c, s, v->seeds_len);
    free(s);
    fxc_c_decls(c, tus, v);
    if (c->seed_reason == NULL)
        return fxc_c_members(c, given, plan, v);
    memcpy(plan, given, sizeof(*plan));
    v->narrowed = false;
    v->reason = c->seed_reason;
    (void)snprintf(v->detail, sizeof(v->detail), "%s", c->seed_detail);
    v->seeds_len = v->seeds_total = 0;
    return zcl_devloop_plan_add_closure(c->root, c->files, c->nfiles, plan);
}

static void fxc_reasons(struct fxc *c, const struct zcl_devloop_plan *plan,
                        const struct zcl_devloop_facts_verdict *v)
{
    struct zcl_devloop_facts_report *r = c->report;
    r->obligations_reason = v->narrowed ? "" : v->reason;
    r->path_reason = "path";
    for (size_t k = 0; k < plan->closure_groups_len; k++)
        if (r->group_reason[k] == NULL)
            r->group_reason[k] = v->narrowed ? "facts-closure" : "plain";
}

static bool fxc_args_ok(const char *const *files, size_t n,
                        const char *facts_dir,
                        const struct zcl_devloop_plan *plan,
                        const struct zcl_devloop_facts_verdict *verdict,
                        const struct zcl_devloop_facts_report *report)
{
    return plan != NULL && verdict != NULL && report != NULL &&
           facts_dir != NULL && (n == 0 || files != NULL);
}

bool zcl_devloop_facts_consume(const char *root, const char *const *files,
                               size_t n, const char *facts_dir,
                               const struct zcl_devloop_facts_tu *tus,
                               struct zcl_devloop_plan *plan,
                               struct zcl_devloop_facts_verdict *verdict,
                               struct zcl_devloop_facts_report *report)
{
    struct fxc c = {.root = root && root[0] ? root : ".", .facts_dir = facts_dir,
                    .files = files, .nfiles = n, .report = report};
    struct zcl_devloop_plan *given = NULL;
    bool ok;
    if (report != NULL)
        memset(report, 0, sizeof(*report));
    if (verdict != NULL)
        memset(verdict, 0, sizeof(*verdict));
    if (!fxc_args_ok(files, n, facts_dir, plan, verdict, report) ||
        (given = zcl_malloc(sizeof(*given), "facts.given")) == NULL)
        return false;
    report->reason = verdict->reason = "";
    report->complete = true;
    memcpy(given, plan, sizeof(*given));
    c.hdrs = zcl_calloc(n + 1, sizeof(*c.hdrs), "facts_consumer.hdrs");
    c.ci = codeindex_open(c.root);
    ok = c.hdrs != NULL && fxc_universe(&c);
    if (ok && tus != NULL)
        ok = fxc_c_path(&c, tus, given, plan, verdict);
    else if (ok)
        ok = fxc_obligations(&c, given, plan, verdict);
    ok = ok && fxc_plain_count(&c, given);
    if (ok)
        fxc_reasons(&c, plan, verdict);
    fxc_free(&c);
    free(given);
    return ok;
}

void zcl_devloop_facts_report_free(struct zcl_devloop_facts_report *report)
{
    if (report == NULL)
        return;
    free(report->tus);
    report->tus = NULL;
    report->ntus = 0;
}

#if defined(ZCL_TESTING)
enum zcl_devloop_consumer_mutant zcl_devloop_test_consumer_mutant =
    ZCL_DEVLOOP_MUTANT_NONE;
#endif
