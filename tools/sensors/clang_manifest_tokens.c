/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Marks where each file the TU read holds a live conditional-lookup word or '#', by clang's own lexing (clang_tokenize under the TU's language options), so the lookup scan never models comments, literals or raw strings.
 *
 * clang_tokenize re-lexes a file raw, with the TU's LangOpts: a word in a
 * comment or in a string, character or raw string literal is no
 * identifier token, and a trigraph or line splice inside a word is
 * cleaned. Raw lexing also covers the groups the preprocessor skipped,
 * which the scan needs (a probe there turns live when another probe
 * flips). A punctuator's spelling is its raw text, so the walk cleans it
 * (translation phases 1 and 2) before reading it as '#', '%:', '(' or '<'.
 *
 * Raw lexing differs from the preprocessor's in two places, and each is
 * lexed here as the preprocessor does:
 *  - a header name: after #include, #include_next, #import, #embed or
 *    #__include_macros, and after __has_include(, __has_include_next( or
 *    __has_embed(, a '<' opens one token up to its '>' (a comment opener
 *    or quote inside it is no comment or literal);
 *  - #warning and #error: the preprocessor reads the rest of the logical
 *    line as plain text, so a comment opener there opens nothing.
 * When the raw tokens run past such a span, the file is tokenized again
 * from its end. Both rules are applied wherever the spelling appears, live
 * or skipped: in a skipped group the preprocessor would open the comment,
 * so the scan then reads text the compiler ignores, which costs precision
 * only. The TU is refused where raw tokens run past a '<' ... '>' span on
 * a #pragma line (GCC dependency and include_alias take header names),
 * and when a file's tokens do not reach its last non-blank byte. */

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "clang_manifest.h"

#include "util/safe_alloc.h"

#include <stdlib.h>
#include <string.h>

/* One tokenization of a file from some offset on. */
struct cm_toks {
    CXToken *t;
    unsigned n;
};

/* Where the walk over a file stands. */
struct cm_tok_walk {
    CXTranslationUnit tu;
    const char *s;
    size_t n;
    bool trigraphs;
    uint8_t *live;
    size_t prev_end;     /* end of the last token, comments included */
    size_t reach;        /* the furthest token end */
    bool bol;            /* no token but comments since a line began */
    bool in_define;      /* the logical line is a #define */
    bool expect_name;    /* the last token was a line-start '#' */
    bool in_pragma;      /* the logical line is a #pragma */
    int header;          /* 1: the next token may open a header name; 2: a
                          * '(' is due first (after a probe word) */
    size_t restart;      /* nonzero: tokenize again from here */
    const char *refused; /* why the TU is refused, or NULL */
};

static size_t cm_tok_offset(CXSourceLocation l)
{
    unsigned off = 0;
    clang_getSpellingLocation(l, NULL, NULL, NULL, &off);
    return off;
}

/* The character at s[*i] after translation phases 1 and 2 (a trigraph
 * replaced when the TU has them; a backslash, optional blanks and a newline
 * removed), advancing *i past it; 0 at the end. */
static char cm_tok_char(const char *s, size_t n, size_t *i, bool trigraphs)
{
    static const char from[] = "=/'()!<>-", to[] = "#\\^[]|{}~";
    while (*i < n) {
        char ch = s[*i];
        size_t w = 1, j;
        const char *p;
        if (trigraphs && n - *i >= 3 && s[*i] == '?' && s[*i + 1] == '?' &&
            s[*i + 2] != '\0' && (p = strchr(from, s[*i + 2])) != NULL) {
            ch = to[p - from];
            w = 3;
        }
        if (ch != '\\') {
            *i += w;
            return ch;
        }
        for (j = *i + w; j < n && (s[j] == ' ' || s[j] == '\t'); j++)
            ;
        if (j < n && s[j] == '\r')
            j++;
        if (j >= n || s[j] != '\n') {
            *i += w;
            return ch;
        }
        *i = j + 1;
    }
    return 0;
}

/* Does the whitespace s[a, b) between two tokens hold a newline that ends
 * a logical line (one no splice removes)? */
static bool cm_tok_newline(const struct cm_tok_walk *w, size_t a, size_t b)
{
    size_t i = a;
    while (i < b) {
        char ch = cm_tok_char(w->s, b, &i, w->trigraphs);
        if (ch == '\n' || ch == '\r')
            return true;
    }
    return false;
}

/* The punctuator s[off, end) as phases 1 and 2 leave it (a splice or a
 * trigraph in it removed); false when it is longer than cap - 1. */
static bool cm_tok_clean(const struct cm_tok_walk *w, size_t off, size_t end,
                         char *out, size_t cap)
{
    size_t i = off, k = 0;
    while (i < end) {
        char ch = cm_tok_char(w->s, end, &i, w->trigraphs);
        if (ch == 0 || k + 1 >= cap)
            return false;
        out[k++] = ch;
    }
    out[k] = '\0';
    return true;
}

/* A punctuation token at s[off, end) spelled `p` once cleaned. */
static bool cm_tok_punct(const struct cm_tok_walk *w, CXToken t, size_t off,
                         size_t end, const char *p)
{
    char c[8];
    return clang_getTokenKind(t) == CXToken_Punctuation &&
           cm_tok_clean(w, off, end, c, sizeof(c)) && strcmp(c, p) == 0;
}

static bool cm_tok_hash(const struct cm_tok_walk *w, CXToken t, size_t off,
                        size_t end)
{
    return cm_tok_punct(w, t, off, end, "#") ||
           cm_tok_punct(w, t, off, end, "%:");
}

/* The end, just past its '>', of the header name whose '<' token starts at
 * s[lt], as clang's LexAngledStringLiteral reads it; 0 when a newline or
 * the end comes first (then the '<' is a lone less-than). */
static size_t cm_tok_header_end(const struct cm_tok_walk *w, size_t lt)
{
    size_t i = lt;
    (void)cm_tok_char(w->s, w->n, &i, w->trigraphs); /* the '<' */
    for (;;) {
        char ch = cm_tok_char(w->s, w->n, &i, w->trigraphs);
        if (ch == '\\')
            ch = cm_tok_char(w->s, w->n, &i, w->trigraphs);
        if (ch == 0 || ch == '\n' || ch == '\r')
            return 0;
        if (ch == '>')
            return i;
    }
}

/* The offset of the newline ending the logical line that holds s[from]. */
static size_t cm_tok_line_end(const struct cm_tok_walk *w, size_t from)
{
    size_t i = from;
    for (;;) {
        size_t at = i;
        char ch = cm_tok_char(w->s, w->n, &i, w->trigraphs);
        if (ch == 0 || ch == '\n' || ch == '\r')
            return ch == 0 ? w->n : at;
    }
}

/* Is the identifier or keyword token t spelled (cleaned) as one of set? */
static bool cm_tok_in(const struct cm_tok_walk *w, CXToken t,
                      const char *const *set, size_t n)
{
    CXString sp = clang_getTokenSpelling(w->tu, t);
    const char *c = clang_getCString(sp);
    bool in = false;
    for (size_t k = 0; c != NULL && k < n && !in; k++)
        in = strcmp(c, set[k]) == 0;
    clang_disposeString(sp);
    return in;
}

/* A conditional-lookup word; *hdr set when the preprocessor lexes a
 * header name in its operand. */
static bool cm_tok_probe(const struct cm_tok_walk *w, CXToken t, size_t off,
                         size_t end, bool *hdr)
{
    static const char *const hdr_words[] = {
        "__has_include", "__has_include_next", "__has_embed"};
    static const char *const other[] = {"__has_include__",
                                        "__has_include_next__"};
    size_t i = off;
    if (clang_getTokenKind(t) != CXToken_Identifier ||
        cm_tok_char(w->s, end, &i, w->trigraphs) != '_')
        return false;
    *hdr = cm_tok_in(w, t, hdr_words, 3);
    return *hdr || cm_tok_in(w, t, other, 2);
}

/* The directive name after a line-start '#'. */
static void cm_tok_directive(struct cm_tok_walk *w, CXToken t, size_t end)
{
    static const char *const define[] = {"define"}, *const pragma[] = {"pragma"};
    static const char *const hdr[] = {"include", "include_next", "import",
                                      "embed", "__include_macros"};
    static const char *const text[] = {"warning", "error"};
    CXTokenKind k = clang_getTokenKind(t);
    if (k != CXToken_Identifier && k != CXToken_Keyword)
        return;
    if (cm_tok_in(w, t, define, 1))
        w->in_define = true;
    else if (cm_tok_in(w, t, pragma, 1))
        w->in_pragma = true;
    else if (cm_tok_in(w, t, hdr, 5))
        w->header = 1;
    else if (cm_tok_in(w, t, text, 2))
        w->restart = cm_tok_line_end(w, end);
}

/* Every token of toks from index i whose start is before `close` ends by
 * it: the raw lexing agrees with the header name or text span there. */
static bool cm_tok_agrees(const struct cm_tok_walk *w,
                          const struct cm_toks *toks, unsigned i, size_t close)
{
    for (; i < toks->n; i++) {
        CXSourceRange r = clang_getTokenExtent(w->tu, toks->t[i]);
        size_t off = cm_tok_offset(clang_getRangeStart(r));
        if (off >= close)
            return true;
        if (cm_tok_offset(clang_getRangeEnd(r)) > close)
            return false;
    }
    return true;
}

/* The index of the first token of toks at or after `close`. */
static unsigned cm_tok_skip(const struct cm_tok_walk *w,
                            const struct cm_toks *toks, unsigned i,
                            size_t close)
{
    while (i < toks->n &&
           cm_tok_offset(clang_getTokenLocation(w->tu, toks->t[i])) < close)
        i++;
    return i;
}

/* Handle a span the preprocessor reads as one token or as text, ending at
 * `close`: skip the raw tokens inside it, or tokenize again from `close`
 * when they run past it. Returns the next index. */
static unsigned cm_tok_span(struct cm_tok_walk *w, const struct cm_toks *toks,
                            unsigned i, size_t close)
{
    w->prev_end = close;
    if (close > w->reach)
        w->reach = close;
    if (!cm_tok_agrees(w, toks, i, close)) {
        w->restart = close;
        return toks->n;
    }
    return cm_tok_skip(w, toks, i, close);
}

/* One token that is no comment, first on its logical line or not. */
static void cm_tok_one(struct cm_tok_walk *w, CXToken t, size_t off,
                       size_t end, bool line_start)
{
    bool hdr = false;
    if (line_start) {
        w->in_define = w->in_pragma = w->expect_name = false;
        w->header = 0;
    }
    if (w->expect_name) {
        w->expect_name = false;
        cm_tok_directive(w, t, end);
        return;
    }
    if (cm_tok_hash(w, t, off, end)) {
        w->live[off] = w->in_define ? 2 : 1;
        w->expect_name = line_start;
        return;
    }
    if (w->header == 2)
        w->header = cm_tok_punct(w, t, off, end, "(") ? 1 : 0;
    else
        w->header = 0;
    if (cm_tok_probe(w, t, off, end, &hdr)) {
        w->live[off] = w->in_define ? 2 : 1;
        w->header = hdr ? 2 : 0;
    }
}

/* After the token at [off, end): a header name the preprocessor lexes
 * whole, a #warning or #error line it reads as text, or a #pragma '<' the
 * raw tokens may read differently. Returns the next index. */
static unsigned cm_tok_after(struct cm_tok_walk *w, const struct cm_toks *toks,
                             unsigned i, size_t off, size_t end, bool opens)
{
    size_t close = cm_tok_punct(w, toks->t[i - 1], off, end, "<")
                       ? cm_tok_header_end(w, off) : 0;
    if (opens && close != 0) {
        w->header = 0;
        return cm_tok_span(w, toks, i, close);
    }
    if (w->in_pragma && close != 0 && !cm_tok_agrees(w, toks, i, close)) {
        w->refused = "a #pragma header name could hide text";
        return toks->n;
    }
    if (w->restart != 0) {
        size_t at = w->restart;
        w->restart = 0;
        i = cm_tok_span(w, toks, i, at);
    }
    return i;
}

/* Walk one tokenization. A comment is whitespace: it neither starts a
 * line nor is a token of one. */
static void cm_tok_walk_toks(struct cm_tok_walk *w, const struct cm_toks *toks)
{
    for (unsigned i = 0; i < toks->n && w->restart == 0 && w->refused == NULL;) {
        CXToken t = toks->t[i];
        CXSourceRange r = clang_getTokenExtent(w->tu, t);
        size_t off = cm_tok_offset(clang_getRangeStart(r));
        size_t end = cm_tok_offset(clang_getRangeEnd(r));
        bool opens = w->header == 1, line_start;
        if (cm_tok_newline(w, w->prev_end, off))
            w->bol = true;
        w->prev_end = end;
        if (end > w->reach)
            w->reach = end;
        i++;
        if (clang_getTokenKind(t) == CXToken_Comment)
            continue;
        line_start = w->bol;
        w->bol = false;
        cm_tok_one(w, t, off, end, line_start);
        i = cm_tok_after(w, toks, i, off, end, opens);
    }
}

/* Tokenize f from `from` to its end; false when libclang cannot place the
 * range in the file. */
static bool cm_tok_range(CXTranslationUnit tu, CXFile f, size_t from,
                         size_t n, struct cm_toks *out)
{
    CXSourceLocation a = clang_getLocationForOffset(tu, f, (unsigned)from);
    CXSourceLocation b = clang_getLocationForOffset(tu, f, (unsigned)n);
    CXSourceLocation null = clang_getNullLocation();
    out->t = NULL;
    out->n = 0;
    if (clang_equalLocations(a, null) || clang_equalLocations(b, null))
        return false;
    clang_tokenize(tu, clang_getRange(a, b), &out->t, &out->n);
    return true;
}

/* Do the tokens reach the file's last byte that is not whitespace (a
 * trailing splice or NUL counts as whitespace)? */
static bool cm_tok_covered(const struct cm_tok_walk *w)
{
    for (size_t i = w->reach; i < w->n; i++)
        if (strchr(" \t\n\r\f\v\\", w->s[i]) == NULL)
            return false;
    return true;
}

/* One tokenization pass from `from`; false when the TU is refused. */
static bool cm_tok_pass(struct cm_state *st, struct cm_file *f,
                        struct cm_tok_walk *w, size_t from)
{
    struct cm_toks toks;
    if (!cm_tok_range(st->tu, (CXFile)f->key, from, f->size, &toks))
        return cm_fail(&st->core, "cannot tokenize %s", f->path);
    w->restart = 0;
    w->prev_end = from;
    cm_tok_walk_toks(w, &toks);
    clang_disposeTokens(st->tu, toks.t, toks.n);
    if (w->refused != NULL)
        return cm_fail(&st->core,
                       "unsupported translation-unit language: %s in %s",
                       w->refused, f->path);
    if (w->restart != 0 && w->restart <= from)
        return cm_fail(&st->core, "cannot tokenize %s past offset %zu",
                       f->path, w->restart);
    return true;
}

static bool cm_tok_file(struct cm_state *st, struct cm_file *f,
                        bool trigraphs)
{
    struct cm_tok_walk w = {.tu = st->tu, .s = f->contents, .n = f->size,
                            .trigraphs = trigraphs, .bol = true};
    size_t from = 0;
    free(f->live);
    f->live = NULL;
    if (f->size == 0)
        return true;
    if (f->contents == NULL || f->size >= UINT32_MAX)
        return cm_fail(&st->core, "cannot tokenize %s", f->path);
    w.live = zcl_calloc(f->size, 1, "clang_manifest.live");
    if (w.live == NULL)
        return cm_fail(&st->core, "out of memory");
    f->live = w.live;
    do {
        if (!cm_tok_pass(st, f, &w, from))
            return false;
        from = w.restart;
    } while (from != 0 && from < f->size);
    if (!cm_tok_covered(&w))
        return cm_fail(&st->core, "cannot tokenize %s: tokens end at %zu",
                       f->path, w.reach);
    return true;
}

bool cm_tokenize_files(struct cm_state *st, bool trigraphs)
{
    for (size_t k = 0; k < st->core.nfiles; k++)
        if (!cm_tok_file(st, &st->core.files[k], trigraphs))
            return false;
    return true;
}
