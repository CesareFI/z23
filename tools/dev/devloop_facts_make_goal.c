/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The goal half of the facts consumer's make reader: which .PHONY goals a line can name through a value no text spells. */
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

/* ---- make inputs: what a recipe writes into a makefile make includes ------ */

/* A recipe line of a rule that makes a missing optional include is read
 * as a small shell: each command is one fxm_gen_plain allows or a program
 * the tree holds; the words a printf or echo writes to a file are makefile
 * text (fxm_gen_printed); mv moves only a temporary file mktemp named. Any
 * other command, a redirection of anything else, or a line too long to
 * hold is UNKNOWN. */
#define FXM_GEN_MAX 4096

static bool fxm_listed(const char *const *v, size_t nv, const char *w,
                       size_t n)
{
    for (size_t k = 0; k < nv; k++)
        if (strlen(v[k]) == n && strncmp(v[k], w, n) == 0)
            return true;
    return false;
}

/* A command that writes no makefile text of its own: printf and echo
 * write only through a redirection fxm_gen_segment reads. */
static bool fxm_gen_plain(const char *w, size_t n)
{
    static const char *const ok[] = {
        "set", "mkdir", "trap", "mktemp", "true", ":", "exit", "test",
        "[", "rm", "touch", "chmod", "printf", "echo", "mv"};
    return fxm_listed(ok, sizeof(ok) / sizeof(ok[0]), w, n);
}

/* A program the tree holds (a literal relative path): an opaque program,
 * as any script a recipe runs. */
static bool fxm_gen_script(const char *w, size_t n)
{
    return n > 0 && w[0] != '/' && memchr(w, '/', n) != NULL &&
           strcspn(w, "$\"'`\\") >= n;
}

/* A command a command substitution runs: one that prints a path. */
static bool fxm_gen_subst_ok(const char *p)
{
    static const char *const ok[] = {"mktemp", "dirname", "basename", "pwd"};
    size_t n = 0;
    while (fxm_space(*p))
        p++;
    while (p[n] != '\0' && !fxm_space(p[n]) && strchr(")`\";", p[n]) == NULL)
        n++;
    return fxm_listed(ok, sizeof(ok) / sizeof(ok[0]), p, n);
}

/* Every command substitution in s runs a command fxm_gen_subst_ok allows. */
static bool fxm_gen_substs_ok(const char *s)
{
    for (const char *p = s; *p != '\0'; p++) {
        if (p[0] == '$' && p[1] == '$' && p[2] == '(' && !fxm_gen_subst_ok(p + 3))
            return false;
        if (p[0] != '`')
            continue;
        if (!fxm_gen_subst_ok(p + 1))
            return false;
        p += 1 + strcspn(p + 1, "`"); /* its closing '`' */
        if (*p == '\0')
            break;
    }
    return true;
}

/* The command word of the segment s (past its prefixes and NAME=value
 * words, a quoted value whole); its length in *n. *bare_ok: every value
 * is a command substitution of mktemp (what a bare assignment may hold).
 * NULL when a quote does not close. */
static char *fxm_gen_name(char *s, size_t *n, bool *bare_ok)
{
    char *p = fxm_command_word(s, s + strlen(s)), *e;
    for (;;) {
        size_t k = 0;
        while (fxm_ident(p[k]))
            k++;
        if ((e = fxm_word_end(p)) == NULL)
            return NULL;
        if (k == 0 || p[k] != '=')
            break;
        *bare_ok &= strncmp(p + k + 1, "$$(mktemp", 9) == 0 ||
                    strncmp(p + k + 1, "\"$$(mktemp", 10) == 0;
        for (p = e; fxm_space(*p); p++)
            ;
    }
    *n = (size_t)(e - p);
    return p;
}

/* A redirection word (digits, '&>', '<', '>'): its target, "" when it is
 * the next word; NULL for any other word. *out: it writes. */
static const char *fxm_gen_redirect(const char *w, bool *out)
{
    const char *p = w;
    while (*p >= '0' && *p <= '9')
        p++;
    if (*p == '&' && p[1] == '>')
        p++;
    if (*p != '<' && *p != '>')
        return NULL;
    *out = *p == '>';
    while (*p == '<' || *p == '>')
        p++;
    return p;
}

/* A redirection target that is a file: not a descriptor (&2, &-) and not
 * /dev/null. */
static bool fxm_gen_file(const char *t)
{
    if (t[0] == '&' && (t[1] == '-' || (t[1] >= '0' && t[1] <= '9')))
        return false;
    return strcmp(t, "/dev/null") != 0;
}

#define FXM_GEN_WORDS 256

/* One command's argument words (redirections and their targets left
 * out), NUL-ended in place, and whether it writes to a file. */
struct fxm_gen_cmd {
    char *w[FXM_GEN_WORDS];
    size_t n;
    bool file;
};

/* Split the words from p into c; false when a quote does not close or
 * there are too many. */
static bool fxm_gen_words(char *p, struct fxm_gen_cmd *c)
{
    bool next = false, out = false;
    const char *t;
    while (*p != '\0') {
        char *w = p, *e;
        while (fxm_space(*w))
            w++;
        if (*w == '\0')
            break;
        if ((e = fxm_word_end(w)) == NULL)
            return false;
        p = *e != '\0' ? e + 1 : e;
        *e = '\0';
        if (next) {
            c->file |= out && fxm_gen_file(w);
            next = false;
        } else if ((t = fxm_gen_redirect(w, &out)) != NULL) {
            c->file |= out && *t != '\0' && fxm_gen_file(t);
            next = *t == '\0';
        } else if (c->n == FXM_GEN_WORDS) {
            return false;
        } else {
            c->w[c->n++] = w;
        }
    }
    return !next;
}

/* One word a printf or echo writes to a file: the makefile text it spells.
 * Quotes are no text; a written newline (\n) or tab (\t) separates; in a
 * printf format (format) a conversion is no text; from a '#' to a written
 * newline is a comment. A shell value ($$x, a command substitution) is
 * text no line holds: every .PHONY name it can be. */
static bool fxm_gen_printed(struct fxm *m, const char *w, bool format)
{
    static const char open[] = {FXM_OPEN, '\0'};
    char b[ZCL_DEVLOOP_PATH_MAX];
    size_t k = 0, n = strlen(w);
    bool comment = false;
    if (n >= sizeof(b) || strstr(w, "$$") != NULL || strchr(w, '`') != NULL)
        return fxm_open_phony(m, open);
    for (size_t i = 0; i < n; i++) {
        bool esc = w[i] == '\\' && (w[i + 1] == 'n' || w[i + 1] == 't');
        if (w[i] == '"' || w[i] == '\'')
            continue;
        if (esc || (format && w[i] == '%' && w[i + 1] != '\0')) {
            comment &= !(esc && w[i + 1] == 'n');
            b[k++] = ' ';
            i++;
            continue;
        }
        comment |= w[i] == '#';
        b[k++] = comment ? ' ' : w[i];
    }
    b[k] = '\0';
    return fxm_goal_words(m, b, false, true);
}

/* mv moves a temporary file (a shell value, $$tmp): every word but its
 * options and its destination holds one. */
static bool fxm_gen_moves(const struct fxm_gen_cmd *c)
{
    size_t last = c->n;
    while (last > 0 && c->w[last - 1][0] == '-')
        last--;
    for (size_t k = 0; last > 0 && k + 1 < c->n; k++)
        if (c->w[k][0] != '-' && strstr(c->w[k], "$$") == NULL)
            return false;
    return true;
}

/* trap's action: none ("-"), or an rm of a temporary file. */
static bool fxm_gen_trap(const struct fxm_gen_cmd *c)
{
    const char *a = c->n > 0 ? c->w[0] : "-";
    if (*a == '\'' || *a == '"')
        a++;
    return strcmp(a, "-") == 0 ||
           (strncmp(a, "rm ", 3) == 0 && strcspn(a, ";|&<>`(") == strlen(a));
}

/* One command of a generated makefile's recipe line (NUL-ended s). */
static bool fxm_gen_command(struct fxm *m, char *s)
{
    struct fxm_gen_cmd c = {.n = 0};
    bool bare_ok = true, grew = false, printf_;
    size_t n;
    char *name = fxm_gen_name(s, &n, &bare_ok);
    if (name == NULL || (n == 0 && !bare_ok) ||
        (n > 0 && !fxm_gen_plain(name, n) && !fxm_gen_script(name, n))) {
        m->unknown = true;
        return false;
    }
    printf_ = n == 6 && strncmp(name, "printf", 6) == 0;
    if (n == 0 || !fxm_gen_words(name + n, &c) ||
        (c.file && !printf_ && !(n == 4 && strncmp(name, "echo", 4) == 0)) ||
        (n == 2 && strncmp(name, "mv", 2) == 0 && !fxm_gen_moves(&c)) ||
        (n == 4 && strncmp(name, "trap", 4) == 0 && !fxm_gen_trap(&c))) {
        m->unknown |= n > 0;
        return false;
    }
    for (size_t k = 0; c.file && k < c.n; k++)
        grew |= fxm_gen_printed(m, c.w[k], printf_ && k == 0);
    return grew;
}

bool fxm_gen_recipe(struct fxm *m, const struct fxm_line *l)
{
    char buf[FXM_GEN_MAX];
    bool grew = false;
    char *p = buf, *e;
    if (strlen(l->raw) >= sizeof(buf) || !fxm_gen_substs_ok(l->raw)) {
        m->unknown = true;
        return false;
    }
    memcpy(buf, l->raw, strlen(l->raw) + 1);
    while (!m->unknown && *p != '\0') {
        if ((e = fxm_command_end(p)) == NULL) {
            m->unknown = true;
            break;
        }
        bool last = *e == '\0';
        *e = '\0';
        grew |= fxm_gen_command(m, p);
        for (p = last ? e : e + 1; *p == ';' || *p == '&' || *p == '|'; p++)
            ;
    }
    return grew;
}
