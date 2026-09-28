/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The goal half of the facts consumer's make reader: which .PHONY goals a line can name through a value no text spells, and what make may write as it reads the makefiles. */
#include "devloop_facts_make.h"

#include <string.h>

/* A variable that reaches a goal position: its value may be a goal or a
 * prerequisite. */
static bool fxm_goal_mark(struct fxm_vname *v)
{
    bool grew = !v->goal;
    v->goal = v->live = true;
    return grew;
}

static bool fxm_goal_glob(struct fxm *m, const char *pat)
{
    bool grew = false;
    for (size_t k = 0; k < m->nvnames; k++)
        if (!m->vnames[k].goal && fxm_glob(pat, m->vnames[k].name))
            grew |= fxm_goal_mark(&m->vnames[k]);
    return grew;
}

/* A reference from a goal position (a computed one: every variable its
 * spelling matches; one no text defines is kept by name, for a make
 * command that sets it). */
bool fxm_goal_ref(struct fxm *m, const char *name, size_t n)
{
    char pat[FXM_NAME_MAX];
    struct fxm_vname *v;
    if (*name == '$')
        return fxm_name_glob(name, pat) && fxm_goal_glob(m, pat);
    if ((v = fxm_vname(m, name, n)) != NULL)
        return fxm_goal_mark(v);
    if (n >= sizeof(pat))
        return false;
    memcpy(pat, name, n);
    pat[n] = '\0';
    if (fxc_strs_has(&m->goal_names, pat))
        return false;
    m->unknown |= !fxc_strs_add(&m->goal_names, pat);
    return true;
}

/* A call whose value holds words of its arguments as they stand. */
static bool fxm_passes_words(const char *fn, size_t n)
{
    static const char *const fns[] = {
        "if",       "or",     "and",        "strip",   "sort",
        "firstword", "lastword", "word",    "wordlist", "filter",
        "filter-out", "foreach", "call",    "value",   "eval",
        "wildcard", "patsubst"};
    for (size_t k = 0; k < sizeof(fns) / sizeof(fns[0]); k++)
        if (strlen(fns[k]) == n && strncmp(fn, fns[k], n) == 0)
            return true;
    return false;
}

static bool fxm_goal_refs(struct fxm *m, const char *s, const char *end,
                          bool twice, int depth);

/* One word of a goal position that is one reference d..e (e its last
 * character): its variable's words stand alone in the position (it is in
 * a goal position too); a call that passes its arguments' words on
 * ($(call F,..) and $(value F) F's too) is read for its arguments' own
 * such references. Any other call's value, and a reference joined to
 * other text, is a glob the expansion already spells. */
static bool fxm_goal_unit(struct fxm *m, const char *d, const char *e,
                          bool twice, int depth)
{
    const char *name, *p = d + 2;
    size_t n = fxm_ref_name(d, &name), k = 0;
    bool grew = false;
    if (d[1] != '(' && d[1] != '{')
        return n > 0 && fxm_goal_ref(m, name, n);
    while (fxm_ident(p[k]))
        k++;
    if (k == 0 || !fxm_space(p[k]))
        return n > 0 && fxm_goal_ref(m, name, n);
    if (!fxm_passes_words(p, k) || depth >= FXM_ROUNDS)
        return false;
    if (name != p && n > 0)
        grew = fxm_goal_ref(m, name, n);
    grew |= fxm_goal_refs(m, p + k + 1, e, twice, depth + 1);
    return grew;
}

/* The reference the word w..s is as a whole, past a leading '@', '-' or
 * '+' (with twice, '$$(' is a reference too: text a later $(eval) expands
 * again): its '$'; NULL when the word is not one reference. */
static const char *fxm_whole_ref(const char *w, const char *s, bool twice)
{
    const char *d, *e = NULL;
    while (w < s && (*w == '@' || *w == '-' || *w == '+'))
        w++;
    d = twice && w + 1 < s && w[0] == '$' && w[1] == '$' ? w + 1 : w;
    if (s - d < 2 || d[0] != '$')
        return NULL;
    if (d[1] == '(' || d[1] == '{')
        e = fxm_ref_end((char *)d);
    else if (s - d == 2)
        e = d + 1;
    return e == s - 1 ? d : NULL;
}

/* Every word of s..end (a goal position's text) that is one reference
 * (fxm_whole_ref), read by fxm_goal_unit. */
static bool fxm_goal_refs(struct fxm *m, const char *s, const char *end,
                          bool twice, int depth)
{
    bool grew = false;
    while (s < end) {
        const char *w, *d;
        int nest = 0;
        while (s < end && fxm_sep(*s))
            s++;
        for (w = s; s < end && (nest > 0 || !fxm_sep(*s)); s++)
            nest += (*s == '(' || *s == '{') - (*s == ')' || *s == '}');
        if ((d = fxm_whole_ref(w, s, twice)) != NULL)
            grew |= fxm_goal_unit(m, d, s - 1, twice, depth);
    }
    return grew;
}

/* A word a goal position spells that names a variable (a call argument, a
 * name held in a value). */
static bool fxm_goal_var(struct fxm *m, const char *t)
{
    struct fxm_vname *v;
    if ((t = fxm_word_of(t)) == NULL)
        return false;
    if (strpbrk(t, FXM_WILDS) != NULL)
        return fxm_goal_glob(m, t);
    v = fxm_vname(m, t, strlen(t));
    return v != NULL && fxm_goal_mark(v);
}

/* A word of a goal position no literal text pins: a lone pattern, or a run
 * of values no text spells (an automatic variable's, a call's new words, a
 * join of two values); not one variable's value alone (FXM_ANY), whose
 * definition is followed. Every .PHONY name it can match is reached. */
static bool fxm_open_phony(struct fxm *m, const char *t)
{
    bool grew = false;
    while (*t == '@' || *t == '-' || *t == '+')
        t++;
    if ((t[0] == FXM_ANY && t[1] == '\0') || fxm_word_of(t) != NULL)
        return false;
    for (size_t k = 0; k < m->npairs; k++)
        if (!m->rules[m->pairs[k].rule].reached && fxm_glob(t, m->pairs[k].name))
            grew |= fxm_reach_name(m, m->pairs[k].name, strlen(m->pairs[k].name));
    return grew;
}

/* A shell word a backslash escapes: the name the shell makes of it is not
 * the text. */
static bool fxm_escaped(struct fxm *m, const char *t)
{
    (void)m;
    return strchr(t, '\\') != NULL;
}

/* Every rule is reached: a goal position spells any name. */
static bool fxm_reach_all(struct fxm *m)
{
    bool grew = false;
    for (size_t r = 0; r < m->nrules; r++) {
        grew |= !m->rules[r].reached;
        m->rules[r].reached = true;
    }
    return grew;
}

/* Blank the words a call's value never holds alone (FXM_HIDE_ON ..
 * FXM_HIDE_OFF): the value's own words spell them joined. */
static void fxm_unhide(char *t)
{
    int depth = 0;
    for (; *t != '\0'; t++) {
        depth += (*t == FXM_HIDE_ON) - (*t == FXM_HIDE_OFF);
        if (depth > 0 || *t == FXM_HIDE_OFF)
            *t = ' ';
    }
}

/* One goal position's text (as written, less what only tests or prints):
 * the variables it reads and spells are goal positions too; a word no
 * literal text pins reaches what it can match (fxm_open_phony). In a shell
 * command (shell), '%' is shell text, a backslash-escaped word reaches
 * every rule, and a word joined across quotes (ge"n") is the name it
 * joins to. */
bool fxm_goal_words(struct fxm *m, const char *raw, bool twice,
                    bool shell)
{
    char *t, *w;
    bool grew = fxm_goal_refs(m, raw, raw + strlen(raw), twice, 0);
    if ((t = (char *)fxm_expand(m, raw, strlen(raw))) == NULL)
        return grew;
    for (char *c = t; shell && *c != '\0'; c++)
        *c = *c == '%' ? FXM_PCT : *c;
    grew |= fxm_tokens(m, t, fxm_goal_var);
    fxm_unhide(t);
    grew |= fxm_tokens(m, t, fxm_open_phony);
    if (!shell)
        return grew;
    if (fxm_tokens(m, t, fxm_escaped))
        grew |= fxm_reach_all(m);
    w = t;
    for (const char *c = t; *c != '\0'; c++)
        if (*c != '"' && *c != '\'')
            *w++ = *c;
    *w = '\0';
    grew |= fxm_tokens(m, t, fxm_token_phony);
    return grew;
}

/* ---- make inputs: what make runs as it reads the makefiles ---------------- */

/* The quote state after s[k] of shell text: '\'' or '"' while inside
 * one. A command substitution ($$( or a backquote) inside double quotes
 * ends the tracking: from there every character counts as unquoted. */
static char fxm_quote_after(const char *s, size_t k, size_t n, char q, bool *plain)
{
    if (q == '\'')
        return s[k] == '\'' ? '\0' : q;
    if (q == '"' && (s[k] == '`' || (s[k] == '$' && k + 2 < n && s[k + 1] == '$' &&
                                     s[k + 2] == '('))) {
        *plain = true;
        return '\0';
    }
    if (s[k] == '"')
        return q == '"' ? '\0' : '"';
    return q == '\0' && s[k] == '\'' ? '\'' : q;
}

/* Unquoted shell text at s[k] (of s[0..n)) writes a file: a redirection
 * to anything but a descriptor or /dev/null, or a tee. */
static bool fxm_writes_at(const char *s, size_t k, size_t n)
{
    if (s[k] == '>')
        return k + 1 == n || !fxm_quiet_redirect(s + k + 1);
    return n - k >= 3 && strncmp(s + k, "tee", 3) == 0 &&
           (k == 0 || !fxm_ident(s[k - 1])) && (k + 3 == n || !fxm_ident(s[k + 3]));
}

/* Shell text s[0..n), as written, writes a file outside quotes
 * (fxm_writes_at). */
static bool fxm_redirects(const char *s, size_t n)
{
    char q = '\0';
    bool plain = false;
    for (size_t k = 0; k < n; k++) {
        if (q != '\'' && s[k] == '\\') {
            k++;
            continue;
        }
        if (q == '\0' && fxm_writes_at(s, k, n))
            return true;
        if (!plain)
            q = fxm_quote_after(s, k, n, q, &plain);
    }
    return false;
}

/* The value of a != definition (a target-specific one too); NULL for any
 * other line. */
static const char *fxm_bang_value(const struct fxm_line *l)
{
    size_t colon = 0;
    enum fxm_kind kind;
    const char *at, *eq;
    if (l->ctx != FXM_DEF || l->body)
        return NULL;
    kind = fxm_kind_of(l->raw, &colon);
    if (kind != FXM_K_DEF && kind != FXM_K_TSV)
        return NULL;
    at = fxm_skip_prefixes(l->raw + (kind == FXM_K_TSV ? colon + 1 : 0));
    eq = fxm_top(at, "=");
    return eq != NULL && eq > at && eq[-1] == '!' ? eq + 1 : NULL;
}

/* A call in s make runs as it expands it: a $(shell) whose command, as
 * written, writes a file, or a $(file) that is not a read. */
static bool fxm_writing_calls(const char *s)
{
    for (const char *d = strchr(s, '$'); d != NULL; d = strchr(d + 1, '$')) {
        const char *o = d + 1, *e, *a;
        if (*o != '(' && *o != '{')
            continue;
        if (fxm_starts_word(o + 1, "file")) {
            for (a = o + 5; fxm_space(*a); a++)
                ;
            if (*a != '<')
                return true;
        } else if (fxm_starts_word(o + 1, "shell")) {
            if ((e = fxm_ref_end((char *)d)) == NULL)
                return true;
            if (e > o + 6 && fxm_redirects(o + 6, (size_t)(e - o - 6)))
                return true;
        }
    }
    return false;
}

bool fxm_parse_writes(const struct fxm *m)
{
    for (size_t k = 0; k < m->nlines; k++) {
        const struct fxm_line *l = &m->lines[k];
        const char *v = fxm_bang_value(l);
        /* A recipe line, a define's too, runs as its rule does. */
        if (l->ctx == FXM_RECIPE || (l->body && l->raw[0] == '\t'))
            continue;
        if ((v != NULL && fxm_redirects(v, strlen(v))) || fxm_writing_calls(l->raw))
            return true;
    }
    return false;
}

/* A target word that makes any file: match-anything (%) or .DEFAULT. */
static bool fxm_makes_anything(const char *t)
{
    while (*t != '\0') {
        size_t n = 0;
        while (fxm_space(*t))
            t++;
        while (t[n] != '\0' && !fxm_space(t[n]))
            n++;
        if ((n == 1 && t[0] == '%') || (n == 8 && strncmp(t, ".DEFAULT", 8) == 0))
            return true;
        t += n;
    }
    return false;
}

/* A match-anything rule (%:) or .DEFAULT makes any file: every missing
 * optional include is made by a recipe no rule names it in. */
bool fxm_anything_made(const struct fxm *m)
{
    for (size_t r = 0; m->missing.n > 0 && r < m->nrules; r++)
        if (fxm_makes_anything(m->rules[r].targets))
            return true;
    return false;
}
