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
 * text (fxm_gen_printf, fxm_gen_echo); mv moves only a temporary file
 * mktemp named. Any other command, a redirection of anything else, a
 * $(shell), $(file) or $(eval) the recipe expands, or a line too long to
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

/* Every .PHONY name is reached: the makefile holds text no line spells. */
static bool fxm_gen_open(struct fxm *m)
{
    static const char open[] = {FXM_OPEN, '\0'};
    return fxm_open_phony(m, open);
}

/* The written makefile text b (NUL-ended): from a '#' to a newline is a
 * comment but on a line a tab leads (a recipe line: the shell reads its
 * '#'), and newlines and tabs separate. Its words reach what they name, as
 * a goal position's (a backslash left in it: every rule). */
static bool fxm_gen_text(struct fxm *m, char *b)
{
    bool comment = false, recipe = *b == '\t';
    for (char *c = b; *c != '\0'; c++) {
        comment = *c != '\n' && (comment || (*c == '#' && !recipe));
        recipe = *c == '\n' ? c[1] == '\t' : recipe;
        if (comment || *c == '\n' || *c == '\t')
            *c = ' ';
    }
    return fxm_goal_words(m, b, false, true);
}

/* Append ch to b at *k (cap bytes); false when it does not fit. */
static bool fxm_gen_put(char *b, size_t *k, size_t cap, char ch)
{
    if (*k + 1 >= cap)
        return false;
    b[(*k)++] = ch;
    return true;
}

/* Append the shell word w, its quotes removed. */
static bool fxm_gen_put_word(char *b, size_t *k, size_t cap, const char *w)
{
    for (; *w != '\0'; w++)
        if (*w != '"' && *w != '\'' && !fxm_gen_put(b, k, cap, *w))
            return false;
    return true;
}

/* A word whose text only the shell knows: a shell value ($$x) or a
 * command substitution. */
static bool fxm_gen_shelled(const struct fxm_gen_cmd *c)
{
    for (size_t k = 0; k < c->n; k++)
        if (strstr(c->w[k], "$$") != NULL || strchr(c->w[k], '`') != NULL)
            return true;
    return false;
}

/* One pass of a printf format f over its arguments from *arg: \n and \t
 * written, %% one '%', %s the next argument (none left: nothing); any
 * other escape stays a backslash. False for any other conversion (%c, %b,
 * %d, a width) or text too long to hold. */
static bool fxm_gen_format(const char *f, const struct fxm_gen_cmd *c,
                           size_t *arg, char *b, size_t *k)
{
    for (; *f != '\0'; f++) {
        char ch = *f;
        bool conv = false;
        if (ch == '"' || ch == '\'')
            continue;
        if (ch == '\\' && (f[1] == 'n' || f[1] == 't'))
            ch = *++f == 'n' ? '\n' : '\t';
        else if (ch == '%' && f[1] == '%')
            ch = *++f;
        else if (ch == '%' && f[1] == 's')
            conv = *++f == 's';
        else if (ch == '%')
            return false;
        if (conv ? *arg < c->n && !fxm_gen_put_word(b, k, FXM_GEN_MAX, c->w[(*arg)++])
                 : !fxm_gen_put(b, k, FXM_GEN_MAX, ch))
            return false;
    }
    return true;
}

/* What a printf writes: its format applied to its arguments, again while
 * arguments remain, as printf does. A shell value, an option (-v) or a
 * conversion other than %s is text no line holds: every .PHONY name. */
static bool fxm_gen_printf(struct fxm *m, const struct fxm_gen_cmd *c)
{
    char b[FXM_GEN_MAX];
    size_t k = 0, arg = 1, before;
    if (c->n == 0)
        return false;
    if (c->w[0][0] == '-' || fxm_gen_shelled(c))
        return fxm_gen_open(m);
    do {
        before = arg;
        if (!fxm_gen_format(c->w[0], c, &arg, b, &k))
            return fxm_gen_open(m);
    } while (arg < c->n && arg > before);
    b[k] = '\0';
    return fxm_gen_text(m, b);
}

/* What an echo writes: its words, one space apart. A shell value or a
 * backslash (echo -e, and dash's echo, read escapes) is text no line
 * holds: every .PHONY name. */
static bool fxm_gen_echo(struct fxm *m, const struct fxm_gen_cmd *c)
{
    char b[FXM_GEN_MAX];
    size_t k = 0;
    if (fxm_gen_shelled(c))
        return fxm_gen_open(m);
    for (size_t i = 0; i < c->n; i++)
        if (strchr(c->w[i], '\\') != NULL || !fxm_gen_put_word(b, &k, sizeof(b), c->w[i]) ||
            !fxm_gen_put(b, &k, sizeof(b), ' '))
            return fxm_gen_open(m);
    b[k] = '\0';
    return fxm_gen_text(m, b);
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

/* The command word w[0..n) is s. */
static bool fxm_gen_is(const char *w, size_t n, const char *s)
{
    return strlen(s) == n && strncmp(w, s, n) == 0;
}

/* The command name[0..n) with its words c is one a generated makefile's
 * recipe may run: only printf and echo write to a file; mv and trap as
 * fxm_gen_moves and fxm_gen_trap say. */
static bool fxm_gen_allowed(const char *name, size_t n,
                            const struct fxm_gen_cmd *c)
{
    if (!fxm_gen_plain(name, n) && !fxm_gen_script(name, n))
        return false;
    if (c->file && !fxm_gen_is(name, n, "printf") && !fxm_gen_is(name, n, "echo"))
        return false;
    if (fxm_gen_is(name, n, "mv"))
        return fxm_gen_moves(c);
    return !fxm_gen_is(name, n, "trap") || fxm_gen_trap(c);
}

/* s[0..n) holds w[0..wn). */
static bool fxm_gen_has(const char *s, size_t n, const char *w, size_t wn)
{
    for (size_t k = 0; wn > 0 && k + wn <= n; k++)
        if (memcmp(s + k, w, wn) == 0)
            return true;
    return false;
}

/* s[0..n) names the file a rule makes: an automatic variable ($@, $(@D),
 * $*), a shell value ($$tmp), or the last path part of one of its targets
 * (targets). */
static bool fxm_gen_names_target(const char *s, size_t n, const char *targets)
{
    static const char *const autos[] = {"$@", "$(@", "${@", "$*", "$(*", "${*", "$$"};
    for (size_t k = 0; k < sizeof(autos) / sizeof(autos[0]); k++)
        if (fxm_gen_has(s, n, autos[k], strlen(autos[k])))
            return true;
    while (*targets != '\0') {
        const char *t, *base;
        size_t len = 0;
        while (fxm_space(*targets))
            targets++;
        for (t = targets; t[len] != '\0' && !fxm_space(t[len]); len++)
            ;
        targets = t + len;
        base = t + len;
        while (base > t && base[-1] != '/')
            base--;
        if (base < t + len && fxm_gen_has(s, n, base, (size_t)(t + len - base)))
            return true;
    }
    return false;
}

/* A program the tree holds may write the makefile through a name it is
 * given (an argument or an environment value, before or after its
 * command word at name[0..n) in s): then what it writes no line holds. */
static bool fxm_gen_hands_target(const char *s, const char *name, size_t n,
                                 const char *targets)
{
    return fxm_gen_names_target(s, (size_t)(name - s), targets) ||
           fxm_gen_names_target(name + n, strlen(name + n), targets);
}

/* What one command that writes to a file puts in the makefile (only
 * printf and echo may). A second write of the rule's (r) recipe can join
 * the first's text (> then >>, echo -n): every .PHONY name. */
static bool fxm_gen_writes(struct fxm *m, struct fxm_rule *r,
                           const struct fxm_gen_cmd *c, bool printf_)
{
    if (!c->file)
        return false;
    if (r->writes > 0)
        return fxm_gen_open(m);
    r->writes = 1;
    return printf_ ? fxm_gen_printf(m, c) : fxm_gen_echo(m, c);
}

/* One command of a generated makefile's recipe line (NUL-ended s) of rule
 * r. */
static bool fxm_gen_command(struct fxm *m, struct fxm_rule *r, char *s)
{
    struct fxm_gen_cmd c = {.n = 0};
    bool bare_ok = true;
    size_t n;
    char *name = fxm_gen_name(s, &n, &bare_ok);
    if (name != NULL && n == 0) {
        m->unknown |= !bare_ok;
        return false;
    }
    if (name == NULL ||
        (fxm_gen_script(name, n) && fxm_gen_hands_target(s, name, n, r->targets)) ||
        !fxm_gen_words(name + n, &c) || !fxm_gen_allowed(name, n, &c)) {
        m->unknown = true;
        return false;
    }
    return fxm_gen_writes(m, r, &c, fxm_gen_is(name, n, "printf"));
}

/* Text that runs something as make expands it: a $(shell), $(file) or
 * $(eval) call (its output or its file is text no line holds). */
static bool fxm_runs_text(const char *s)
{
    static const char *const fns[] = {"shell", "file", "eval"};
    for (const char *d = strchr(s, '$'); d != NULL; d = strchr(d + 1, '$'))
        for (size_t k = 0; k < sizeof(fns) / sizeof(fns[0]); k++)
            if ((d[1] == '(' || d[1] == '{') && fxm_starts_word(d + 2, fns[k]))
                return true;
    return false;
}

/* A reference whose expansion may run something: a computed name, or a
 * variable fxm_gen_runs marked. */
static bool fxm_ref_runs(struct fxm *m, const char *name, size_t n)
{
    struct fxm_vname *v;
    return *name == '$' || m->runs_any ||
           ((v = fxm_vname(m, name, n)) != NULL && v->runs);
}

/* The value text of a definition line that make expands each time it is
 * referenced (=, ?=, +=, a define's body; not :=, ::= or !=, expanded as
 * the line is read); NULL for any other line. */
static const char *fxm_deferred_value(const struct fxm_line *l)
{
    size_t colon = 0;
    enum fxm_kind kind;
    const char *at, *eq;
    if (l->body)
        return l->raw;
    kind = fxm_kind_of(l->raw, &colon);
    if (kind != FXM_K_DEF && kind != FXM_K_TSV)
        return NULL;
    at = fxm_skip_prefixes(l->raw + (kind == FXM_K_TSV ? colon + 1 : 0));
    eq = fxm_top(at, "=");
    if (eq == NULL || eq == at || eq[-1] == ':' || eq[-1] == '!')
        return NULL;
    return eq + 1;
}

void fxm_gen_runs(struct fxm *m)
{
    for (bool grew = true; grew && !m->runs_any;) {
        grew = false;
        for (size_t k = 0; k < m->nlines && !m->runs_any; k++) {
            const struct fxm_line *l = &m->lines[k];
            struct fxm_vname *v;
            const char *text = l->ctx == FXM_DEF ? fxm_deferred_value(l) : NULL;
            if (text == NULL ||
                (!fxm_runs_text(text) && !fxm_each_ref(m, text, true, fxm_ref_runs)))
                continue;
            if (l->name[0] == '\0') {
                m->runs_any = grew = true;
                continue;
            }
            v = fxm_vname(m, l->name, strlen(l->name));
            if (v != NULL && !v->runs)
                v->runs = grew = true;
        }
    }
}

bool fxm_gen_recipe(struct fxm *m, const struct fxm_line *l)
{
    char buf[FXM_GEN_MAX];
    bool grew = false;
    char *p = buf, *e;
    if (strlen(l->raw) >= sizeof(buf) || !fxm_gen_substs_ok(l->raw) ||
        fxm_runs_text(l->raw) || fxm_each_ref(m, l->raw, false, fxm_ref_runs)) {
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
        grew |= fxm_gen_command(m, &m->rules[l->rule], p);
        for (p = last ? e : e + 1; *p == ';' || *p == '&' || *p == '|'; p++)
            ;
    }
    return grew;
}
