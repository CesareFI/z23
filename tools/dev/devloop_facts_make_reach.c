/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The reach half of the facts consumer's make reader: which rules can run while an object is built, and which variables their lines use. */
#include "devloop_facts_make.h"

#include "util/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fxm_str_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void fxm_strs_seal(struct fxc_strs *s)
{
    size_t w = 0;
    qsort(s->v, s->n, sizeof(*s->v), fxm_str_cmp);
    for (size_t k = 0; k < s->n; k++) {
        if (w > 0 && strcmp(s->v[w - 1], s->v[k]) == 0) {
            free(s->v[k]);
            continue;
        }
        s->v[w++] = s->v[k];
    }
    s->n = w;
}

/* name[0..n) is in the sealed set. */
static bool fxm_in(const struct fxc_strs *s, const char *name, size_t n)
{
    size_t lo = 0, hi = s->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int d = strncmp(s->v[mid], name, n);
        if (d == 0 && s->v[mid][n] != '\0')
            d = 1;
        if (d == 0)
            return true;
        if (d < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return false;
}

/* The variable the reference at d names ($X, $(X...), ${X...}, and
 * $(call X,...) or $(value X)): its length, with its start at *name; 0 for
 * none. A name that starts with '$' is computed. '$$' is not skipped: a
 * definition eval'd later references what it spells after one expansion. */
static size_t fxm_ref_name(const char *d, const char **name)
{
    const char *q = d + 1;
    size_t n = 0;
    if (*q != '(' && *q != '{') {
        *name = q;
        return fxm_ident(*q) ? 1 : 0;
    }
    q++;
    while (fxm_ident(q[n]))
        n++;
    if (((n == 4 && strncmp(q, "call", 4) == 0) ||
         (n == 5 && strncmp(q, "value", 5) == 0)) &&
        fxm_space(q[n])) {
        for (q += n; fxm_space(*q); q++)
            ;
        for (n = 0; fxm_ident(q[n]); n++)
            ;
    }
    *name = q;
    return n > 0 ? n : *q == '$';
}

/* fn over every variable s references; whether it held for any. With
 * twice, '$$(X)' references X too (text a later $(eval) or $(call) expands
 * again); without it '$$' is an escaped '$' (a recipe's shell text). */
static bool fxm_each_ref(struct fxm *m, const char *s, bool twice,
                         bool (*fn)(struct fxm *, const char *, size_t))
{
    bool any = false;
    for (const char *d = strchr(s, '$'); d != NULL; d = strchr(d + 1, '$')) {
        const char *name;
        size_t n;
        if (!twice && d[1] == '$') {
            d++;
            continue;
        }
        if ((n = fxm_ref_name(d, &name)) > 0)
            any |= fn(m, name, n);
    }
    return any;
}

/* ---- make inputs: the recipe lines that can run make --------------------------- */

/* The glob a computed name ($($(X)_Y)) spells: each inner reference any
 * text, into pat. False when it names no variable by its spelling: one
 * inner reference alone ($($(X)), a name held in a value, which the words
 * of that value spell) or a space outside its references (no variable name
 * holds one). Too long to spell: any variable. */
static bool fxm_name_glob(const char *name, char *pat)
{
    size_t n = 0;
    int depth = 0;
    bool literal = false;
    for (const char *p = name; *p != '\0'; p++) {
        if (n + 2 >= FXM_NAME_MAX) {
            memcpy(pat, "\x01", 2);
            return true;
        }
        if (*p == '$' && (p[1] == '(' || p[1] == '{')) {
            if (depth++ == 0)
                pat[n++] = FXM_ANY;
            p++;
        } else if (depth > 0) {
            depth += (*p == '(' || *p == '{') - (*p == ')' || *p == '}');
        } else if (fxm_ident(*p)) {
            pat[n++] = *p;
            literal = true;
        } else if (fxm_space(*p)) {
            return false;
        } else {
            break;
        }
    }
    pat[n] = '\0';
    return literal;
}

/* Text that may run make by itself: the command make in any case ($(MAKE),
 * make, gmake, cmake; not MAKEFLAGS or Makefile), $(eval) or $(file). */
static bool fxm_direct_maker(const char *s)
{
    static const char *const marks[] = {"$(eval", "${eval", "$(file",
                                        "${file"};
    for (const char *p = s; *p != '\0'; p++)
        if ((p[0] | 0x20) == 'm' && (p[1] | 0x20) == 'a' &&
            (p[2] | 0x20) == 'k' && (p[3] | 0x20) == 'e' && !fxm_ident(p[4]))
            return true;
    for (size_t k = 0; k < sizeof(marks) / sizeof(marks[0]); k++)
        if (strstr(s, marks[k]) != NULL)
            return true;
    return false;
}

/* A maker a glob matches. */
static bool fxm_maker_glob(const struct fxm *m, const char *pat)
{
    for (size_t k = 0; k < m->makers.n; k++)
        if (fxm_glob(pat, m->makers.v[k]))
            return true;
    return false;
}

/* A reference to a variable that may run make. */
static bool fxm_is_maker(struct fxm *m, const char *name, size_t n)
{
    char pat[FXM_NAME_MAX];
    if (*name != '$')
        return fxm_in(&m->makers, name, n);
    return fxm_name_glob(name, pat) && fxm_maker_glob(m, pat);
}

/* A word that spells a variable that may run make (a call argument, a
 * name held in a value). */
static bool fxm_spells_maker(struct fxm *m, const char *t)
{
    if ((t = fxm_word_of(t)) == NULL)
        return false;
    if (strpbrk(t, FXM_WILDS) != NULL)
        return fxm_maker_glob(m, t);
    return fxm_in(&m->makers, t, strlen(t));
}

static int fxm_vname_cmp(const void *a, const void *b)
{
    return strcmp(((const struct fxm_vname *)a)->name,
                  ((const struct fxm_vname *)b)->name);
}

/* Every defined variable, sorted; make's own hooks (.EXTRA_PREREQS adds
 * prerequisites to every rule, VPATH and GPATH resolve them) are live. */
static void fxm_vnames(struct fxm *m)
{
    static const char *const hooks[] = {".EXTRA_PREREQS", ".DEFAULT_GOAL",
                                        "VPATH", "GPATH",
                                        "MAKEFILES", ".LIBPATTERNS"};
    size_t w = 0;
    m->vnames = zcl_calloc(m->nlines + 1, sizeof(*m->vnames), "facts_consumer.mkvn");
    if (m->vnames == NULL) {
        m->unknown = true;
        return;
    }
    for (size_t k = 0; k < m->nlines; k++)
        if (m->lines[k].ctx == FXM_DEF && m->lines[k].name[0] != '\0')
            m->vnames[m->nvnames++].name = m->lines[k].name;
    qsort(m->vnames, m->nvnames, sizeof(*m->vnames), fxm_vname_cmp);
    for (size_t k = 0; k < m->nvnames; k++)
        if (w == 0 || strcmp(m->vnames[w - 1].name, m->vnames[k].name) != 0)
            m->vnames[w++] = m->vnames[k];
    m->nvnames = w;
    for (size_t k = 0; k < m->nvnames; k++)
        for (size_t h = 0; h < sizeof(hooks) / sizeof(hooks[0]); h++)
            m->vnames[k].live |= strcmp(m->vnames[k].name, hooks[h]) == 0;
}

static struct fxm_vname *fxm_vname(struct fxm *m, const char *name, size_t n)
{
    size_t lo = 0, hi = m->nvnames;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int d = strncmp(m->vnames[mid].name, name, n);
        if (d == 0 && m->vnames[mid].name[n] != '\0')
            d = 1;
        if (d == 0)
            return &m->vnames[mid];
        if (d < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/* ---- make inputs: recipe commands that only print ------------------------------ */

/* Characters a value may carry into a recipe as shell syntax. */
#define FXM_SHELL_MARKS "\"'`;|&<>!\\"

/* Value text that may carry shell syntax, a command or make into a recipe:
 * a mark, a command substitution, make, or a call whose value is not the
 * text it is given. */
static bool fxm_shelly_text(const char *s)
{
    static const char *const fns[] = {"shell", "file", "eval", "wildcard",
                                       "realpath"};
    if (strpbrk(s, FXM_SHELL_MARKS) != NULL || fxm_direct_maker(s))
        return true;
    for (const char *d = strchr(s, '$'); d != NULL; d = strchr(d + 1, '$')) {
        if (d[1] == '$' && (d[2] == '(' || d[2] == '{'))
            return true;
        for (size_t k = 0; k < sizeof(fns) / sizeof(fns[0]); k++)
            if ((d[1] == '(' || d[1] == '{') && fxm_starts_word(d + 2, fns[k]))
                return true;
    }
    return false;
}

/* A reference that may carry shell syntax: a computed name, a name no
 * definition gives (an argument, the environment), or a marked variable. */
static bool fxm_ref_shelly(struct fxm *m, const char *name, size_t n)
{
    struct fxm_vname *v;
    return *name == '$' || (v = fxm_vname(m, name, n)) == NULL || v->shelly;
}
/* Mark every variable the computed name `at` starts with (up to its
 * operator) may spell; whether one was new. A name with a character a
 * reference cannot spell outside its references ('@', ';', a space) is
 * never read as plain text, so it marks nothing; one that spells no name
 * of its own ($($(X)) :=) marks every variable. */
static bool fxm_shelly_glob(struct fxm *m, const char *at)
{
    char pat[FXM_NAME_MAX], spelled[FXM_NAME_MAX];
    const char *op = fxm_top(at, ":+?!=");
    size_t n = op == NULL ? 0 : (size_t)(op - at);
    bool grew = false;
    int depth = 0;
    while (n > 0 && fxm_space(at[n - 1]))
        n--;
    for (size_t k = 0; k < n && k + 1 < FXM_NAME_MAX; k++) {
        if (at[k] == '$' && (at[k + 1] == '(' || at[k + 1] == '{'))
            depth++;
        else if (depth > 0)
            depth += (at[k] == '(' || at[k] == '{') - (at[k] == ')' || at[k] == '}');
        else if (!fxm_ident(at[k]))
            return false;
    }
    memcpy(spelled, at, n < FXM_NAME_MAX ? n : 0);
    spelled[n < FXM_NAME_MAX ? n : 0] = '\0';
    m->shelly_any |= n == 0 || n >= FXM_NAME_MAX || !fxm_name_glob(spelled, pat);
    for (size_t k = 0; !m->shelly_any && k < m->nvnames; k++)
        if (!m->vnames[k].shelly && fxm_glob(pat, m->vnames[k].name))
            m->vnames[k].shelly = grew = true;
    return grew;
}

/* Mark the variable name, or (computed) what `at` may spell; whether one
 * was new. */
static bool fxm_shelly_name(struct fxm *m, const char *name, const char *at)
{
    struct fxm_vname *v;
    bool grew;
    if (*name == '\0')
        return fxm_shelly_glob(m, at);
    v = fxm_vname(m, name, strlen(name));
    grew = v != NULL && !v->shelly;
    if (v != NULL)
        v->shelly = true;
    return grew;
}
/* One definition line's marks: a define's own variable (its value spans
 * lines), and the variable the line defines when make or $(eval) reads it
 * when its value may carry shell syntax. */
static bool fxm_shelly_line(struct fxm *m, const struct fxm_line *l)
{
    char name[FXM_NAME_MAX];
    size_t colon = 0;
    enum fxm_kind kind = fxm_kind_of(l->raw, &colon);
    const char *at, *eq;
    bool grew = l->body && fxm_shelly_name(m, l->name, "");
    if (kind != FXM_K_DEF && kind != FXM_K_TSV)
        return grew;
    at = fxm_skip_prefixes(l->raw + (kind == FXM_K_TSV ? colon + 1 : 0));
    fxm_def_name(at, name);
    eq = fxm_top(at, "=");
    if (eq != NULL && eq > at && eq[-1] != '!' && !fxm_shelly_text(eq + 1) &&
        !fxm_each_ref(m, eq + 1, true, fxm_ref_shelly))
        return grew;
    return fxm_shelly_name(m, name, at) || grew;
}

/* Mark every variable a definition of which may carry shell syntax into a
 * recipe, closed over references. */
static void fxm_shelly(struct fxm *m)
{
    for (bool grew = true; grew && !m->shelly_any;) {
        grew = false;
        for (size_t k = 0; k < m->nlines && !m->shelly_any; k++)
            if (m->lines[k].ctx == FXM_DEF)
                grew |= fxm_shelly_line(m, &m->lines[k]);
    }
}

/* The bracket that closes the reference at d ("$(" or "${"); NULL for none. */
static char *fxm_ref_end(char *d)
{
    int depth = 0;
    for (char *p = d + 1; *p != '\0'; p++)
        if ((depth += (*p == '(' || *p == '{') - (*p == ')' || *p == '}')) == 0)
            return p;
    return NULL;
}

/* p..e (e its last character) names one of make's automatic variables. */
static bool fxm_auto_var(const char *p, const char *e)
{
    return *p != '\0' && strchr("@<*^+?", *p) != NULL &&
           (e == p || (e == p + 1 && (p[1] == 'D' || p[1] == 'F')));
}

static bool fxm_plain_name(struct fxm *m, const char *name, size_t n)
{
    struct fxm_vname *v = fxm_vname(m, name, n);
    return !m->shelly_any && v != NULL && !v->shelly;
}

/* The reference d..e (e its last character) puts no shell syntax into a
 * command: an automatic variable, or $X, $(X) or ${X} of a variable every
 * definition of which is plain text. */
static bool fxm_plain_ref(struct fxm *m, char *d, const char *e)
{
    const char *name;
    size_t n;
    if (e == d + 1)
        return fxm_auto_var(d + 1, e) ||
               (fxm_ident(d[1]) && fxm_plain_name(m, d + 1, 1));
    if (fxm_auto_var(d + 2, e - 1))
        return true;
    n = fxm_ref_name(d, &name);
    return n > 0 && name == d + 2 && name + n == e && fxm_plain_name(m, name, n);
}

/* The end of the shell command at p: the first ';', '&' or '|' outside
 * quotes, references and redirections; NULL when one does not close. */
static char *fxm_command_end(char *p)
{
    char quote = '\0';
    for (const char *s = p; *p != '\0'; p++) {
        if (p[0] == '$' && p[1] == '$')
            p++;
        else if (p[0] == '$' && (p[1] == '(' || p[1] == '{')) {
            if ((p = fxm_ref_end(p)) == NULL)
                return NULL;
        } else if (*p == '\\' && quote != '\'' && p[1] != '\0')
            p++;
        else if (quote != '\0')
            quote = *p == quote ? '\0' : quote;
        else if (*p == '\'' || *p == '"')
            quote = *p;
        else if (*p == '&' && (p[1] == '>' || (p > s && (p[-1] == '<' || p[-1] == '>'))))
            continue;
        else if (*p == ';' || *p == '&' || *p == '|')
            return p;
    }
    return quote == '\0' ? p : NULL;
}

/* After '>': a redirection that feeds no command and names no file. */
static bool fxm_quiet_redirect(const char *p)
{
    while (fxm_space(*p))
        p++;
    return (p[0] == '&' && p[1] >= '0' && p[1] <= '9') ||
           strncmp(p, "/dev/null", 9) == 0;
}

/* Past the @, -, +, (, {, !, then, else, do and elif that lead a command. */
static char *fxm_command_word(char *p, const char *e)
{
    static const char *const kw[] = {"then", "else", "do", "elif"};
    for (bool more = true; more;) {
        while (p < e && (fxm_space(*p) || strchr("@-+({!", *p) != NULL))
            p++;
        more = false;
        for (size_t k = 0; k < sizeof(kw) / sizeof(kw[0]); k++)
            if (fxm_starts_word(p, kw[k])) {
                p += strlen(kw[k]);
                more = true;
            }
    }
    return p;
}

/* p..e is one echo or printf that runs nothing: no command substitution,
 * no redirection but to a descriptor or /dev/null, only plain references. */
static bool fxm_prints_only(struct fxm *m, char *p, const char *e)
{
    p = fxm_command_word(p, e);
    if (!fxm_starts_word(p, "echo") && !fxm_starts_word(p, "printf"))
        return false;
    for (char *r; p < e; p++) {
        if (p[0] == '$' && p[1] == '$') {
            if (p[2] == '(' || p[2] == '{')
                return false;
            p++;
        } else if (p[0] == '$') {
            r = p[1] == '(' || p[1] == '{' ? fxm_ref_end(p) : p + 1;
            if (r == NULL || r >= e || !fxm_plain_ref(m, p, r))
                return false;
            p = r;
        } else if (*p == '`' || *p == '<' ||
                   (*p == '>' && !fxm_quiet_redirect(p + 1))) {
            return false;
        }
    }
    return true;
}

/* Blank in a recipe line each command that only prints: what it prints is
 * text, never a goal make runs. One piped into another command stays. */
static void fxm_quiet_recipe(struct fxm *m, char *s)
{
    for (char *p = s, *e; *p != '\0'; p = e + 1) {
        if ((e = fxm_command_end(p)) == NULL)
            return;
        if (!(*e == '|' && e[1] != '|') && fxm_prints_only(m, p, e))
            memset(p, ' ', (size_t)(e - p));
        if (*e == '\0')
            return;
    }
}

/* The end of the argument that starts at p (a top-level ',' or the call's
 * closing bracket), or NULL when the call does not close. */
static char *fxm_arg_end(char *p, char close)
{
    int depth = 0;
    for (; *p != '\0'; p++) {
        if (*p == '(' || *p == '{')
            depth++;
        else if ((*p == ')' || *p == '}') && depth-- == 0)
            return *p == close ? p : NULL;
        else if (*p == ',' && depth == 0)
            return p;
    }
    return NULL;
}

/* A call that runs something as it is expanded: its text is not only a
 * value. */
static bool fxm_running_call(const char *name)
{
    static const char *const fns[] = {"shell", "eval", "file", "call", "value"};
    for (size_t k = 0; k < sizeof(fns) / sizeof(fns[0]); k++)
        if (fxm_starts_word(name, fns[k]))
            return true;
    return false;
}

/* Blank p..e, text whose value is only tested or printed, but for what
 * runs as it is expanded: a reference to a variable that may run make and
 * a call that runs something stay whole; any other call is blanked around
 * its arguments, which are blanked the same way. Text holding '$$' (a
 * reference a later expansion makes) stays as it is. */
static void fxm_quiet_value(struct fxm *m, char *p, char *e, int depth)
{
    for (char *r, *q; p < e && depth < FXM_ROUNDS;) {
        if (p[0] == '$' && p[1] == '$')
            return;
        if (p[0] == '$' && fxm_ident(p[1])) {
            p += fxm_is_maker(m, p + 1, 1) ? 2 : 0;
            if (p[0] == '$')
                memset(p, ' ', 2), p += 2;
            continue;
        }
        if (p[0] != '$' || (p[1] != '(' && p[1] != '{')) {
            *p++ = ' ';
            continue;
        }
        if ((r = fxm_ref_end(p)) == NULL || r >= e)
            return;
        for (q = p + 2; fxm_ident(*q); q++)
            ;
        if (q == r && !fxm_is_maker(m, p + 2, (size_t)(q - p - 2))) {
            memset(p, ' ', (size_t)(r + 1 - p));
        } else if (q > p + 2 && fxm_space(*q) && !fxm_running_call(p + 2)) {
            memset(p, ' ', (size_t)(q - p));
            *r = ' ';
            fxm_quiet_value(m, q, r, depth + 1);
        }
        p = r + 1;
    }
}

/* Blank what a call can never put in its value: $(if)'s condition,
 * $(foreach)'s variable name, $(filter)'s and $(filter-out)'s patterns,
 * and all of $(error), $(info), $(warning), $(origin) and $(flavor), but
 * for what runs as it is expanded. A word there cannot become a
 * prerequisite or a goal. */
static void fxm_blank(struct fxm *m, char *s)
{
    static const char *const first[] = {"if", "filter", "filter-out", "foreach"};
    static const char *const whole[] = {"error", "info", "warning", "origin",
                                        "flavor"};
    for (char *d = strchr(s, '$'); d != NULL; d = strchr(d + 1, '$')) {
        char close = d[1] == '(' ? ')' : '}', *a, *e;
        bool one = false, all = false;
        if (d[1] != '(' && d[1] != '{')
            continue;
        for (size_t k = 0; k < sizeof(first) / sizeof(first[0]); k++)
            one |= fxm_starts_word(d + 2, first[k]) && d[2 + strlen(first[k])] != '(';
        for (size_t k = 0; k < sizeof(whole) / sizeof(whole[0]); k++)
            all |= fxm_starts_word(d + 2, whole[k]) && d[2 + strlen(whole[k])] != '(';
        if (!one && !all)
            continue;
        for (a = d + 2; *a != '\0' && !fxm_space(*a); a++)
            ;
        e = fxm_arg_end(a, close);
        while (all && e != NULL && *e == ',')
            e = fxm_arg_end(e + 1, close);
        if (e != NULL)
            fxm_quiet_value(m, a, e, 0);
    }
}

/* What a line says from its offset on, less what its calls cannot put in
 * their values and (a recipe line) its commands that only print, into
 * m->line; expanded, '%' as shell text in a recipe. NULL when it cannot be
 * held (then the text is UNKNOWN). */
static char *fxm_said(struct fxm *m, const struct fxm_line *l)
{
    char *t;
    m->line.n = 0;
    if (!fxm_put(&m->line, l->raw + l->from, strlen(l->raw + l->from))) {
        m->unknown = true;
        return NULL;
    }
    fxm_blank(m, m->line.p);
    if (l->ctx == FXM_RECIPE)
        fxm_quiet_recipe(m, m->line.p);
    t = (char *)fxm_expand(m, m->line.p, m->line.n);
    for (char *c = t; c != NULL && *c != '\0' && l->ctx == FXM_RECIPE; c++)
        *c = *c == '%' ? FXM_PCT : *c;
    return t;
}

/* The line may run make: by itself, or through a variable it references or
 * spells. A recipe line's commands that only print run nothing. */
static bool fxm_line_makes(struct fxm *m, struct fxm_line *l)
{
    const char *raw = l->raw;
    char *text = l->text;
    if (l->ctx == FXM_RECIPE) {
        if ((text = fxm_said(m, l)) == NULL)
            return true;
        raw = m->line.p;
    }
    return fxm_direct_maker(raw) ||
           fxm_each_ref(m, raw, l->ctx != FXM_RECIPE, fxm_is_maker) ||
           fxm_tokens(m, text, fxm_spells_maker);
}

/* The definition's variable may run make: false when it already did. */
static bool fxm_maker_add(struct fxm *m, const char *name)
{
    if (*name == '\0') {
        m->whole = true; /* a computed variable that may run make */
        return false;
    }
    if (fxm_in(&m->makers, name, strlen(name)))
        return false;
    if (!fxc_strs_add(&m->makers, name)) {
        m->unknown = true;
        return false;
    }
    fxm_strs_seal(&m->makers);
    return true;
}

/* The defined variables and those that may carry shell syntax; the
 * variables whose value may run make, closed over references and
 * spellings; then each recipe line's own verdict. */
void fxm_makers(struct fxm *m)
{
    bool grew = true;
    fxm_vnames(m);
    if (!m->unknown)
        fxm_shelly(m);
    while (grew && !m->unknown) {
        grew = false;
        for (size_t k = 0; k < m->nlines; k++)
            if (m->lines[k].ctx == FXM_DEF &&
                !fxm_in(&m->makers, m->lines[k].name, strlen(m->lines[k].name)) &&
                fxm_line_makes(m, &m->lines[k]))
                grew |= fxm_maker_add(m, m->lines[k].name);
    }
    for (size_t k = 0; k < m->nlines; k++)
        if (m->lines[k].ctx == FXM_RECIPE)
            m->lines[k].runs_make = fxm_line_makes(m, &m->lines[k]);
}

/* ---- make inputs: the rules that can change an object -------------------------- */

static int fxm_pair_cmp(const void *a, const void *b)
{
    return strcmp(((const struct fxm_pair *)a)->name,
                  ((const struct fxm_pair *)b)->name);
}

/* A rule every target of which is a literal .PHONY name waits to be
 * reached, its words (NUL-ended in place when add) joining the pairs; any
 * other rule is reached. */
static bool fxm_rule_words(struct fxm *m, uint32_t r, bool add)
{
    char *s = m->rules[r].targets;
    size_t words = 0;
    while (*s != '\0') {
        char *w = s;
        size_t n;
        while (fxm_space(*w))
            w++;
        for (s = w; *s != '\0' && !fxm_space(*s); s++)
            ;
        if ((n = (size_t)(s - w)) == 0)
            continue;
        if (!add && (!fxm_literal_span(w, n) || !fxm_in(&m->phony, w, n)))
            return false;
        if (add) {
            s += *s != '\0';
            w[n] = '\0';
            m->pairs[m->npairs++] = (struct fxm_pair){.name = w, .rule = r};
        }
        words++;
    }
    return words > 0;
}

static void fxm_pairs(struct fxm *m)
{
    size_t cap = 0;
    for (size_t k = 0; k < m->nrules; k++)
        cap += strlen(m->rules[k].targets) / 2 + 1;
    m->pairs = zcl_calloc(cap + 1, sizeof(*m->pairs), "facts_consumer.mkpairs");
    if (m->pairs == NULL) {
        m->unknown = true;
        return;
    }
    for (uint32_t r = 0; r < m->nrules; r++) {
        m->rules[r].reached |= !fxm_rule_words(m, r, false);
        if (!m->rules[r].reached)
            (void)fxm_rule_words(m, r, true);
    }
    qsort(m->pairs, m->npairs, sizeof(*m->pairs), fxm_pair_cmp);
}

/* A pair's name against name[0..len): <0, 0 or >0 as strcmp would say. */
static int fxm_pair_key(const struct fxm_pair *p, const char *name, size_t len)
{
    int d = strncmp(p->name, name, len);
    return d != 0 ? d : p->name[len] != '\0';
}

/* Reach every rule the .PHONY name name[0..len) belongs to. */
static bool fxm_reach_name(struct fxm *m, const char *name, size_t len)
{
    size_t lo = 0, hi = m->npairs;
    bool grew = false;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (fxm_pair_key(&m->pairs[mid], name, len) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    for (; lo < m->npairs && fxm_pair_key(&m->pairs[lo], name, len) == 0; lo++) {
        grew |= !m->rules[m->pairs[lo].rule].reached;
        m->rules[m->pairs[lo].rule].reached = true;
    }
    return grew;
}

/* The .PHONY name a word is (make matches a target by its exact name: a
 * word led by '-', '@' or '+' names only a target spelled so), or
 * every one a glob word matches: reach its rules. */
static bool fxm_token_phony(struct fxm *m, const char *t)
{
    bool grew = false;
    if (*t != '-' && *t != '@' && *t != '+' && (t = fxm_word_of(t)) == NULL)
        return false;
    if (strpbrk(t, FXM_WILDS) == NULL)
        return fxm_reach_name(m, t, strlen(t));
    for (size_t k = 0; k < m->npairs; k++)
        if (!m->rules[m->pairs[k].rule].reached && fxm_glob(t, m->pairs[k].name))
            grew |= fxm_reach_name(m, m->pairs[k].name, strlen(m->pairs[k].name));
    return grew;
}

/* Live every variable a glob of a name matches; whether one was new. */
static bool fxm_live_glob(struct fxm *m, const char *pat)
{
    bool grew = false;
    for (size_t k = 0; k < m->nvnames; k++)
        if (!m->vnames[k].live && fxm_glob(pat, m->vnames[k].name)) {
            m->vnames[k].live = true;
            grew = true;
        }
    return grew;
}

/* A computed name ($($(X)_Y)): every variable its spelling can match. */
static bool fxm_live_computed(struct fxm *m, const char *name)
{
    char pat[FXM_NAME_MAX];
    return fxm_name_glob(name, pat) && fxm_live_glob(m, pat);
}

/* A word that spells a variable's name (a call argument, a name held in a
 * value, $(value X)): that variable is live, as its reference would be. */
static bool fxm_token_var(struct fxm *m, const char *t)
{
    struct fxm_vname *v;
    if ((t = fxm_word_of(t)) == NULL)
        return false;
    if (strpbrk(t, FXM_WILDS) != NULL)
        return fxm_live_glob(m, t);
    v = fxm_vname(m, t, strlen(t));
    if (v == NULL || v->live)
        return false;
    v->live = true;
    return true;
}

/* A reference from a line that reaches: its variable is live. */
static bool fxm_live_ref(struct fxm *m, const char *name, size_t n)
{
    struct fxm_vname *v;
    if (*name == '$')
        return fxm_live_computed(m, name);
    v = fxm_vname(m, name, n);
    if (v == NULL || v->live)
        return false;
    v->live = true;
    return true;
}

/* A line whose mentions count: of a reached rule, or of no rule. */
bool fxm_live(const struct fxm *m, const struct fxm_line *l)
{
    if (l->ctx == FXM_ACTIVE || l->ctx == FXM_DEF || l->ctx == FXM_QUIET)
        return true;
    if (l->ctx == FXM_PHONY)
        return false;
    return m->whole || m->rules[l->rule].reached || l->runs_make;
}

/* A line whose .PHONY names and references reach: a directive (not a
 * conditional: it cannot make a rule run), a definition of a live or
 * computed variable or one that runs make as it is read, the prerequisites
 * of a reached rule, and a recipe line of one that runs make (only make
 * runs a rule; any other command cannot). */
static bool fxm_reaches(struct fxm *m, const struct fxm_line *l)
{
    struct fxm_vname *v;
    switch (l->ctx) {
    case FXM_ACTIVE:
        return true;
    case FXM_DEF:
        v = fxm_vname(m, l->name, strlen(l->name));
        return l->name[0] == '\0' || v == NULL || v->live ||
               fxm_direct_maker(l->raw);
    case FXM_RULE:
        return m->whole || m->rules[l->rule].reached;
    case FXM_RECIPE:
        return m->whole || (m->rules[l->rule].reached && l->runs_make);
    default:
        return false;
    }
}

/* Reach what a line names from its offset on (a rule's targets name no
 * rule it runs; a target-specific value's targets neither), less what its
 * calls cannot put in their values: .PHONY names, variables it references,
 * and variables it spells. */
static bool fxm_follow(struct fxm *m, const struct fxm_line *l)
{
    bool twice = l->ctx != FXM_RECIPE && (l->ctx != FXM_RULE || m->second);
    char *t = fxm_said(m, l);
    bool grew;
    if (t == NULL)
        return false;
    grew = fxm_tokens(m, t, fxm_token_phony);
    grew |= fxm_tokens(m, t, fxm_token_var);
    grew |= fxm_each_ref(m, m->line.p, twice, fxm_live_ref);
    return grew;
}

/* From every line that reaches, reach the .PHONY rules it names and the
 * variables it references, until nothing new is reached. */
void fxm_reach(struct fxm *m)
{
    bool grew = true;
    fxm_strs_seal(&m->phony);
    fxm_pairs(m);
    while (grew && !m->unknown) {
        grew = false;
        for (size_t k = 0; k < m->nlines; k++) {
            struct fxm_line *l = &m->lines[k];
            if (l->followed || !fxm_reaches(m, l))
                continue;
            l->followed = true;
            grew |= fxm_follow(m, l);
        }
    }
}

