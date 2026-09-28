/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Include lookups, stat-confirmed negative probes and __has_include replay shared by both semantic manifest producers. */
/* realpath() is declared only under _DEFAULT_SOURCE on glibc without the
 * fortify inline; set it before the first header pulls in <features.h>. */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "clang_manifest_core.h"

#include "base/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CM_PRESENT_MAX 64

/* Probe slots of one lookup, in the order the front end searches them:
 * quoted: the includer's directory, then the quote dirs, then the angled
 * dirs; angled: the angled dirs only. */
struct cm_probe {
    uint8_t form;
    char includer_dir[PATH_MAX];
    const char *name;
    uint32_t hit_slot;
    uint32_t present[CM_PRESENT_MAX];
    size_t npresent;
    char found_real[PATH_MAX];
    bool found;
};

static uint32_t cm_slot_count(const struct cm_core *c, uint8_t form)
{
    size_t n = c->angled.n;
    if (form == VCS_SEMANTIC_FORM_V1_QUOTED)
        n += 1 + c->quote.n;
    return (uint32_t)n;
}

static const char *cm_slot_dir(const struct cm_core *c,
                               const struct cm_probe *p, uint32_t slot)
{
    if (p->form == VCS_SEMANTIC_FORM_V1_QUOTED) {
        if (slot == 0)
            return p->includer_dir;
        slot -= 1;
        if (slot < c->quote.n)
            return c->quote.items[slot].raw;
        slot -= (uint32_t)c->quote.n;
    }
    return slot < c->angled.n ? c->angled.items[slot].raw : NULL;
}

static void cm_dir_of(const char *opened, char out[PATH_MAX])
{
    const char *slash = strrchr(opened, '/');
    if (slash == NULL) {
        (void)snprintf(out, PATH_MAX, ".");
        return;
    }
    if (slash == opened) {
        (void)snprintf(out, PATH_MAX, "/");
        return;
    }
    (void)snprintf(out, PATH_MAX, "%.*s", (int)(slash - opened), opened);
}

/* Walk the slots. With want_real set, stop at the slot whose regular file
 * is that exact file (the compiler's reported hit) and record any earlier
 * regular file that the compiler skipped; without it, stop at the first
 * regular file (a replay of the search). Misses are stat-confirmed. */
static void cm_probe_run(const struct cm_core *c, struct cm_probe *p,
                         const char *want_real)
{
    uint32_t count = cm_slot_count(c, p->form);
    p->hit_slot = count;
    p->npresent = 0;
    p->found = false;
    for (uint32_t s = 0; s < count; s++) {
        char cand[PATH_MAX * 2];
        struct stat sb;
        (void)snprintf(cand, sizeof(cand), "%s/%s", cm_slot_dir(c, p, s),
                       p->name);
        if (stat(cand, &sb) != 0 || !S_ISREG(sb.st_mode))
            continue;
        if (realpath(cand, p->found_real) == NULL)
            continue;
        if (want_real == NULL || strcmp(p->found_real, want_real) == 0) {
            p->hit_slot = s;
            p->found = true;
            return;
        }
        if (p->npresent < CM_PRESENT_MAX)
            p->present[p->npresent++] = s;
    }
}

static bool cm_emit_lookup(struct cm_core *c, const struct cm_file *includer,
                           const struct cm_probe *p, uint8_t kind,
                           const char *hit_path, uint8_t evidence)
{
    struct vcs_semantic_record_v1 rec = {0};
    bool ok;
    vcs_semantic_record_v1_cstr(&rec, includer->path);
    vcs_semantic_record_v1_cstr(&rec, p->name);
    vcs_semantic_record_v1_u8(&rec, p->form);
    vcs_semantic_record_v1_u8(&rec, kind);
    vcs_semantic_record_v1_cstr(&rec, hit_path);
    vcs_semantic_record_v1_u32(&rec, p->hit_slot);
    vcs_semantic_record_v1_u8(&rec, evidence);
    vcs_semantic_record_v1_u32(&rec, (uint32_t)p->npresent);
    for (size_t k = 0; k < p->npresent; k++)
        vcs_semantic_record_v1_u32(&rec, p->present[k]);
    ok = cm_add(c, VCS_SEMANTIC_SECTION_V1_LOOKUPS, &rec);
    vcs_semantic_record_v1_free(&rec);
    if (ok && c->facts)
        ok = cm_emit_probe(c, includer, p->name, p->form, kind,
                           evidence != VCS_SEMANTIC_MISS_V1_NONE, p->hit_slot,
                           p->present, p->npresent);
    return ok;
}

bool cm_lookup_directive(struct cm_core *c, const struct cm_file *includer,
                         const char *spelled, uint8_t form, uint8_t kind,
                         bool computed, const struct cm_file *hit)
{
    struct cm_probe p = {.form = form, .name = spelled};
    uint8_t evidence = VCS_SEMANTIC_MISS_V1_DERIVED_STAT;
    if (hit == NULL)
        return cm_fail(c, "include of \"%s\" in %s resolved to no read file",
                       spelled, includer->path);
    cm_dir_of(includer->opened, p.includer_dir);
    if (kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT || computed ||
        spelled[0] == '/') {
        p.hit_slot = cm_slot_count(c, p.form);
        evidence = VCS_SEMANTIC_MISS_V1_NONE;
        kind = kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT
                   ? kind : VCS_SEMANTIC_LOOKUP_V1_INCLUDE;
    } else {
        cm_probe_run(c, &p, hit->real);
        if (!p.found) {
            p.npresent = 0;
            evidence = VCS_SEMANTIC_MISS_V1_NONE;
        }
    }
    return cm_emit_lookup(c, includer, &p, kind, hit->path, evidence);
}

/* ---- conditional lookups: text scan + search replay --------------------------- */

/* __has_include and its relatives ask whether a name resolves, and #embed
 * reads a resource, without an include directive the front end reports. The
 * scan finds every one in a repo file. A __has_include whose operand is a
 * literal "x" or <x> is replayed: the producer runs the search itself and
 * stat-confirms every probe (REPLAYED_STAT). Every other spelling - a macro
 * operand, __has_include_next, __has_embed, the GNU __has_include__ words,
 * #embed, or an occurrence a line continuation runs through - is recorded
 * with no negative claim (MISS_V1_NONE), as include_next is: nothing then
 * binds its answer, so a warm session recreates the TU and a facts closure
 * refuses to narrow on it. The scan reads text, not tokens, so a word in a
 * comment or a skipped group counts too: that costs warm reuse, never truth.
 * Contract: docs/work/SEMANTIC_MANIFEST.md, "Warm session". */

/* The longest spelled name an unbound record keeps. */
#define CM_COND_NAME_MAX 200

static bool cm_ident_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

static size_t cm_skip_space(const char *s, size_t i, size_t n)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i;
}

/* A file's text as translation phase 2 leaves it: every backslash-newline
 * (clang also takes spaces between the two) removed. splices holds, strictly
 * increasing, each offset of the spliced text that one was removed before. */
struct cm_spliced {
    const char *s;
    size_t n;
    char *owned;
    size_t *splices;
    size_t nsplices, cap;
};

/* The end of a continuation starting at the backslash s[i], or 0. */
static size_t cm_continuation_end(const char *s, size_t i, size_t n)
{
    size_t j = cm_skip_space(s, i + 1, n);
    if (j < n && s[j] == '\n')
        return j + 1;
    if (j + 1 < n && s[j] == '\r' && s[j + 1] == '\n')
        return j + 2;
    return 0;
}

static bool cm_splice(const char *s, size_t n, struct cm_spliced *t)
{
    size_t w = 0;
    memset(t, 0, sizeof(*t));
    t->s = s;
    t->n = n;
    if (memchr(s, '\\', n) == NULL)
        return true;
    t->owned = zcl_malloc(n + 1, "clang_manifest.spliced");
    if (t->owned == NULL)
        return false;
    for (size_t i = 0; i < n;) {
        size_t end = s[i] == '\\' ? cm_continuation_end(s, i, n) : 0;
        if (end == 0) {
            t->owned[w++] = s[i++];
            continue;
        }
        if (!cm_grow((void **)&t->splices, &t->cap, t->nsplices,
                     sizeof(*t->splices)))
            return false;
        if (t->nsplices == 0 || t->splices[t->nsplices - 1] != w)
            t->splices[t->nsplices++] = w;
        i = end;
    }
    t->owned[w] = '\0';
    t->s = t->owned;
    t->n = w;
    return true;
}

static void cm_spliced_free(struct cm_spliced *t)
{
    free(t->owned);
    free(t->splices);
}

/* Did a continuation run through the occurrence [start, end)? */
static bool cm_spliced_inside(const struct cm_spliced *t, size_t start,
                              size_t end)
{
    for (size_t k = 0; k < t->nsplices; k++) {
        if (t->splices[k] > start && t->splices[k] < end)
            return true;
    }
    return false;
}

/* The quoted or angled operand starting at s[j]; copied to name. *close is
 * the offset of its closing character. */
static bool cm_include_operand(const char *s, size_t j, size_t n,
                               uint8_t *form, char name[PATH_MAX],
                               size_t *close)
{
    size_t end;
    char want;
    if (j >= n || (s[j] != '"' && s[j] != '<'))
        return false;
    want = s[j] == '"' ? '"' : '>';
    *form = s[j] == '"' ? VCS_SEMANTIC_FORM_V1_QUOTED : VCS_SEMANTIC_FORM_V1_ANGLED;
    for (end = j + 1; end < n && s[end] != want && s[end] != '\n'; end++)
        ;
    if (end >= n || s[end] != want || end - j - 1 == 0 || end - j - 1 >= PATH_MAX)
        return false;
    memcpy(name, s + j + 1, end - j - 1);
    name[end - j - 1] = '\0';
    *close = end;
    return true;
}

/* The length of the conditional-lookup word at s[i], else 0. *literal: it is
 * __has_include, the only word the scan replays. */
static size_t cm_cond_word(const char *s, size_t i, size_t n, bool *literal)
{
    static const char *const words[] = {
        "__has_include_next__", "__has_include_next", "__has_include__",
        "__has_include", "__has_embed",
    };
    if (s[i] != '_' || (i > 0 && cm_ident_char(s[i - 1])))
        return 0;
    for (size_t w = 0; w < sizeof(words) / sizeof(words[0]); w++) {
        size_t len = strlen(words[w]);
        if (n - i >= len && memcmp(s + i, words[w], len) == 0 &&
            (i + len == n || !cm_ident_char(s[i + len]))) {
            *literal = strcmp(words[w], "__has_include") == 0;
            return len;
        }
    }
    return 0;
}

/* Skip spaces, tabs and block comments: what may sit between a directive's
 * '#' and its name. */
static size_t cm_skip_directive_space(const char *s, size_t i, size_t n)
{
    for (;;) {
        i = cm_skip_space(s, i, n);
        if (n - i < 2 || s[i] != '/' || s[i + 1] != '*')
            return i;
        for (i += 2; n - i >= 2 && (s[i] != '*' || s[i + 1] != '/'); i++)
            ;
        if (n - i < 2)
            return n;
        i += 2;
    }
}

/* An #embed (or %:embed) directive's name at s[i]: the offset just past
 * "embed", else 0. */
static size_t cm_embed_at(const char *s, size_t i, size_t n)
{
    size_t j;
    if (s[i] == '#')
        j = i + 1;
    else if (s[i] == '%' && n - i >= 2 && s[i + 1] == ':')
        j = i + 2;
    else
        return 0;
    j = cm_skip_directive_space(s, j, n);
    if (n - j < 5 || memcmp(s + j, "embed", 5) != 0 ||
        (j + 5 < n && cm_ident_char(s[j + 5])))
        return 0;
    return j + 5;
}

/* A __has_include(...) with a literal operand, closed on its own spelling:
 * form and name; *end is just past the ')'. */
static bool cm_literal_at(const char *s, size_t i, size_t n, uint8_t *form,
                          char name[PATH_MAX], size_t *end)
{
    size_t j = cm_skip_space(s, i, n), close;
    if (j >= n || s[j] != '(')
        return false;
    if (!cm_include_operand(s, cm_skip_space(s, j + 1, n), n, form, name,
                            &close))
        return false;
    j = cm_skip_space(s, close + 1, n);
    if (j >= n || s[j] != ')')
        return false;
    *end = j + 1;
    return true;
}

/* The form of an unbound occurrence s[i, end): angled when its operand's
 * first quote or angle is '<', else quoted. */
static uint8_t cm_cond_form(const char *s, size_t i, size_t end)
{
    size_t open = i;
    while (open < end && s[open] != '"' && s[open] != '<')
        open++;
    return open < end && s[open] == '<' ? VCS_SEMANTIC_FORM_V1_ANGLED
                                        : VCS_SEMANTIC_FORM_V1_QUOTED;
}

/* An unbound occurrence's spelled name: from s[i] through the first ')' of
 * its line, else to the line's end, trailing blanks dropped, with prefix in
 * front; and its form. */
static void cm_cond_name(const char *prefix, const char *s, size_t i,
                         size_t n, char name[PATH_MAX], uint8_t *form)
{
    size_t end = i;
    while (end < n && s[end] != '\n' && s[end] != ')')
        end++;
    if (end < n && s[end] == ')')
        end++;
    while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                       s[end - 1] == '\r'))
        end--;
    if (end - i > CM_COND_NAME_MAX)
        end = i + CM_COND_NAME_MAX;
    (void)snprintf(name, PATH_MAX, "%s%.*s", prefix, (int)(end - i), s + i);
    *form = cm_cond_form(s, i, end);
}

/* A lookup the scan found and cannot replay: no hit, no negative claim. */
static bool cm_cond_unbound(struct cm_core *c, const struct cm_file *f,
                            const char *name, uint8_t form)
{
    struct cm_probe p = {.form = form, .name = name};
    cm_dir_of(f->opened, p.includer_dir);
    p.hit_slot = cm_slot_count(c, form);
    return cm_emit_lookup(c, f, &p, VCS_SEMANTIC_LOOKUP_V1_HAS_INCLUDE, "",
                          VCS_SEMANTIC_MISS_V1_NONE);
}

static bool cm_has_include_one(struct cm_core *c, const struct cm_file *f,
                               uint8_t form, const char *name)
{
    struct cm_probe p = {.form = form, .name = name};
    char *hit_path = NULL;
    bool ok;
    cm_dir_of(f->opened, p.includer_dir);
    cm_probe_run(c, &p, NULL);
    if (p.found && !cm_norm_path(c, p.found_real, &hit_path))
        return false;
    ok = cm_emit_lookup(c, f, &p, VCS_SEMANTIC_LOOKUP_V1_HAS_INCLUDE,
                        p.found ? hit_path : "",
                        VCS_SEMANTIC_MISS_V1_REPLAYED_STAT);
    free(hit_path);
    return ok;
}

/* The conditional lookup at t[i], if any: replayed or recorded unbound. */
static bool cm_cond_at(struct cm_core *c, const struct cm_file *f,
                       const struct cm_spliced *t, size_t i)
{
    char name[PATH_MAX];
    uint8_t form;
    bool literal = false;
    size_t len = cm_cond_word(t->s, i, t->n, &literal), end;
    if (len != 0 && literal &&
        cm_literal_at(t->s, i + len, t->n, &form, name, &end) &&
        !cm_spliced_inside(t, i, end))
        return cm_has_include_one(c, f, form, name);
    if (len != 0) {
        cm_cond_name("", t->s, i, t->n, name, &form);
        return cm_cond_unbound(c, f, name, form);
    }
    end = cm_embed_at(t->s, i, t->n);
    if (end == 0)
        return true;
    cm_cond_name("#embed ", t->s, cm_skip_space(t->s, end, t->n), t->n, name,
                 &form);
    return cm_cond_unbound(c, f, name, form);
}

bool cm_scan_has_include(struct cm_core *c)
{
    for (size_t k = 0; k < c->nfiles; k++) {
        const struct cm_file *f = &c->files[k];
        struct cm_spliced t;
        bool ok;
        if (f->origin == VCS_SEMANTIC_ORIGIN_V1_SYSTEM)
            continue;
        ok = cm_splice(f->contents, f->size, &t);
        if (!ok)
            (void)cm_fail(c, "cannot splice %s", f->path);
        for (size_t i = 0; ok && i < t.n; i++) {
            char ch = t.s[i];
            if (ch == '_' || ch == '#' || ch == '%')
                ok = cm_cond_at(c, f, &t, i);
        }
        cm_spliced_free(&t);
        if (!ok)
            return false;
    }
    return true;
}
