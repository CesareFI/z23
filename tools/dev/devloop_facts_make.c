/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The make half of the non-C/H build-input rule: which changed paths the makefiles name where make can change an object with them. */
#include "devloop_facts_make.h"

#include "util/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool fxm_put(struct fxm_buf *b, const char *s, size_t n)
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

bool fxm_ident(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-';
}

bool fxm_space(char ch)
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

const char *fxm_skip_prefixes(const char *p)
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

/* '%' in s[from..] is shell text: FXM_PCT. */
static void fxm_shell_text(struct fxm_buf *b, size_t from)
{
    for (size_t k = from; k < b->n; k++)
        if (b->p[k] == '%')
            b->p[k] = FXM_PCT;
}

/* The body of one reference: a function call yields its arguments ('%' in
 * a $(shell) argument is shell text), a path-like variable its value, an
 * automatic variable nothing, anything else FXM_ANY. */
static bool fxm_inner(const struct fxm *m, const char *s, size_t n,
                      struct fxm_buf *out)
{
    const char *v;
    size_t k = 0, at;
    char any = FXM_ANY;
    while (k < n && fxm_ident(s[k]))
        k++;
    if (k > 0 && k < n && fxm_space(s[k])) {
        if (!fxm_put(out, " ", 1))
            return false;
        at = out->n;
        if (!fxm_put(out, s + k + 1, n - k - 1))
            return false;
        if (k == 5 && strncmp(s, "shell", 5) == 0)
            fxm_shell_text(out, at);
        return fxm_put(out, " ", 1);
    }
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
 * characters, '/' included; '?' and a '[...]' set match any one character
 * (a superset of the set); a trailing '/' matches a whole directory. */
static bool fxm_run(char ch)
{
    return ch == '*' || ch == '%' || ch == '$' || ch == FXM_ANY;
}

static const char *fxm_one_end(const char *p)
{
    const char *e;
    if (*p == '?')
        return p + 1;
    return *p == '[' && (e = strchr(p + 1, ']')) != NULL ? e + 1 : NULL;
}

bool fxm_glob(const char *p, const char *s)
{
    const char *star = NULL, *back = NULL, *w;
    while (*s != '\0') {
        if (p[0] == '/' && p[1] == '\0' && *s == '/')
            return true;
        if (fxm_run(*p)) {
            star = ++p;
            back = s;
        } else if ((w = fxm_one_end(p)) != NULL || *p == *s) {
            p = w != NULL ? w : p + 1;
            s++;
        } else if (star != NULL) {
            p = star;
            s = ++back;
        } else {
            return false;
        }
    }
    while (fxm_run(*p))
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
    /* A directory mention with no trailing slash (the common recipe-argument
     * form: a tool is handed a bare directory and reads whatever is under
     * it) is just as much a directory as one written with a trailing '/':
     * t is an exact path-component prefix of path when the next byte of
     * path after t is '/'. */
    if (tn > 0 && strncmp(path, t, tn) == 0 && path[tn] == '/')
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

/* A word as make or the shell would use it: recipe prefixes and a leading
 * "./" dropped; NULL when it holds only references (their definitions are
 * scanned too). */
const char *fxm_word_of(const char *t)
{
    while (*t == '@' || *t == '-' || *t == '+')
        t++;
    if (t[0] == '.' && t[1] == '/')
        t += 2;
    return fxm_literal_char(t) ? t : NULL;
}

/* A glob word names path: the path, or a directory it lives under (with or
 * without a trailing '/'), each also under a root the glob's leading text
 * may spell ($(CURDIR)/tools/x.sh names tools/x.sh). Too long to test: it
 * does. */
static bool fxm_glob_named(const char *t, const char *path)
{
    char buf[4096 + 3];
    size_t n = strlen(path);
    if (n + 3 > sizeof(buf))
        return true;
    buf[0] = '/';
    memcpy(buf + 1, path, n + 1);
    for (size_t k = n + 1; k > 1; k--) {
        char at = buf[k], next = buf[k + 1];
        bool hit;
        if (at != '\0' && at != '/')
            continue;
        buf[k] = '\0';
        hit = fxm_glob(t, buf) || fxm_glob(t, buf + 1);
        buf[k] = at;
        if (!hit && at == '/') {
            buf[k + 1] = '\0';
            hit = fxm_glob(t, buf) || fxm_glob(t, buf + 1);
            buf[k + 1] = next;
        }
        if (hit)
            return true;
    }
    return false;
}

/* Mark every path asked about that t names. */
static bool fxm_token(struct fxm *m, const char *t)
{
    bool glob;
    if ((t = fxm_word_of(t)) == NULL)
        return false;
    glob = strpbrk(t, FXM_WILDS) != NULL;
    for (size_t k = 0; k < m->npaths; k++)
        if (m->want[k] && !m->make[k] &&
            (glob ? fxm_glob_named(t, m->paths[k]) : fxm_named(t, m->paths[k])))
            m->make[k] = true;
    return false;
}

static bool fxm_sep(char ch)
{
    return ch != '\0' && strchr(" \t\r\n,(){};|'\"`=<>&!:", ch) != NULL;
}

/* fn over every word of s; whether it held for any. */
bool fxm_tokens(struct fxm *m, char *s,
                bool (*fn)(struct fxm *, const char *))
{
    bool any = false;
    while (*s != '\0') {
        char *t, save;
        while (fxm_sep(*s))
            s++;
        for (t = s; *s != '\0' && !fxm_sep(*s); s++)
            ;
        save = *s;
        *s = '\0';
        if (*t != '\0')
            any |= fn(m, t);
        *s = save;
    }
    return any;
}

/* s[0..n) expanded FXM_ROUNDS deep, in one of m's buffers until the next
 * expansion; NULL (and UNKNOWN) when it cannot be held. */
const char *fxm_expand(struct fxm *m, const char *s, size_t n)
{
    struct fxm_buf *in = &m->a, *out = &m->b;
    bool left = true;
    in->n = 0;
    if (!fxm_put(in, s, n)) {
        m->unknown = true;
        return NULL;
    }
    for (int r = 0; left && r < FXM_ROUNDS; r++) {
        struct fxm_buf *t = in;
        if (!fxm_round(m, in->p, out, &left)) {
            m->unknown = true;
            return NULL;
        }
        in = out;
        out = t;
    }
    return in->p;
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

/* Every whitespace-separated word of an expanded include line's argument
 * text (already macro-expanded by fxm_include). */
static void fxm_include_words(struct fxm *m, char *s, bool optional)
{
    for (; !m->unknown && s != NULL && *s != '\0';) {
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
    if (!m->unknown)
        fxm_include_words(m, in->p, optional);
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

/* ---- make inputs: where each line sits ------------------------------------------ */

static bool fxm_grow(void **v, size_t *cap, size_t n, size_t size)
{
    size_t next;
    void *g;
    if (n < *cap)
        return true;
    next = *cap ? *cap * 2 : 256;
    g = zcl_realloc(*v, next * size, "facts_consumer.mkgrow");
    if (g == NULL)
        return false;
    *v = g;
    *cap = next;
    return true;
}

bool fxm_starts_word(const char *p, const char *w)
{
    size_t n = strlen(w);
    return strncmp(p, w, n) == 0 &&
           (p[n] == '\0' || fxm_space(p[n]) || p[n] == '(');
}

static bool fxm_cond_word(const char *p)
{
    return fxm_starts_word(p, "ifeq") || fxm_starts_word(p, "ifneq") ||
           fxm_starts_word(p, "ifdef") || fxm_starts_word(p, "ifndef") ||
           fxm_starts_word(p, "else") || fxm_starts_word(p, "endif");
}

/* A conditional directive: a later recipe line belongs to the rule before
 * the conditional, unless a branch changed it; then to no rule that can be
 * named. */
static void fxm_cond(struct fxm *m, struct fxm_place *st, const char *p)
{
    bool is_else = fxm_starts_word(p, "else");
    int d = st->depth - 1;
    if (!is_else && !fxm_starts_word(p, "endif")) {
        if (st->depth == FXM_COND_MAX) {
            m->unknown = true;
            return;
        }
        st->open[st->depth] = st->rule;
        st->moved[st->depth++] = false;
        return;
    }
    if (d < 0) {
        m->unknown = true;
        return;
    }
    st->moved[d] |= st->rule != st->open[d];
    if (is_else) {
        st->rule = st->open[d];
        return;
    }
    st->rule = st->moved[d] ? FXM_NONE : st->open[d];
    st->depth--;
}

/* The first character of s from set outside any reference; NULL for none. */
const char *fxm_top(const char *s, const char *set)
{
    int depth = 0;
    for (; *s != '\0'; s++) {
        if (*s == '$') {
            depth += s[1] == '(' || s[1] == '{';
            s += s[1] != '\0';
        } else if (depth > 0) {
            depth += (*s == '(' || *s == '{') - (*s == ')' || *s == '}');
        } else if (strchr(set, *s) != NULL) {
            return s;
        }
    }
    return NULL;
}


/* A definition (its operator first), a rule (*colon at its ':'), a
 * target-specific assignment, or anything else. */
enum fxm_kind fxm_kind_of(const char *s, size_t *colon)
{
    const char *p = fxm_top(s, ":="), *q;
    if (p == NULL)
        return FXM_K_OTHER;
    if (*p == '=' || p[1] == '=' || strncmp(p + 1, ":=", 2) == 0 ||
        strncmp(p + 1, "::=", 3) == 0)
        return FXM_K_DEF;
    *colon = (size_t)(p - s);
    q = fxm_top(p + 1, "=;");
    return q != NULL && *q == '=' ? FXM_K_TSV : FXM_K_RULE;
}

/* The variable p defines into out: "" when its name is computed or longer
 * than out holds. */
void fxm_def_name(const char *p, char *out)
{
    size_t n = 0;
    p = fxm_skip_prefixes(p);
    while (n + 1 < FXM_NAME_MAX && fxm_ident(p[n]))
        n++;
    if (fxm_ident(p[n]) || p[n] == '$')
        n = 0;
    memcpy(out, p, n);
    out[n] = '\0';
}

bool fxm_all_space(const char *s, size_t n)
{
    for (size_t k = 0; k < n; k++)
        if (!fxm_space(s[k]))
            return false;
    return true;
}

/* w[0..n) holds no wildcard, reference or unknown. */
bool fxm_literal_span(const char *w, size_t n)
{
    for (size_t k = 0; k < n; k++)
        if (strchr(FXM_WILDS "\x02", w[k]) != NULL)
            return false;
    return n > 0;
}

/* .PHONY's literal names. */
static void fxm_phony_words(struct fxm *m, const char *s)
{
    char w[FXM_NAME_MAX];
    while (*s != '\0' && !m->unknown) {
        size_t n = 0;
        while (fxm_space(*s))
            s++;
        while (s[n] != '\0' && !fxm_space(s[n]))
            n++;
        if (n > 0 && n < sizeof(w) && fxm_literal_span(s, n)) {
            memcpy(w, s, n);
            w[n] = '\0';
            m->unknown |= !fxc_strs_add(&m->phony, w);
        }
        s += n;
    }
}

/* s[0..n) (a rule's targets) is .PHONY. */
static bool fxm_phony_head(const char *s, size_t n)
{
    const char *p = s;
    while (fxm_space(*p))
        p++;
    return strncmp(p, ".PHONY", 6) == 0 &&
           fxm_all_space(p + 6, (size_t)(s + n - (p + 6)));
}

/* A line a definition holds: text a later $(eval) may read as a .PHONY
 * line (it reaches nothing) or a rule (its targets reach nothing). */
static uint8_t fxm_define_line(struct fxm *m, struct fxm_place *st,
                               const char *s, bool recipe)
{
    size_t colon = 0;
    enum fxm_kind kind;
    st->in_define = !fxm_starts_word(fxm_skip_prefixes(s), "endef");
    if (!st->in_define)
        return FXM_ACTIVE;
    memcpy(m->name, st->define_name, sizeof(m->name));
    if (recipe)
        return FXM_DEF;
    kind = fxm_kind_of(s, &colon);
    if (kind == FXM_K_RULE && fxm_phony_head(s, colon))
        return FXM_QUIET;
    if (kind == FXM_K_RULE || kind == FXM_K_TSV)
        m->from = colon + 1;
    return FXM_DEF;
}

/* A rule line: .PHONY's names, or a new rule with its expanded targets. */
static uint8_t fxm_rule(struct fxm *m, struct fxm_place *st, const char *s,
                        size_t colon)
{
    size_t n = colon > 0 && s[colon - 1] == '&' ? colon - 1 : colon;
    const char *t;
    struct fxm_rule *r;
    if (fxm_phony_head(s, n)) {
        if ((t = fxm_expand(m, s + colon + 1, strlen(s + colon + 1))) != NULL)
            fxm_phony_words(m, t);
        return FXM_PHONY;
    }
    if ((t = fxm_expand(m, s, n)) == NULL || m->nrules >= FXM_NONE ||
        !fxm_grow((void **)&m->rules, &m->caprules, m->nrules, sizeof(*r))) {
        m->unknown = true;
        return FXM_ACTIVE;
    }
    r = &m->rules[m->nrules];
    while (fxm_space(*t))
        t++;
    /* A makefile's first rule not led by '.' is the goal of a bare make. */
    r->reached = !st->goal && *t != '.';
    st->goal |= r->reached;
    if ((r->targets = zcl_strdup(t, "facts_consumer.mktgt")) == NULL) {
        m->unknown = true;
        return FXM_ACTIVE;
    }
    st->rule = (uint32_t)m->nrules++;
    return FXM_RULE;
}

static uint8_t fxm_define(struct fxm_place *st, const char *p)
{
    fxm_def_name(p, st->define_name);
    st->in_define = true;
    return FXM_QUIET; /* its body lines are the definition */
}

/* The context of the logical line in m->line (a tab line when recipe); a
 * definition's variable in m->name, and where what the line reaches with
 * starts in m->from. */
static uint8_t fxm_place_line(struct fxm *m, struct fxm_place *st, bool recipe)
{
    const char *s = m->line.p, *p = fxm_skip_prefixes(s);
    size_t colon = 0;
    enum fxm_kind kind;
    m->from = 0;
    m->whole |= strstr(s, ".ONESHELL") != NULL ||
                strstr(s, ".RECIPEPREFIX") != NULL;
    m->second |= strstr(s, ".SECONDEXPANSION") != NULL;
    m->body = st->in_define;
    if (st->in_define)
        return fxm_define_line(m, st, s, recipe);
    if (recipe && st->rule != FXM_NONE)
        return FXM_RECIPE;
    /* A tab line outside a rule is an ordinary line to make too. */
    if (*p == '\0')
        return FXM_ACTIVE; /* blank: the rule goes on */
    if (fxm_cond_word(p)) {
        fxm_cond(m, st, p);
        return FXM_QUIET;
    }
    st->rule = FXM_NONE;
    if (fxm_starts_word(p, "define"))
        return fxm_define(st, p + 6);
    kind = fxm_kind_of(s, &colon);
    if (kind == FXM_K_OTHER)
        return FXM_ACTIVE;
    m->from = kind == FXM_K_DEF ? 0 : colon + 1;
    if (kind == FXM_K_RULE)
        return fxm_rule(m, st, s, colon);
    /* A target-specific value is a definition of its variable. */
    fxm_def_name(s + m->from, m->name);
    return FXM_DEF;
}

/* Keep the line as written and expanded ('%' as shell text in a recipe). */
static void fxm_keep_line(struct fxm *m, uint8_t ctx, uint32_t rule)
{
    struct fxm_line *l;
    const char *t = fxm_expand(m, m->line.p, strlen(m->line.p));
    if (t == NULL ||
        !fxm_grow((void **)&m->lines, &m->caplines, m->nlines, sizeof(*l))) {
        m->unknown = true;
        return;
    }
    l = &m->lines[m->nlines];
    memset(l, 0, sizeof(*l));
    l->ctx = ctx;
    l->rule = rule;
    l->from = m->from < FXM_LINE_MAX ? (uint32_t)m->from : 0;
    l->body = m->body && ctx == FXM_DEF;
    l->text = zcl_strdup(t, "facts_consumer.mkline");
    l->raw = zcl_strdup(m->line.p, "facts_consumer.mkraw");
    l->name = zcl_strdup(ctx == FXM_DEF ? m->name : "", "facts_consumer.mkname");
    if (l->text == NULL || l->raw == NULL || l->name == NULL) {
        free(l->text);
        free(l->raw);
        free(l->name);
        m->unknown = true;
        return;
    }
    if (ctx == FXM_RECIPE)
        for (char *c = l->text; *c != '\0'; c++)
            *c = *c == '%' ? FXM_PCT : *c;
    m->nlines++;
}

/* Every logical line of one makefile but its whole-line comments. */
static void fxm_lines_of(struct fxm *m, size_t f)
{
    struct fxm_place st = {.rule = FXM_NONE};
    const char *at = m->text[f];
    bool recipe, comment;
    int r;
    while (!m->unknown &&
           (r = fxm_next_line(&m->line, &at, &recipe, &comment)) > 0) {
        uint8_t ctx;
        if (comment)
            continue;
        ctx = fxm_place_line(m, &st, recipe);
        if (!m->unknown)
            fxm_keep_line(m, ctx,
                          ctx == FXM_RULE || ctx == FXM_RECIPE ? st.rule
                                                               : FXM_NONE);
    }
    m->unknown |= r < 0;
}

/* ---- make inputs: variable references ------------------------------------------- */

static void fxm_free(struct fxm *m)
{
    for (size_t k = 0; k < m->files.n; k++)
        free(m->text[k]);
    for (size_t k = 0; k < m->nvars; k++) {
        free(m->vars[k].name);
        free(m->vars[k].value);
    }
    for (size_t k = 0; k < m->nlines; k++) {
        free(m->lines[k].text);
        free(m->lines[k].raw);
        free(m->lines[k].name);
    }
    for (size_t k = 0; k < m->nrules; k++)
        free(m->rules[k].targets);
    free(m->vars);
    free(m->lines);
    free(m->rules);
    free(m->pairs);
    free(m->vnames);
    fxc_strs_free(&m->files);
    fxc_strs_free(&m->phony);
    fxc_strs_free(&m->makers);
    free(m->line.p);
    free(m->a.p);
    free(m->b.p);
}

static bool fxm_ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), k = strlen(suffix);
    return n >= k && strcmp(s + n - k, suffix) == 0;
}

static bool fxm_makefile_name(const char *path)
{
    const char *b = strrchr(path, '/');
    b = b != NULL ? b + 1 : path;
    return strcmp(b, "GNUmakefile") == 0 || strcmp(b, "makefile") == 0 ||
           strncmp(b, "Makefile", 8) == 0 || fxm_ends_with(b, ".mk") ||
           fxm_ends_with(b, ".make");
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

void fxm_classify(const char *root, const char *const *paths, const bool *want,
                  bool *make, size_t n)
{
    struct fxm m = {.root = root, .paths = paths, .want = want, .make = make,
                    .npaths = n};
    for (size_t k = 0; k < n; k++)
        make[k] = want[k] && (fxm_makefile_name(paths[k]) ||
                              fxm_submake(root, paths[k]));
    fxm_read_all(&m);
    for (size_t f = 0; !m.unknown && f < m.files.n; f++)
        fxm_lines_of(&m, f);
    if (!m.unknown)
        fxm_makers(&m);
    if (!m.unknown)
        fxm_reach(&m);
    for (size_t k = 0; !m.unknown && k < m.nlines; k++)
        if (fxm_live(&m, &m.lines[k]))
            (void)fxm_tokens(&m, m.lines[k].text, fxm_token);
    for (size_t k = 0; k < n; k++)
        make[k] |= want[k] && (m.unknown || fxc_strs_has(&m.files, paths[k]));
    fxm_free(&m);
}
